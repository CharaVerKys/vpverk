#pragma once

#include <cvk.hpp>

class SessionsControl : public cvk::Receiver{
    std::stop_token stop_token;
            
public:
    cvk::expected_contextsReg onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs);
    tl::expected<Unit,std::exception_ptr> acceptConnection(Unit&&);
                    
private:
    // for multiple session add here thread pool, and just share io context and stop token
    void asyncStart(std::stop_token);

    // promise<bool> (void) // just invoked promise on this thread
    void move_to_main_context(std::any promise_bool_void);
};

