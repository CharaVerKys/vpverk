#include "asioapi.hpp"
#include "coroutinesthings.hpp"
#include "defines.h"
#include "other_coroutinethings.hpp"
#include "some_other_help_funcs.hpp"
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
#include <fstream>

//there to many repeats
#define THROW_EC(ec) if(ec){throw std::runtime_error(ec.message());}
#define THROW_EXP_EC(exp) if(not exp){throw std::runtime_error(exp.error().message());}


cvk::future<Unit> startAsync(asio::io_context* ctx, cvk::args const& args){
    asio::ip::tcp::socket socket(*ctx);
    socket.non_blocking(true);
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
    
    std::ofstream ip_file(cvk::get_current_exec_path().parent_path() / "tun_ip.raw",
            std::ios::binary | std::ios::out); // no append, recreate
    ip_file << ip_bytes_raw;
    ip_file.close();

    setupTun(asio::ip::make_address_v4(ip_bytes_raw)); //sync

    std::shared_ptr<asio::ip::tcp::socket> s_socket = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
    std::shared_ptr<aig::AesSession> s_aes = std::make_shared<aig::AesSession>(std::move(aes));
    std::stop_source stop_source_;
    startTunRead(s_socket, s_aes, stop_source_.get_token());
    startSocketRead(s_socket, s_aes, stop_source_.get_token());
    //btw, will write down here
    //tun support async read and write, both require lock, when write invokes,
    // it preempt, not locks, so it is safe for single thread // it also used on server without threading anyway

    //only valid exit path is ctrl-c
    //may change later, but than think about canceling this signal handle

                    // is source behave like ptr? idk...
    auto signals_handle = [stop_source_ = std::move(stop_source_), s_socket]
        (std::error_code const& ec, [[maybe_unused]] int signal){
            //not even cancel, if any ec
            std::cout << "try to exit..." <<std::endl;
        if(ec){cuabort(("signal handler: " + ec.message()).c_str());}
        stop_source_.request_stop();
        s_socket->cancel();
        //Tun::instance().cancel(); //if it write that stuck, well im fucked \ on read it just timer timeout
    };

    asio::signal_set signals(**t_ctx, SIGINT, SIGTERM); //not remember what is sigterm / sigint = ctrl-c
    signals.async_wait(signals_handle);


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
        auto handle = co_await cvk::co_getHandle();
        socket.async_receive(asio::buffer(_error),asio::socket_base::message_peek,[handle, &ec]
                (std::error_code const&e, uint64_t read){
                    ec = e;
                    (void)read; //if not size match, and it is actually an error, than something completely broken
                    handle.resume();
                });
        THROW_EC(ec);

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

void system_(const std::string& c) {
    if (std::system(c.c_str()) != 0)
        cuabort(("command failed: " + c).c_str());
};

void setupTun(asio::ip::address_v4 const& vpn_client_ip){
    //call constructor (side effect basically)
    (void)Tun::instance();

    // 1. bring tun up (REQUIRED — you're missing this)
    system_("ip link set tun0 up");

    // 2. assign address
    system_("ip addr add " + vpn_client_ip.to_string() + "/16 dev tun0");

    // 3. pin real server via original gateway so it doesn't go into the tunnel
    std::cout << "automatic route to server setup is not implemented\n"
        << "you should call something like 'ip route add <server_ip>/32 via <gateway> dev <inf>'\n";

    // 4. redirect all traffic into tunnel (two halves avoid replacing the default route)
    system_("ip route add 0.0.0.0/1 via " + vpn_client_ip.to_string() + " dev tun0");
    system_("ip route add 128.0.0.0/1 via " + vpn_client_ip.to_string() + " dev tun0");
}


cvk::coroutine_t startTunRead(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<aig::AesSession> aes, std::stop_token stop){
    constexpr uint16_t max_possible_ip_packet_size = 1500;
    std::array<uint8_t,max_possible_ip_packet_size+4> buffer_raw;
    std::array<uint8_t,max_possible_ip_packet_size+4> buffer_crypt;

    try{
    while(not stop.stop_requested()){
        uint32_t read_size = co_await Tun::instance().read(
                std::span<uint8_t>{buffer_raw.data()+4, max_possible_ip_packet_size}, 
                std::chrono::milliseconds{100});
        if(read_size == 0){continue;}
        std::memcpy(buffer_raw.data(), &read_size, 4);
        auto ec = aes->encrypt(std::span<const uint8_t> {buffer_raw.data(),read_size+4}, buffer_crypt);
        THROW_EC(ec)
        auto res = co_await cvk_asio::send(*socket, std::span<const uint8_t>{buffer_crypt.data(),read_size+4})   ;
        if(not res and res.error() == std::errc::operation_canceled){continue;}
        THROW_EXP_EC(res)
        // it is literally all????
    }
    }catch(std::exception const& e){
        std::cerr << __func__ << '\n';
        cuabort(e.what());
    }
    write_clnt() << "exiting...";
}
cvk::coroutine_t startSocketRead(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<aig::AesSession> aes, std::stop_token stop){
    // will implement this first cuz it seems easier
    constexpr uint16_t max_possible_ip_packet_size = 1500;
    std::array<uint8_t,max_possible_ip_packet_size> buffer_crypt;
    std::array<uint8_t,max_possible_ip_packet_size> buffer_raw;

    try{
    while(not stop.stop_requested()){
        uint32_t read_size = co_await read_block_size(*socket, *aes);
        //if(read_size > max_possible_ip_packet_size){cuabort("read_size > max_possible_ip_packet_size");}
        cussert(read_size > max_possible_ip_packet_size);
        auto res = co_await cvk_asio::read_some(*socket, buffer_crypt, read_size);
        if(not res and res.error() == std::errc::operation_canceled){continue;}
        THROW_EXP_EC(res)
        auto ec = aes->decrypt(std::span<const uint8_t> {buffer_crypt.data(),read_size}, buffer_raw);
        THROW_EC(ec)
        co_await Tun::instance().write(std::span<const uint8_t>{buffer_raw.data(),read_size});
        //wait, is it all???? is it really everything i need?????
    }
    }catch(std::exception const& e){
        std::cerr << __func__ << '\n';
        cuabort(e.what());
    }
    write_clnt() << "exiting...";
}
