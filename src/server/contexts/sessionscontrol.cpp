#include "sessionscontrol.hpp"
#include "coroutinesthings.hpp"
#include "defines.h"
#include "server_method_config.hpp"
#include "threadchecking.hpp"
#include "types/settingssnapshot.hpp"
#include <coroutine>
#include <other_coroutinethings.hpp>


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
            asio::ip::tcp::socket socket{*cvk::ContStore::instance()->get_io_context(MainCtx)};
            acceptor_->async_accept(socket, [handle, &ec](std::error_code const& error){
                    ec = error;
                    RESCHEDULE_HERE(handle); //reschedule on same thread macros
                    }
                  );
            co_await std::suspend_always{};

            if(ec){
                throw std::runtime_error(std::string("asio <error_code> ")+ec.message());
            }

            Connection connection(std::move(socket));

            //detached coroutine
            startConnection(std::move(connection));
            
            // done, just process next
        }
        
    } catch (std::exception const& e) {
        write_serv() << cll::error << "error in acceptor: " << e.what();
        acceptor_ = nullptr;
        acceptConnection();
    }
}

cvk::coroutine_t SessionsControl::closeOldSessions()
{
    size_t processed = 0;
    size_t i = 0;

    const SettingsSnapshot settings = co_await cvk::method::Invoke<SettingsSnapshot>{
        // ? from context to context
        // ? first is where coroutine will resume, second where func will calls
        MainCtx, SettingsCtx,
        method::get_settings
    };
    const std::chrono::seconds max_alive_time = settings.getMaxAliveTime();

    while (i < connections_.size()) {
        auto const& conn = connections_[i];
        const auto opt = conn->is_active(); 
        if (not opt) {
            ++i;
            continue;
        }

        if(not *opt){
            conn->close("not active (should be no-op)");
            connections_[i] = std::move(connections_.back());
            connections_.pop_back();
        }

        if (conn->alive_time() >= max_alive_time) {
            conn->close("session expired");
            connections_[i] = std::move(connections_.back());
            connections_.pop_back();
        } else {
            ++i;
        }

        if (++processed % 10 == 0) {
            co_await reschedule{};
            if (i > connections_.size())
                i = connections_.size();
        }
    }
}

cvk::coroutine_t SessionsControl::startConnection() //detached from accept connection, perform current socket lifetime logic
{
}
cvk::future<aig::AesSession> SessionsControl::exchangeAESkey(SettingsSnapshot const& settings/*maybe only private key*/) // exchange via RSA
{
}
cvk::future<bool/*allowed*/> SessionsControl::authentificateUser(SettingsSnapshot const& settings/*maybe only logins*/) // verify user in allowed list
{
}
cvk::future<bool/*normal disconnect == true*/> SessionsControl::performDataExchange() // just vpn logic, when coroutine return connection ended, no other logic
{
}

void SessionsControl::asyncStart(std::stop_token token){
    stop_token = token;

    checkThreadIsNOT(&mainThreadID);
    t_ctx = &this->loop_;

    //add thread pool here
    // and init context for them all to use coroutines!


    cleanup_timer_ = std::make_unique<asio::steady_timer>(loop_);

    // ready for macros code, if i will ever want
    static std::optional<std::function<void()>> lambda_self_ = std::nullopt;
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

    write_serv() << "started sessions control";
}

void SessionsControl::move_to_main_context(std::any promise_bool_void){
    checkThreadIsNOT(&mainThreadID);
    auto [promise,_/*args*/] = std::any_cast<cvk::method::args<bool>>(std::move(promise_bool_void));
    promise->set_value(true);
}
