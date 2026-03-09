#pragma once

#include <chrono>
#include <vector>
#include <atomic>
#include <string>
#include <cstdint>
#include <memory>
#include <cvkrsa.hpp>


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
    std::shared_ptr<const aig::RsaKey> getRSAPrivateKey()const{return private_key;}
    uint16_t getPort()const{return port;}
    std::chrono::seconds getMaxAliveTime()const{return max_alive_time;}

  private:
    std::shared_ptr<std::vector<std::string>> binarySorted_allowedLogins = std::make_shared<std::vector<std::string>>();
    std::shared_ptr<aig::RsaKey> private_key = nullptr;
    uint16_t port;
    std::chrono::seconds max_alive_time;
};
