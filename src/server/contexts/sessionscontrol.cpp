#include "sessionscontrol.hpp"
#include "connection.hpp"
#include "coroutinesthings.hpp"
#include "defines.h"
#include "server_method_config.hpp"
#include "threadchecking.hpp"
#include "types/settingssnapshot.hpp"
#include <stdexcept>
#include <tun.hpp>
#include <algorithm>
#include <asio/ip/address.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/strand.hpp>
#include <coroutine>
#include <limits>
#include <other_coroutinethings.hpp>
#include <queue>


cvk::expected_contextsReg SessionsControl::onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs){
    regMethod(this,&SessionsControl::move_to_main_context);

    assert(previousFuncs.size() == MainCtx); // ? check that order is correct
    std::function<void(std::stop_token)> func = std::bind_front(&SessionsControl::asyncStart, this);
    previousFuncs.push_back(std::move(func)); // ! push back only
    return previousFuncs;
}

tl::expected<Unit,std::exception_ptr> SessionsControl::startAcceptingConnections(Unit&&){
    // start acceptor and run as infinite loop
    acceptConnection(); // self running coroutine
    return tl::unexpected{std::make_exception_ptr(std::logic_error("you shouldnt use this expected (it just chain end)"))}; // change to return Unit{} if you want to continue chain
}

