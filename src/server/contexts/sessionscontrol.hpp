#pragma once

#include <cvk.hpp>
#include <asio.hpp>
#include <coroutinesthings.hpp>
#include <types/settingssnapshot.hpp>
#include <connection.hpp>
#include <cvkaes.hpp>


class SessionsControl : public cvk::Receiver{

public:
    cvk::expected_contextsReg onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs);
    tl::expected<Unit,std::exception_ptr> startAcceptingConnections(Unit&&);
                    
private:
    // for multiple session add here thread pool, and just share io context and stop token
    void asyncStart(std::stop_token);

    // promise<bool> (void) // just invoked promise on this thread
    void move_to_main_context(std::any promise_bool_void);

    cvk::coroutine_t acceptConnection();

    // preemptive with reschedule on self after several ops
    cvk::coroutine_t closeOldSessions();

    cvk::coroutine_t startConnection(); //detached from accept connection, perform current socket lifetime logic
    cvk::future<aig::AesSession> exchangeAESkey(SettingsSnapshot const& settings/*maybe only private key*/); // exchange via RSA
    cvk::future<bool/*allowed*/> authentificateUser(SettingsSnapshot const& settings/*maybe only logins*/); // verify user in allowed list
    cvk::future<bool/*normal disconnect == true*/> performDataExchange(); // just vpn logic, when coroutine return connection ended, no other logic
private:
    std::stop_token stop_token;

    std::unique_ptr<asio::ip::tcp::acceptor>  acceptor_;
    std::unique_ptr<asio::steady_timer>       cleanup_timer_; // session alive time limit
    std::vector<std::shared_ptr<Connection>>  connections_;
};

