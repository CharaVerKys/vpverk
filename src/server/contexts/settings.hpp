#pragma once

#include <cvk.hpp>
#include <filesystem>
#include <list>
#include <types/settingssnapshot.hpp>

class Settings : public cvk::Receiver{
public:
            
public:
    cvk::expected_contextsReg onAsyncStart(std::vector<std::function<void(std::stop_token)>>&& previousFuncs);
    void read(std::filesystem::path const& path);
                    
private:
    void asyncStart(std::stop_token);

    // promise<SettingsSnapshot> (void)
    void get_settings(std::any promise_bool_void);

private:
    SettingsSnapshot current_snapshot;
    std::list<SettingsSnapshot> prev_snapshots;
    std::stop_token stop_token;
};