cvk::future<Unit> SessionsControl::routeTunToClients(bool use_loop){
    cussert(tunRouter.empty());
    try{ 
        while(not stop_token.stop_requested()){
            constexpr uint16_t max_possible_size_on_tun = 1500; // max possible ip packet
                                                                // as i understand i getting whole ip packet on tun
            std::array<uint8_t,max_possible_size_on_tun+sizeof(Connection::block_size_t)> buffer; //assumed data from tune will be written on buffer.data()+sizeof(block_size)
            std::array<uint8_t,max_possible_size_on_tun+sizeof(Connection::block_size_t)> buffer_encrypted;
            // open tun, at first there no any connection, it will lock forever if read without timeout
            // so i do read with timeout for ~~1 sec~~ 100ms
            // ? how do i handle when there some garbage on tun? i guess if there nothing in queue == garbage

            // get next packet for tun with read timeout
            bool by_timeout = false; //if by timeout just process queue and continue loop
            //uint32_t ip = 0; // read from packet(buffer)
            Connection::block_size_t got_packet_size = 0; //size of ip packet to send

            got_packet_size = co_await Tun::instance().read(
                    std::span<uint8_t>{buffer.data()+sizeof(Connection::block_size_t),max_possible_size_on_tun}
                    , std::chrono::milliseconds{50});


            by_timeout = got_packet_size == 0;

            cussert(got_packet_size <= max_possible_size_on_tun);

            emptyQueue();
            if(by_timeout){continue;}
            
            // ai offered better interface (better than manual swap bytes for order)
            asio::ip::address_v4::bytes_type dst_bytes;
            std::memcpy(dst_bytes.data()
                    , buffer.data() +16 +sizeof(Connection::block_size_t)
                    , 4);
            auto dst = asio::ip::address_v4(dst_bytes); // bytes_type constructor = network order


            uint32_t a;
            std::memcpy(&a
                    , buffer.data() +16 +sizeof(Connection::block_size_t)
                    , 4);

            unsigned char first ;
            unsigned char second;
            std::memcpy(&first ,(reinterpret_cast<unsigned char*>(&a)+0),1);
            std::memcpy(&second,(reinterpret_cast<unsigned char*>(&a)+1),1);
            const char* res = "not like this2";
            if(first == 10 and second == 123){res = "like this2";}
            cuabort(res);

            std::memcpy(buffer.data(),&got_packet_size,sizeof(got_packet_size));
            
            // all possible active connections are in router now
            auto it = tunRouter.find(dst);
            if(it == tunRouter.end()){
                continue;
            }


            // no i shouldnt, i shouldnt change anything at all
            // and also getting remote ip is completely wrong here
            // replaced everything with sending to client ip they should set
            //
            // // wait, i am lost, i have to change destination ip to original, right?
            // auto ip = con->ip().value_or({});//if cant get ip, than probably connection was reset, will fail later before packet send anyway
            // asio::ip::address_v4::bytes_type restore_bytes = ip.to_bytes(); //really much easier than manually swap order
            // std::memcpy(buffer.data() +16 +sizeof(Connection::block_size_t)
            //         , restore_bytes.data()
            //         , 4);
            // static_assert(false, "check");


            std::shared_ptr<Connection> con = it->second.first;
            std::shared_ptr<aig::AesSession> aes = it->second.second;

            { //// lock lifetime
                auto lock = con->write_lock();
                while(not lock){
                    //co_await resched{}; //hmmmmm, maybe instead perform next read on tun?
                    co_await routeTunToClients(false); // do not repeat in loop
                                                //btw ip packets (if same connection) order is broken here, they are broken anyway so i not care
                                                //network stack tcp implementation will fix it anyway
                                                
                    // child coroutine completed, dead lock is impossible
                    lock = con->write_lock();
                }
                std::span<uint8_t> packet_with_bounds_encrypted{buffer_encrypted.data(),got_packet_size+sizeof(Connection::block_size_t)};
                {/// name bounds
                std::span<const uint8_t> packet_with_bounds    {buffer          .data(),got_packet_size+sizeof(Connection::block_size_t)};
                auto ec = aes->encrypt(packet_with_bounds, packet_with_bounds_encrypted);
                if(ec){
                    write_serv() << "failed to encrypt (from tun): " << ec.message();
                    // have to release lock first before close
                    lock.reset();
                    co_await con->close("server failed to encrypt"); // in most cases should be same fail on send
                    //tunRouter.erase(it); //this client is gone // but that was error to erase here, it should be only in connection coroutine lifetime -> emptyQueue (it may cause issues with requeue algo)
                    continue; 
                }//if error code
                }/// name bounds

                auto expected = co_await con->send(packet_with_bounds_encrypted);
                // we under lock, so coroutine currently in main context, can just close
                if(not expected){
                    write_serv() << "failed to send to client (from tun): " << expected.error().message();
                    // have to release lock first before close
                    lock.reset();
                    co_await con->close("server failed to send"); // in most cases should be same fail on send
                    //tunRouter.erase(it); //this client is gone // but that was error to erase here, it should be only in connection coroutine lifetime -> emptyQueue (it may cause issues with requeue algo)
                    continue; 
                }
                // fine, packet send, work done
            } // unlock

            if(not use_loop){ //until root coroutine that performed first lock-fail rerun
                              // todo, ideally check(verify) 'try next' flow again from scratch
                co_return {}; //wake up prev
            }
        }//while true
    } catch (std::exception const& e) {
        write_serv() << cll::error << "error in tun read: " << e.what();
        routeTunToClients(true).subscribe([](tl::expected<Unit,std::exception_ptr>&&exp){
                    if(not exp){
                        cuabort("unhandled exception in routeTunToClients (like where it even possible lol)");
                    }
                },**t_ctx);
    }
    write_serv() << "stop tun read via loop";
    co_return {};
}

