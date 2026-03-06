#include "sessionscontrol.hpp"
#include "defines.h"
#include "server_method_config.hpp"
#include "threadchecking.hpp"
#include "types/settingssnapshot.hpp"
#include <asio/use_awaitable.hpp>
#include <other_coroutinethings.hpp>


namespace {

// awaitable: suspend until asio::steady_timer fires
struct await_timer {
    asio::steady_timer& timer;
    asio::error_code    ec_{};

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        timer.async_wait([this, h](asio::error_code ec) {
            ec_ = ec;
            h.resume();
        });
    }
    asio::error_code await_resume() noexcept { return ec_; }
};

} // namespace

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
            auto socket = co_await acceptor_->async_accept(asio::use_awaitable);
        }
        
    } catch (std::exception const& e) {
        write_serv() << cll::error << "error in acceptor: " << e.what();
        acceptor_ = nullptr;
        acceptConnection();
    }
}

void SessionsControl::asyncStart(std::stop_token token){
    stop_token = token;

    checkThreadIsNOT(&mainThreadID);
    t_ctx = &this->loop_;

    //add thread pool here
    // and init context for them all to use coroutines!


    // chatbot: start timer here, it should only fire every hour and create coroutine
    cleanup_timer_ = std::make_unique<asio::steady_timer>(loop_);

    write_serv() << "started sessions control";
}

void SessionsControl::move_to_main_context(std::any promise_bool_void){
    checkThreadIsNOT(&mainThreadID);
    auto [promise,_/*args*/] = std::any_cast<cvk::method::args<bool>>(std::move(promise_bool_void));
    promise->set_value(true);
}
