#include "settings.hpp"
#include <defines.h>

#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

cvk::expected_contextsReg Settings::onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs){
    regMethod(this,&Settings::get_settings);

    assert(previousFuncs.size() == SettingsCtx); // ? check that order is correct
    std::function<void(std::stop_token)> func = std::bind_front(&Settings::asyncStart, this);
    previousFuncs.push_back(std::move(func)); // ! push back only
    return previousFuncs;
}

void Settings::asyncStart(std::stop_token token){
    stop_token = token;
}

void Settings::read(std::filesystem::path const& path){
    write_serv() << "start reading settings";
    std::ifstream file(path);
    json data = json::parse(file);
}

// promise<SettingsSnapshot> (void)
void Settings::get_settings(std::any promise_bool_void){
    // ? method::args<promise_value>
    auto [promise,_/*args*/] = std::any_cast<cvk::method::args<SettingsSnapshot>>(std::move(promise_bool_void));
    //int8_t i = std::any_cast<int8_t>(std::move(args));
    // allow move only
    auto new_ = current_snapshot;
    promise->set_value(std::move(new_));
}