void SessionsControl::emptyQueue(){
    std::queue<aq_route*> local_handle;
    while(not tunQueue.was_empty()){
        aq_route* ptr;
        if(not tunQueue.try_pop(ptr)){ //!! got
            continue;
        }

        cussert(ptr not_eq nullptr);
        if(ptr->add_or_remove){
            //new register
            if(tunRouter.contains(ptr->addr)){
                //wasnt deleted yet, requeue
                local_handle.push(ptr); //!! give
                continue;
            }

            auto value = std::make_pair(ptr->con, ptr->aes);
            tunRouter.emplace(ptr->addr, value);
        }else{
            //delete old
            auto it = tunRouter.find(ptr->addr);
            if(it == tunRouter.end()){
                //wasnt registered yet, requeue
                local_handle.push(ptr); //!! give
                continue;
            }
            
            tunRouter.erase(it);
        }
        delete ptr; //!! free 
    }
    if(local_handle.empty()){return;}
    // if size still the same, it means only that there actual error
    size_t prev_size = local_handle.size();
    size_t size = -1;
    do{
        if(prev_size == size){
            write_serv() << cll::critical << "some sort of error in queue processing";
            while(local_handle.size()){
                auto e = local_handle.front();
                local_handle.pop();
                write_serv() << cll::debug << "ip: " << e->addr.to_string()
                    << " \\ want: " << (e->add_or_remove ? "add" : "remove");
                delete e;
            }
            return;
        }
        //ideally it should be one iteration, but i repeat just in case (lock free after all idk)

        prev_size = size;
        size_t size_ = local_handle.size();
        for(uint i = 0; i < size_; ++i){
            auto ptr_ = local_handle.front();
            local_handle.pop();

            if(ptr_->add_or_remove){
                //new register
                if(tunRouter.contains(ptr_->addr)){
                    //wasnt deleted yet, requeue
                    local_handle.push(ptr_); //!! give
                    continue;
                }
                auto value = std::make_pair(ptr_->con, ptr_->aes);
                tunRouter.emplace(ptr_->addr, value);
            }else{
                //delete old
                auto it = tunRouter.find(ptr_->addr);
                if(it == tunRouter.end()){
                    //wasnt registered yet, requeue
                    local_handle.push(ptr_); //!! give
                    continue;
                }
                tunRouter.erase(it);
            }
            delete ptr_;
        }


        size = local_handle.size();
    }while(size);
}

asio::ip::address_v4 SessionsControl::getNewAddress(){
    auto lock = std::lock_guard{unique_address_mutex};
    asio::ip::address_v4 address;
    do{
        ++unique_address_counter;
        if(unique_address_counter == std::numeric_limits<uint16_t>::max()){
            unique_address_counter=1;
        }
        uint32_t a = address_begin | uint32_t(unique_address_counter);
    
        unsigned char first ;
        unsigned char second;
        std::memcpy(&first ,(reinterpret_cast<unsigned char*>(&a)+3),1);
        std::memcpy(&second,(reinterpret_cast<unsigned char*>(&a)+2),1);
        const char* res = "not like this";
        if(first == 10 and second == 123){res = "like this";}
        cuabort(res);

        address = asio::ip::make_address_v4(a); 
    }while(unique_address_set.contains(address));
    unique_address_set.insert(address);
    return address;
}

cvk::coroutine_t SessionsControl::acceptConnection(){
    try {
        if(not acceptor_){
            // here because i want settings context to be valid and running

            const SettingsSnapshot settings = co_await cvk::method::Invoke<SettingsSnapshot>{
                // ? from context to context
                // ? first is where coroutine will resume, second where func will calls
                MainCtx, SettingsCtx,
                method::get_settings
            };

            acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
                loop_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), settings.getPort()));
        }

        while(not stop_token.stop_requested()){
            // not want to create awaiter for this
            auto handle = co_await cvk::co_getHandle{};
            //asio::ip::tcp::socket socket{t_ctx};
            // using api, i made it so why not
            std::error_code ec;
            asio::strand<asio::io_context::executor_type> strand  
            //auto strand /// idk why clangd cant detect that make_stand return and io_context (no ::executor_type) not match
                = asio::make_strand(
                *cvk::ContStore::instance()->get_io_context(MainCtx)
            );
            
            asio::ip::tcp::socket socket{strand};
            acceptor_->async_accept(socket, [handle, &ec](std::error_code const& error){
                    ec = error;
                    RESCHEDULE_HERE(handle); //reschedule on same thread macros
                    }
                  );
            co_await std::suspend_always{};

            if(ec == std::errc::operation_canceled and stop_token.stop_requested()){
                write_serv() << "stop accepting via cancel on acceptor";
                co_return;
            }
            if(ec){
                throw std::runtime_error(std::string("asio <error_code> ")+ec.message());
            }

            std::shared_ptr<Connection> connection = std::make_shared<Connection>(std::move(strand),std::move(socket));
            connections_.push_back(connection);
            
            //detached coroutine
            startConnection(connection);
            
            // done, just process next

            // against dos - on overflow connections_ vector
            // this will just delete connections from vector;
            if(connections_.size() > 1<<13){closeOldSessions();}
        }
        
    } catch (std::exception const& e) {
        write_serv() << cll::error << "error in acceptor: " << e.what();
        acceptor_ = nullptr;
        acceptConnection();
    }
    write_serv() << "stop accepting via loop";
}

