#include "asioapi.hpp"
#include "coroutinesthings.hpp"
#include "defines.h"
#include "other_coroutinethings.hpp"
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/address_v6.hpp>
#include <async.hpp>
#include <asio.hpp>
#include <coroutine>
#include <iostream>
#include <system_error>
#include <cvkaes.hpp>
#include <cvkrsa.hpp>

//expected to many repeats
#define THROW_EC(ec) if(ec){throw std::runtime_error(ec.message());}
#define THROW_EXP_EC(exp) if(not exp){throw std::runtime_error(exp.error().message());}

cvk::future<Unit> asyncStart(asio::io_context* ctx, cvk::args const& args){
    asio::ip::tcp::socket socket(*ctx);
    asio::ip::tcp::endpoint endpoint(
        asio::ip::make_address_v4(args.server_ip),
        args.port
            );
    auto h = co_await cvk::co_getHandle{};
    std::error_code ec;
    auto callback = [&ec, h](std::error_code const& e, asio::ip::tcp::endpoint const&){
        ec = e;
        RESCHEDULE_HERE(h);
    };
    asio::async_connect(socket, std::array<asio::ip::tcp::endpoint,1>{endpoint},callback);
    co_await std::suspend_always{};
    THROW_EC(ec);

    aig::AesSession aes;
    std::array<uint8_t,260> rsa_buff;
    {// rsa names guard (make another func lol)
    auto res = aig::RsaKey::from_pem_file(args.key_path.c_str(), aig::RsaKeyType::Public);
    if(not res){
        throw std::runtime_error("rsa key creation error: " + res.error());
    }
    aig::RsaKey rsa = std::move(res.value());

    std::array<uint8_t,64>input;
    
    auto key = aig::AesSession::random_bytes<32>();
    auto iv1 = aig::AesSession::random_bytes<16>();
    auto iv2 = aig::AesSession::random_bytes<16>();

    std::memcpy(input.data(),key.data(),32);
    std::memcpy(input.data()+32,iv2.data(),16);
    std::memcpy(input.data()+48,iv1.data(),16);

    constexpr const char* salt = "Chara_VerKys";
    constexpr uint8_t len = 12;
    for(uint8_t i = 0; i < len; ++i){
        key[i+len] ^= salt[i];
    }

    //and add salt same as server
    auto aes_res = aig::AesSession::create(key, iv1, iv2);

    THROW_EXP_EC(aes_res);
    aes = std::move(*aes_res);

    auto used = rsa.encrypt(input, 
            std::span<uint8_t>(rsa_buff.data()+4, 256)
            );
    if(not used){
        throw std::runtime_error("rsa encrypt error: " + used.error());
    }
    cussert(used.value() == 256);
    }//rsa names guard

    uint32_t size_rsa = 4;
    std::memcpy(rsa_buff.data(),&size_rsa,4);

    auto res = co_await cvk_asio::send(socket, rsa_buff);
    THROW_EXP_EC(res);

    auto open = cvk_asio::reliable_is_open(socket);
    THROW_EXP_EC(open);
    if(not *open){throw std::runtime_error("socket was closed during key exchange");}

    //possible that something live on buffer, try to read error
    if(socket.available() >0){
        // i mean, server can send anything now only if it is error
        co_await read_block_size(socket, aes, false);
        cuabort("unexpected valid read");
    }

    std::vector<uint8_t> login_buf_raw;
    login_buf_raw.resize(4+args.login.size());
    std::memcpy(login_buf_raw.data()+4,args.login.data(),args.login.size());

    std::vector<uint8_t> login_buf_crypted;
    login_buf_crypted.resize(4+args.login.size());
    ec = aes.encrypt(login_buf_raw,login_buf_crypted);
    THROW_EC(ec);

    res = co_await cvk_asio::send(socket, login_buf_crypted);
    THROW_EXP_EC(res);

    res = co_await cvk_asio::send(socket, login_buf_crypted);
    THROW_EXP_EC(res);

    uint32_t client_ip_read = co_await read_block_size(socket, aes);

    if(client_ip_read not_eq 4){
        throw std::runtime_error("expected ip (so size should be 4), but size is: " + std::to_string(client_ip_read));
    }

    //lol i can use this here, cuz it is 4 bytes anyway
    //uint32_t client_ip_read = co_await read_block_size(socket, aes);

    std::array<uint8_t,4> ip_bytes_crypted;
    res = co_await cvk_asio::read_some(socket, ip_bytes_crypted);

    //server just use to uint, 
    uint32_t ip_bytes_raw;
    ec = aes.decrypt(
            std::span<const uint8_t>{(uint8_t*)&ip_bytes_crypted,4},
            std::span<uint8_t>{      (uint8_t*)&ip_bytes_raw    ,4}
            );
    THROW_EC(ec);
    
    setupTun(asio::ip::make_address_v4(ip_bytes_raw)); //sync

    co_return {};
}

cvk::future<uint32_t> read_block_size(asio::ip::tcp::socket& socket, aig::AesSession& aes, bool encryption_used){
    uint32_t size_crypted;
    auto suc = co_await cvk_asio::read_some(socket,std::span<uint8_t>{(uint8_t*)&size_crypted,4});
    // nah i should change read some to reliable variant by default
    //  now better
    THROW_EXP_EC(suc);

    uint32_t size;
    if(not encryption_used){
        size = size_crypted;
    }else{
        auto ec = aes.decrypt(
                std::span<const uint8_t>{(uint8_t*)&size_crypted, 4},
                std::span<uint8_t>{      (uint8_t*)&size,         4}
                );
        THROW_EC(ec);
    }


    //since i send (on server) one buffer, there no way error can differ between packets
    //(i rely on it)
    //(anyway in worst case i only lose error msg)
    //so i can use blocking peek to check is it error or just encrypted msg

    //if(size_crypted == std::numeric_limits<uint32_t>::max() ){ //want to use break as exit path for this block
    while(size_crypted == std::numeric_limits<uint32_t>::max() ){
        //read until error 
        const char* _Error = "_Error: ";
        std::string _error;
        _error.resize(std::strlen(_Error));
        asio::error_code ec;
        //may block but should be fine in 99.99...% cases
        socket.receive(asio::buffer(_error),asio::socket_base::message_peek,ec);
        THROW_EC(ec); //so it is blocking, so may be some sort of different error, idk, should be fine

        if(_error not_eq _Error){
            break; //it was just funny -1 due to encryption, it is not error msg
        }

        std::array<uint8_t,256> buff; // no way i may want more than 256 for error
        auto read = co_await cvk_asio::read_some_unreliable(socket, buff);
        THROW_EXP_EC(read);
        write_clnt() << std::string(buff.data(), buff.data()+*read);
        std::cerr << std::string(buff.data(), buff.data()+*read);
        read = co_await cvk_asio::read_some_unreliable(socket, buff);
        if(read){
            throw std::runtime_error("expected ec, but got read");
        }
        if(read.error() not_eq asio::error::eof){
            throw std::runtime_error("expected normal ec == eof, but got: " + read.error().message());
        }
        throw std::runtime_error("error from server" + std::string((const char*)buff.data())); //error from serv is null terminated
                                                                                               // (it is just cstring constructor)
    }
    co_return size;
}

void setupTun(asio::ip::address_v4 const&){

}

