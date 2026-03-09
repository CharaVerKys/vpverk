#include "settings.hpp"
#include <defines.h>
#include "cvkrsa.hpp"
#include "threadchecking.hpp"
#include <other_coroutinethings.hpp>

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

    checkThread(&settingsThreadID);
    t_ctx = &this->loop_;

    write_serv() << "started settings";
}

void Settings::read(std::filesystem::path const& path){
    write_serv() << "start reading settings";
    std::ifstream file(path);
    if (!file.is_open()){
        throw std::runtime_error("Failed to open config file: " + path.string());
    }
    json js = json::parse(file);

    //chatbot gen but that kind of just api

    // port
    current_snapshot.port = js.at("port").get<uint16_t>();

    // max_alive_time (in seconds)
    current_snapshot.max_alive_time = std::chrono::seconds(js.at("max_alive_time").get<int64_t>());

    // private_key from file path
    std::string keyPath = js.at("private_key_path").get<std::string>();

    auto key_ = aig::RsaKey::from_pem_file(path.c_str(), aig::RsaKeyType::Private);
    if(not key_){
        throw std::runtime_error("Failed to read private key: " + keyPath);
    }
    current_snapshot.private_key = std::make_shared<aig::RsaKey>(std::move(*key_));

    // allowed_logins — parse, sort, store
    auto logins = js.at("allowed_logins").get<std::vector<std::string>>();
    std::sort(logins.begin(), logins.end());
    if(not logins.size()){
        throw std::runtime_error("logins list appears to be empty");
    }
    current_snapshot.binarySorted_allowedLogins = std::make_shared<std::vector<std::string>>(std::move(logins));
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
