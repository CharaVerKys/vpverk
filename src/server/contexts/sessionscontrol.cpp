#include "sessionscontrol.hpp"

cvk::expected_contextsReg SessionsControl::onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs){
    regMethod(this,&SessionsControl::move_to_main_context);

    assert(previousFuncs.size() == MainCtx); // ? check that order is correct
    std::function<void(std::stop_token)> func = std::bind_front(&SessionsControl::asyncStart, this);
    previousFuncs.push_back(std::move(func)); // ! push back only
    return previousFuncs;
}

tl::expected<Unit,std::exception_ptr> SessionsControl::acceptConnection(Unit&&){
    // start acceptor and run as infinite loop
    
    return tl::unexpected{std::make_exception_ptr(std::logic_error("function incomplete"))};
}

void SessionsControl::asyncStart(std::stop_token token){
    stop_token = token;
    //add thread pool here
}

void SessionsControl::move_to_main_context(std::any promise_bool_void){
    auto [promise,_/*args*/] = std::any_cast<cvk::method::args<bool>>(std::move(promise_bool_void));
    promise->set_value(true);
}
