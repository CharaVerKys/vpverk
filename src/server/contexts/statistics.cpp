#include "statistics.hpp"
#include <types/communicationstats.hpp>

cvk::expected_contextsReg Statistics::onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs){
    regMethod(this,&Statistics::write_session_stats);

    assert(previousFuncs.size() == StatisticsCtx); // ? check that order is correct
    std::function<void(std::stop_token)> func = std::bind_front(&Statistics::asyncStart, this);
    previousFuncs.push_back(std::move(func)); // ! push back only
    return previousFuncs;
}

void Statistics::asyncStart(std::stop_token token){
    stop_token = token;
}

// promise<SettingsSnapshot> (void)
void Statistics::write_session_stats(std::any void_CommunicationStats){
    auto stats = std::any_cast<CommunicationStats>(std::move(void_CommunicationStats));
    (void) stats;
    // do work
}