cvk::coroutine_t SessionsControl::closeOldSessions()
{
    // exception here is crash
    // thread safe funcs for Connection only
    // acknowledge possible out of bounds after reschedule
    try{
    size_t i = 0;

    const SettingsSnapshot settings = co_await cvk::method::Invoke<SettingsSnapshot>{
        // ? from context to context
        // ? first is where coroutine will resume, second where func will calls
        MainCtx, SettingsCtx,
        method::get_settings
    };
    const std::chrono::seconds max_alive_time = settings.getMaxAliveTime();

    while (i < connections_.size()) {
        std::shared_ptr<Connection> conn = connections_[i];
        const auto opt = conn->is_active(); 
        if (not opt) {
            ++i;
            continue;
        }

        if(not *opt){
            connections_[i] = std::move(connections_.back());
            connections_.pop_back();
            co_await conn->close("not active (should be no-op)");
            if (i > connections_.size())
                i = connections_.size();
            continue;
        }

        if (conn->alive_time() >= max_alive_time) {
            connections_[i] = std::move(connections_.back());
            connections_.pop_back();
            co_await conn->close("session expired");
            if (i > connections_.size())
                i = connections_.size();
        } else {
            ++i;
        }
    }
    }catch(std::exception const& e){cuabort((std::string("exception in close old: ") + e.what()).c_str());}
}

