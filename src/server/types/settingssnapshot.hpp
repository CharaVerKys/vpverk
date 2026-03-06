#pragma once

#include <optional>
struct RSAKey{};
#include <vector>
#include <atomic>
#include <string>
#include <cstdint>
#include <memory>

class SettingsSnapshot{
    std::shared_ptr<std::atomic<uint>> counter = std::make_shared<std::atomic<uint>>(1);
    friend class Settings; // maybe instead make proper constructor, but later

  public:
    ~SettingsSnapshot(){
        counter->fetch_sub(1,std::memory_order_relaxed);
    }
    SettingsSnapshot(){}

    SettingsSnapshot(SettingsSnapshot const& other)
        :binarySorted_allowedLogins(other.binarySorted_allowedLogins)
        ,private_key(other.private_key)
        ,port(other.port)
    {
        counter->fetch_add(1,std::memory_order_relaxed);
    }
    SettingsSnapshot& operator=(SettingsSnapshot const& other){
        binarySorted_allowedLogins = (other.binarySorted_allowedLogins);
        private_key = (other.private_key);
        port = (other.port);
        counter->fetch_add(1,std::memory_order_relaxed);
        return *this;
    }
    SettingsSnapshot(SettingsSnapshot&&other)
        :binarySorted_allowedLogins(other.binarySorted_allowedLogins)
        ,private_key(other.private_key)
        ,port(other.port)
    {
        counter->fetch_add(1,std::memory_order_relaxed);
    }
    SettingsSnapshot& operator=(SettingsSnapshot&&other){
        *this = other;
        return *this;
    }
  public: 
    std::vector<std::string> const& getBinarySorted_allowedLogins()const{return *binarySorted_allowedLogins;}
    //todo now copy, with c++26 make reference, or wrapper_ref if need earlier
    std::optional<RSAKey> getRSAPrivateKey()const{return private_key ? std::optional<RSAKey>{*private_key} : std::nullopt;}
    uint16_t getPort()const{return port;}

  private:
    std::shared_ptr<std::vector<std::string>> binarySorted_allowedLogins = std::make_shared<std::vector<std::string>>();
    std::shared_ptr<RSAKey> private_key = nullptr;
    uint16_t port;
};
