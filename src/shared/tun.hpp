#pragma once

#include "hpp/defines.h"
#include "hpp/future.h"
#include <asio/posix/stream_descriptor.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>



class Tun{
public:
    // cuabort on error here
    Tun();

    ~Tun();

    [[nodiscard]] static Tun& instance(){static Tun t; return t;}

    // buffer have to be exactly 1500 - maximal possible ip packet size by default (linux)
    // only one thread expected, but technically support several
    [[nodiscard]] cvk::future<uint16_t/*size*/> read(std::span<uint8_t> buffer, std::chrono::milliseconds timeout);
    // return 0 if was canceled
    // cancel may be frequent

    // async thread-safe, throw on file descriptor error (kind of unrealistic actually), async lock inside
    [[nodiscard]] cvk::future<Unit> write(std::span<const uint8_t> ip_packet);
private:
    //does not perform lock
    struct Lock{std::atomic_bool* l; Lock(Lock const&) = delete;Lock():l{nullptr}{}Lock(std::atomic_bool&a):l{&a}{}~Lock(){if(l){l->store(false,std::memory_order_release);}}
           Lock(Lock&& o):l(o.l){o.l = nullptr;} //move constructor allowed
    };
    [[nodiscard]] cvk::future<Lock> lock(); //async lock and give out lifetime

private:
    int file_descriptor = 0;
    std::atomic_bool it_is_read_lock = false;
    std::atomic_bool lock_ = false; // since AGAIN asio have no support for thread safety even when it is literally posix file descriptor
                                    // and that additionally create issues with read process
    asio::posix::stream_descriptor tun;
    asio::steady_timer cancel_timer;
};
