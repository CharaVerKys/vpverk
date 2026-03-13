#pragma once

#include <asio/ip/address_v4.hpp>
#include <cvk.hpp>
#include <asio.hpp>
#include <coroutinesthings.hpp>
#include <set>
#include <types/settingssnapshot.hpp>
#include <connection.hpp>
#include <cvkaes.hpp>
#include <atomic_queue_type.hpp>


class SessionsControl : public cvk::Receiver{

public:
    cvk::expected_contextsReg onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs);
    tl::expected<Unit,std::exception_ptr> startAcceptingConnections(Unit&&);
                    
private:
    // for multiple session add here thread pool, and just share io context and stop token
    void asyncStart(std::stop_token);
    void setupTun();

    // promise<bool> (void) // just invoked promise on this thread
    void move_to_main_context(std::any promise_bool_void);

    // wait for anything on tun interface, detect whom packet belong to, and spawn coroutine that will do send on corresponding connection
    cvk::future<Unit> routeTunToClients(bool use_loop); //no actual code yet
    void emptyQueue();
    asio::ip::address_v4 getNewAddress();

    cvk::coroutine_t acceptConnection();

    // preemptive with reschedule on self after several ops
    cvk::coroutine_t closeOldSessions();

    cvk::coroutine_t startConnection(std::shared_ptr<Connection> con); //detached from accept connection, perform current socket lifetime logic
    cvk::future<aig::AesSession> exchangeAESkey(Connection&, SettingsSnapshot const& settings/*maybe only private key*/); // exchange via RSA
    cvk::future<bool/*allowed*/> authenticateUser(Connection&, aig::AesSession& aeskey, SettingsSnapshot const& settings/*maybe only logins*/); // verify user in allowed list
    cvk::future<Unit> sendIp(Connection&, aig::AesSession& aeskey, uint32_t ip);  //send ip, that client should assign to their tun
    cvk::future<bool/*normal disconnect == true*/> performDataExchange(Connection&, aig::AesSession& aeskey); // just vpn logic, when coroutine return connection ended, no other logic
private:
    std::stop_token stop_token;

    std::unique_ptr<asio::ip::tcp::acceptor>  acceptor_;
    std::unique_ptr<asio::steady_timer>       cleanup_timer_; // session alive time limit
    std::vector<std::shared_ptr<Connection>>  connections_; //for closing on socket, destroy happens in coroutines
                                                            

    std::map</*uint*/asio::ip::address_v4,
        std::pair<
            std::shared_ptr<Connection>,
            std::shared_ptr<aig::AesSession>
        >
    > tunRouter; //no fill and clean process yet
    routes_queue tunQueue{routes_q::CAPACITY}; //manual memory management

    //hard coded same value in setupTun
    static constexpr uint32_t address_begin = (10u << 24) | (123u << 16); // use 10.123.0.0/16 subnet
    //std::atomic_uint16_t counter = 1; // let subset be 10.xxx.0.0/16, than it is 'almost' safe \ need to sub broadcast and network addresses (if i remember how subnets work right)
                          // todo - add this xxx as setting, and unconstexpr address_begin
    // nah i have to use lock
    std::mutex unique_address_mutex;
    uint16_t unique_address_counter = 1;
    std::set<asio::ip::address_v4> unique_address_set;
};