cvk::coroutine_t SessionsControl::startConnection(std::shared_ptr<Connection> connection) //detached from accept connection, perform current socket lifetime logic
{
    std::optional<asio::ip::address_v4> assigned_address;
    try{
        cussert(connection and connection->is_active());
        // no pending bounds cuz it is literally first operation, with reasonable session max life time impossible to trigger close here
        const SettingsSnapshot settings = co_await cvk::method::Invoke<SettingsSnapshot>{
            // ? from context to context
            // ? first is where coroutine will resume, second where func will calls
            MainCtx, SettingsCtx,
            method::get_settings
        };
        aig::AesSession aeskey = co_await exchangeAESkey(*connection, settings);
        bool allowed = co_await authenticateUser(*connection, aeskey, settings);
        if(not allowed){
            co_await connection->close("Access denied");
            co_return;
        }

        // i not sure, but seems that i may access aes session when this coroutine is destroyed
        // so just in case make it ptr lifetime
        std::shared_ptr<aig::AesSession> aes_ = std::make_shared<aig::AesSession>(std::move(aeskey));
        // accessing aeskey no longer allowed, it was moved

        assigned_address = getNewAddress();
        co_await sendIp(*connection, *aes_, assigned_address.value().to_uint());


        //
        // be safe with lifetimes below
        //
        // i wonder... how to make it thread safe... i guess i have to use atomic queue that make actual changes of tunRouter in tun fd loop
        //tunRouter
        aq_route* route = new aq_route;
    #ifndef NDEBUG
        void* ptr_addr = (void*)route;
    #endif
        route->add_or_remove = true;
        route->addr = assigned_address.value();
        route->con = connection;
        route->aes = aes_;
                        // there taken rvalue, but seems like it just take reference and its all
                        // idk, seems like ptr is valid anyway
        if(not tunQueue.try_push(route)){
            cussert_d((void*)route == ptr_addr);
            constexpr const char* msg = "failed to add pointer to atomic queue";
            write_serv() << cll::critical << msg;

            delete route;

            co_await connection->close("server error: queue full");
            throw /*duplicate*/ std::runtime_error(msg);
        }
        cussert_d((void*)route == ptr_addr);
        //
        // bellow this point no return statements allowed
        //

        bool normal_disconnect = co_await performDataExchange(*connection, *aes_);



        if(not normal_disconnect){
            // idk, noop yet, dont even know what to log, later will be statistics flush i guess
        }else{
            //normal
            //
        }
    }catch(std::exception const& e){
        // maybe make special exception for closed case (operation aborted/canceled) idk yet
        write_serv() << cll::error << "exception in startConnection (lifecycle): " << e.what();
    }
    
    // all pathes above, after register call, should lead to unregister call

    if(not assigned_address){co_return;} //for unhandled exception above registration
                                        
    aq_route* route = new aq_route;
    route->add_or_remove = false;
    route->addr = assigned_address.value();
    route->con = connection; //redundant, i just want to delete from map by key
    //route->aes = aes_;
    while(not tunQueue.try_push(route)){
        co_await reschedule{}; //unlike init, there i really want to process it
                               //actually maybe init also should be reschedule
                               //but if there some sort of error id rather see log that some one cant init
                               //instead of some memory leaks here
    }
    // all pathes should lead here
    // so i do it here
    try{
        auto lock = std::lock_guard{unique_address_mutex};
        unique_address_set.erase(*assigned_address);
    }catch(std::exception const& e){
        write_serv() << cll::error << "some weird error (wa;eljnal): " << e.what() << " // " << assigned_address->to_string();
    }
}
cvk::future<aig::AesSession> SessionsControl::exchangeAESkey(Connection& con, SettingsSnapshot const& settings/*maybe only private key*/) // exchange via RSA
{
    constexpr uint16_t expected_first_msg_size = 256;// rsa first packet
    Connection::block_size_t read;
    //auto buf = Connection::block_get_buffer(read);
    (co_await con.read_some_reliable(Connection::block_get_buffer(read)))
        .or_else([](std::error_code&&ec){throw std::runtime_error(
                    "error read on key exchange: " + ec.message());});
    if(expected_first_msg_size not_eq read){
        co_await con.close("Protocol violation");
        throw std::runtime_error("wrong first msg size: " + std::to_string(read));
    }

    std::array<uint8_t,expected_first_msg_size> aes_key_under_rsa;
    (co_await con.read_some_reliable(aes_key_under_rsa))
        .or_else([](std::error_code&&ec){throw std::runtime_error(
                    "error read on key exchange: " + ec.message());});

    std::array<uint8_t,32+16+16> aes_key_raw;
    
    //perform rsa decrypt, will learn later
    cuabort("no code");

    // salt just against some one who not have source code, useless op in general
    constexpr const char* salt = "Chara_VerKys";
    constexpr uint8_t len = 12;
    for(uint8_t i = 0; i < len; ++i){
        aes_key_raw[i+len] ^= salt[i];
    }

    std::span<const uint8_t> span = aes_key_raw;

    auto exp = aig::AesSession::create(
                span.subspan(0,32),                
                span.subspan(32,16),                
                span.subspan(32+16,16)
            );
        // .or_else([&con](std::error_code&&ec){
        //         //con.close("something gone wrong with aes");
        //         co_await con.close("Protocol violation");
        //         throw std::runtime_error(
        //             "error creating aes session: " + ec.message());})
        // .value();
        if(not exp){
            co_await con.close("Protocol violation");
            throw std::runtime_error("error creating aes session: " + exp.error().message());
        }
    co_return std::move(exp.value());
}

