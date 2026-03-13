#pragma once

#include "cussert.hpp"
#include "hpp/defines.h"
#include "hpp/future.h"
#include <asio/ip/address_v4.hpp>
#include <asio/strand.hpp>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <types/settingssnapshot.hpp>
#include <types/communicationstats.hpp>
#include <asio/ip/tcp.hpp>

// idk, this lock system is actually lock system, and if there more than 2 coroutines try to lock same path (read/write) it WILL spin scheduler for no reason
// so ideally dont use it if possible
// context lock is required anyway, via current plans it will be actually performed on statistics send context switch only
// read/send locks are required for thread safe reliable_is_open() and close()

// actual work will be performed (write) on tun router, and (read) on just per-connection coroutine

class Connection{
  public:
    using block_size_t = uint32_t;
    static std::span<uint8_t> block_get_buffer(block_size_t& read_){return std::span<uint8_t>((uint8_t*)&read_,sizeof(read_));}
  public:
    struct Socket_lock{
      Socket_lock(std::atomic_bool&s):l{s}{}; //does not perform lock itself
      Socket_lock(Socket_lock const&) = delete;
      ~Socket_lock(){l.store(false,std::memory_order_release);}
     private:
      std::atomic_bool& l;
    };
    struct read_wait_awaiter : std::suspend_always{
        read_wait_awaiter(asio::ip::tcp::socket& s):s_{s}{}
        void await_suspend(std::coroutine_handle<> h)noexcept;
        std::error_code await_resume()noexcept{return ec;}
      private: asio::ip::tcp::socket& s_; std::error_code ec;
    };
  public:
    Connection(asio::strand<asio::io_context::executor_type> strand, asio::ip::tcp::socket sock)
        :strand(std::move(strand))
        ,sock_{std::move(sock)}
        ,spawned{std::chrono::steady_clock::now()}{cussert(this->strand == sock_.get_executor());}
    Connection(Connection&& o)
        :strand(std::move(o.strand))
        ,sock_{std::move(o.sock_)}
        ,spawned{std::move(o.spawned)}{cussert(this->strand == sock_.get_executor());}
        // no operator yet
    Connection(Connection const& o) = delete;

    //4(6) thread safe funcs
    [[nodiscard]] std::optional<std::unique_ptr<Socket_lock>> read_lock();
    [[nodiscard]] std::optional<std::unique_ptr<Socket_lock>> write_lock();
    [[nodiscard]] std::optional<std::pair<std::unique_ptr<Socket_lock>,std::unique_ptr<Socket_lock>>>
        context_lock();
    [[nodiscard]] std::chrono::seconds alive_time(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - spawned);}
    [[nodiscard]] std::optional<bool> is_active();
    [[nodiscard]] cvk::future<Unit> close(std::string_view /*reason*/);

    //thread unsafe
    [[nodiscard]] bool is_active_nolock();
    std::optional<asio::ip::address_v4> ip();

    //since i added strand they are thread safe but will cause garbage data because of asio sockets unreliable send/read _some
    //so have to lock anyway
    // ... maybe delete read_some and keep only reliable variant?

    // should be called only under locks
    // also logic for coroutine to move out of their io_context to perform some tasks is outer logic
    // but they all should be under lock, when 'this' coroutine perform on other io_context it is similar to performing IO 
    [[nodiscard]] cvk::future<tl::expected<Unit,std::error_code>> send(std::span<const uint8_t>);
        //inner api names reverted, changed at last moment
    [[nodiscard]] cvk::future<tl::expected<uint32_t/*amount*/,std::error_code>> read_some(std::span<uint8_t> out_buffer, uint32_t amount = 0 /*0 == max possible */);
    [[nodiscard]] cvk::future<tl::expected<Unit,std::error_code>> read_some_reliable(std::span<uint8_t> out_buffer, uint32_t amount = 0 /*0 == max possible */);
    [[nodiscard]] read_wait_awaiter wait_read(){return {sock_};}
    [[nodiscard]] size_t available(){return sock_.available();}

    //cvk::future<tl::expected<std::vector<uint8_t>,std::error_code>> read_some_nb/*no buffer*/(uint32_t amount = 0 /*0 == max possible */);

  private:
    std::atomic_bool closed = false; //not sure that it should be atomics or not

    // when lock hold, no coroutine can perform :
    // switching to another thread, 
    // call async operations for this connection (socket read/write correspondingly)
    std::atomic_bool write_lock_ = false;
    std::atomic_bool read_lock_ = false;
    // anyway, just basically anything that may be considered unsafe to do from different coroutines == potentially threads
    // and it is performing method::invoke and socket read-write
    // no locks only on initialization, before registration as route for tun
     //NOTE: when performing context switch, it should be lock both
     //and also dont hold write+read lock while doing socket work against dead locks, it is coroutines after all

    // JUST FOUND OUT THAT **ANY** ASYNC OPERATIONS ARE THREAD UNSAFE
    // EVEN IF POLL SUPPORT MULTIPLE THREAD WAITING ON SAME SOCKET LIKE async_wait(read) ASIO DOES NOT SUPPORT IT
    // EVEN READ AND SEND ACTUALLY UNSAFE CONCURRENTLY
    // AND I HAVE TO ADD STRAND ANYWAY
    asio::strand<asio::io_context::executor_type> strand;

    asio::ip::tcp::socket sock_;
    std::chrono::time_point<std::chrono::steady_clock> spawned;
};
