#include <asio/use_awaitable.hpp>
#include <coroutine>
#include <limits>
#include <tun.hpp>

#include "coroutinesthings.hpp"
#include "cussert.hpp"
#include <cvk.hpp>

#include <asio/write.hpp>

#include <linux/if_tun.h>
#include <net/if.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <atomic>
#include <other_coroutinethings.hpp>

Tun::Tun()
// int
:file_descriptor{[]()->int{
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0){cuabort("failed to open tun file descriptor");}
    cussert(t_ctx.has_value());
    return fd;
}()}
,strand{asio::make_strand(**t_ctx)}
//posix stream
,tun{[](int fd, auto& strand)->asio::posix::stream_descriptor{
    struct ifreq ifr{};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; // no packet info header, raw IP
    const char* name = "tun0";
    std::strncpy(ifr.ifr_name, name, IFNAMSIZ);

    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        close(fd);
        cuabort("failed to set tun descriptor flags");
    }

    // make non-blocking for asio
    fcntl(fd, F_SETFL, O_NONBLOCK); //? cant fail?
    cussert(cvk::ContStore::instance()->get_io_context(MainCtx) == *t_ctx); //if i not stupid it should be same **pointers**
    return {strand, fd};
}(file_descriptor, strand)}
//timer
,cancel_timer{strand}
{   //constructor body
    //std::atomic_thread_fence(std::memory_order_release); //not required
}

Tun::~Tun(){
    std::atomic_thread_fence(std::memory_order_acquire);
    if(file_descriptor > 0){
        close(file_descriptor);
    }
}


cvk::future<uint16_t/*size*/> Tun::read(std::span<uint8_t> buffer, std::chrono::milliseconds timeout){
    cussert(buffer.size() == 1500);
    cussert(timeout.count() >= 10 and timeout.count() <= 100);
    
    //for thread, data and state safety, resume coroutine only in read callback
    std::error_code ec;
    uint16_t bytes_read = 0;
    auto handle = co_await cvk::co_getHandle{};

    co_await resched{}; // see comments in Tun::write()
    Lock lock_ = co_await lock();
    // gap between two atomics
    it_is_read_lock.store(true,std::memory_order_relaxed);
    Lock lock_2{it_is_read_lock};

    // can be canceled from:
    // 1. sender if it see it_is_read_lock == true
    // 2. by timeout
    tun.async_read_some(asio::buffer(buffer), [handle, &ec, this, &bytes_read](std::error_code e, size_t read){
        if(e == std::errc::operation_canceled){
            if(read not_eq 0){
                cuabort("wrong asio api usage, idk what exactly, but i did something wrong");
            }
            RESCHEDULE_HERE(handle);
            return;
        }
        ec = e;
        cussert(read < std::numeric_limits<uint16_t>::max());
        bytes_read = (uint16_t)read;

        cancel_timer.cancel();
        RESCHEDULE_HERE(handle);
    });
    
    cancel_timer.expires_after(timeout);
    cancel_timer.async_wait([this](std::error_code ec){
                if(ec == std::errc::operation_canceled){return;}
                tun.cancel();
            });

    co_await std::suspend_always();

    if(ec){throw std::runtime_error(ec.message());}
    if(bytes_read == 0){co_return 0;}
    co_return bytes_read;
}

    // async thread-safe, throw on file descriptor error (kind of unrealistic actually), async lock inside
cvk::future<Unit> Tun::write(std::span<const uint8_t> ip_packet){
    if(it_is_read_lock.load(std::memory_order_acquire)){
        //btw, this can fire badly between read and current threads, but at max i lose 100ms; even if it bad, idk this is already kind of bad design

        // AND BECAUSE OF CANCEL BEING UNSAFE
        auto handle = co_await cvk::co_getHandle();
        asio::post(strand,[handle](){handle.resume();}); // AT LEAST IT IS SAFE TO CALL CANCEL NOW
        co_await std::suspend_always{};
        //imagine if this exist
        //co_await asio::post(strand,asio::use_awaitable);
       
        cancel_timer.cancel();
        tun.cancel();
        // read may be on timeout, so id better cancel it and perform send first
        // read will rerun anyway, just like if they timeouted on their own
    }
    // first call will be reschedule cuz lock is still hold
    // than, in queue on io context first invoke on read process
    // when read process return with bytes read 0, they will wait again
    // between start of read and acquire lock, there is explicit preemption
    // so i have guarantee that after cancel here, send will acquire lock first
    Lock lock_ = co_await lock();

    std::error_code ec;
    auto handle = co_await cvk::co_getHandle();

    // i am really really unsure about nature of this function
    // it could do implicit reschedule, so i prefer to avoid this one
    // but here:
    // 1. lock hold, should be ok
    // 2. (and more critical) on file descriptor like tun, it should be only one operation
    //          i probably could use just async_write_some and get bytes written == buffer size always

    asio::async_write(tun,asio::buffer(ip_packet),
            [handle, &ec, &ip_packet](std::error_code const& e, size_t send_){
                ec = e;
                // send; // oh there actually global namespace send included here....
                if(ip_packet.size() not_eq send_){
                    cuabort("async_write can return less bytes written than buffer size, wrong code");
                }
                RESCHEDULE_HERE(handle);
            });
    co_await std::suspend_always{};

    if(ec){throw std::runtime_error(ec.message());}
    co_return {};
    //wow, most of this function is just comments
}


cvk::future<Tun::Lock> Tun::lock(){
    while(true){
        bool exchanged = lock_.exchange(true,std::memory_order_acquire);
        if(exchanged == true){
            co_await reschedule{};
        }else{
            break;
        }
    }
    co_return {lock_};
}