cvk::future<bool/*allowed*/> SessionsControl::authenticateUser(Connection& con, aig::AesSession& aeskey, SettingsSnapshot const& settings/*maybe only logins*/) // verify user in allowed list
{
    //todo move to some sort of helper function for whole packet, later (duplicate in perform)
    Connection::block_size_t read_raw;
    {///
    Connection::block_size_t read_crypt;
    //auto buf = Connection::block_get_buffer(read);
    (co_await con.read_some_reliable(Connection::block_get_buffer(read_crypt)))
        .or_else([](std::error_code&&ec){throw std::runtime_error(
                    "error read on authentication: " + ec.message());});
    auto ec = aeskey.decrypt(Connection::block_get_buffer(read_crypt),Connection::block_get_buffer(read_raw));
    if(ec){
        co_await con.close("Decryption failure");
        throw std::runtime_error("first decrypt fail: " + ec.message());
    }
    }///
    if(read_raw > 200){
        throw std::runtime_error("they on trees jonny");
    }

    std::vector<uint8_t> login_raw;
    {///
    std::vector<uint8_t> login_crypt;
    login_crypt.resize(read_raw);
    login_raw.resize(read_raw);
    (co_await con.read_some_reliable(login_crypt))
        .or_else([](std::error_code&&ec){throw std::runtime_error(
                    "error read on authentication (2): " + ec.message());});
    auto ec = aeskey.decrypt(login_crypt,login_raw);
    if(ec){
        co_await con.close("Decryption failure");
        throw std::runtime_error("first decrypt fail (2): " + ec.message());
    }
    }///

    co_return std::ranges::binary_search(
            settings.getBinarySorted_allowedLogins(),
            std::string{login_raw.begin(), login_raw.end()}
            );
}

// kill meeeee by this shit code idk why i keep writing it instead of recreating everything entirely
// by its roots, with acknowledge of asio absolute inability to thread and only one file descriptor - tun, not several as i planed initially
// i really made such bad code now

cvk::future<Unit> SessionsControl::sendIp(Connection& con, aig::AesSession& aeskey, uint32_t ip)
{
    std::array<uint8_t,8> buffer;
    //uint32_t u4 = 4;
    buffer[0] = 4;
    buffer[1] = buffer[2] = buffer[3] = 0; // nah mem copy, i can reconstruct by bytes itself

    // but not ip lol
    std::memcpy(buffer.data()+4,&ip,4);

    std::array<uint8_t,8> buffer_2;
    auto ec = aeskey.encrypt(buffer, buffer_2);
    if(ec){
        co_await con.close("Encryption failure");
        throw std::runtime_error("first encrypt fail: " + ec.message());
    }
    // no lock cuz still in init process
    auto expected = co_await con.send(buffer_2);
    if(not expected){
        co_await con.close("Send failure ыыыыыыыыыыыыы");
        throw std::runtime_error("first send fail: " + expected.error().message());
    }
    co_return {};
}

