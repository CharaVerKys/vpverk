#include <settings.hpp>

cvk::expected_contextsReg Settings::onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs){
    regMethod(this,&Settings::get_settings);

    assert(previousFuncs.size() == Settings); // ? check that order is correct
    std::function<void(std::stop_token)> func = std::bind_front(&Settings::asyncStart, this);
    previousFuncs.push_back(std::move(func)); // ! push back only
    return previousFuncs;
}

void Settings::asyncStart(std::stop_token token){
    stop_token = token;
}

void Settings::read(std::filesystem::path const& path){
    //work
}

// promise<SettingsSnapshot> (void)
void Settings::get_settings(std::any promise_bool_void){
    // ? method::args<promise_value>
    auto [promise,_/*args*/] = std::any_cast<cvk::method::args<SettingsSnapshot>>(std::move(promise_bool_void));
    //int8_t i = std::any_cast<int8_t>(std::move(args));
    promise->set_value(current_snapshot);
}
