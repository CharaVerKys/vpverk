#pragma once

struct Connection{
    //just to compile
    Connection(socket&&);
    std::chrono::seconds alive_time(){return {};}
    std::optional<bool> is_active(){return true;}
    void close(std::string_view /*reason*/){}
};