cvk::future<bool/*normal disconnect == true*/> SessionsControl::performDataExchange(Connection& con, aig::AesSession& aeskey) // just vpn logic, when coroutine return connection ended, no other logic
{
    //since tun return ip packets, and default for them is 1500, i limit to 1500
    constexpr uint16_t max_bounds_size = 1500;

    std::array<uint8_t, max_bounds_size> buffer_crypt; //reusable
    std::array<uint8_t, max_bounds_size> buffer_raw; //reusable

    //while(not stop_token.stop_requested()){
    while(true){
        Connection::block_size_t read_raw; // needed outside lock lifetime
                                           
        //here i perform only read, send in tun routing logic
        auto ec = co_await con.wait_read();
        if(ec){
            if(ec not_eq std::errc::connection_reset){
                write_serv() << ec.message();
            }
            co_return false;
        }
        { //// lock lifetime
            auto lock = con.read_lock();
            while(not lock){
                co_await resched{};
                lock = con.read_lock();
            }
            if(con.available() == 0 and con.is_active_nolock()){
                continue; //some one used socket between wait read and read start
            }

            {///
            Connection::block_size_t read_crypt;
            //auto buf = Connection::block_get_buffer(read);
            (co_await con.read_some_reliable(Connection::block_get_buffer(read_crypt)))
                .or_else([](std::error_code&&ec){throw std::runtime_error(
                            "error read on data exchange: " + ec.message());});
            auto ec = aeskey.decrypt(Connection::block_get_buffer(read_crypt),Connection::block_get_buffer(read_raw));
            if(ec){
                co_await con.close("Decryption failure");
                throw std::runtime_error("first decrypt fail: " + ec.message());
            }
            }///
            if(read_raw > max_bounds_size){
                throw std::runtime_error("they on trees jonny");
            }

            {///
            (co_await con.read_some_reliable(buffer_crypt,read_raw))
                .or_else([](std::error_code&&ec){throw std::runtime_error(
                            "error read on data exchange (2): " + ec.message());});
            auto ec = aeskey.decrypt(
                    std::span{buffer_crypt.data(), read_raw},
                    std::span{buffer_raw.data(), read_raw}
                );
            if(ec){
                co_await con.close("Decryption failure");
                throw std::runtime_error("first decrypt fail (2): " + ec.message());
            }
            }///
        }//unlock
        
        // // here some sort of write on tun file descriptor, idk how to do now
        // // should have some sort of lock, multiple threads want to write to tun
        // cuabort("no code 2");

        // nah, i should change crc than, it should be done, replaced whole logic here with setting up client ip during handshake
        // // replace source
        // uint8_t* ip = buffer_raw.data()+12;// hardcoded: souce address in packet
        // std::memcpy(ip,vpn_ip.to_bytes().data(),4);


        std::span<const uint8_t> ip_packet{buffer_raw.data(),read_raw};

        co_await Tun::instance().write(ip_packet);
        // on exception will propagate above (shouldnt be any exceptions btw)

    }//while true
}

void SessionsControl::asyncStart(std::stop_token token){
    stop_token = token;

    checkThreadIsNOT(&mainThreadID);
    t_ctx = &this->loop_;

    //add thread pool here
    // and init context for them all to use coroutines!

    cleanup_timer_ = std::make_unique<asio::steady_timer>(loop_);

    // ready for macros code, if i will ever want // it is qt like timer, that run forever
    static std::optional<std::function<void()>> lambda_self_ = std::nullopt; //should be safe actually, even if some static duration object call timer
                                                                             //ideally just dont start timer from static lifetime objects
                                                                             //todo maybe find better way to self-contain callback
    auto cleanup_timer_call = [this](){
        cleanup_timer_->expires_after(std::chrono::seconds(10));
        cleanup_timer_->async_wait([this](std::error_code ec) {
            if (ec == std::errc::operation_canceled) return; // timer cancelled = server shutting down
            (void)this;
            closeOldSessions();
            (*lambda_self_)();
        });
    };
    if(not lambda_self_){lambda_self_ = cleanup_timer_call;}
    cleanup_timer_call();

    routeTunToClients(true).subscribe([](tl::expected<Unit,std::exception_ptr>&&exp){
                if(not exp){
                    cuabort("unhandled exception in routeTunToClients (like where it even possible lol)");
                }
            },**t_ctx);

    write_serv() << "started sessions control";
}

void SessionsControl::move_to_main_context(std::any promise_bool_void){
    checkThreadIsNOT(&mainThreadID);
    auto [promise,_/*args*/] = std::any_cast<cvk::method::args<bool>>(std::move(promise_bool_void));
    promise->set_value(true);
}
