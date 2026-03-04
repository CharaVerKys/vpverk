#pragma once

#include <cvk.hpp>

class Statistics : public cvk::Receiver{
    std::stop_token stop_token;
            
public:
    cvk::expected_contextsReg onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs);
    void acceptConnection(Unit&&);
                    
private:
    void asyncStart(std::stop_token);
    //
    // void (CommunicationStats)
    void write_session_stats(std::any void_CommunicationStats);
};

