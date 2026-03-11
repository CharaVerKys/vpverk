#include "connection.hpp"
#include "other_coroutinethings.hpp"
#include <asioapi.hpp>
#include <atomic>
#include <limits>

void Connection::read_wait_awaiter::await_suspend(std::coroutine_handle<> h)noexcept{
    s_.async_wait(asio::ip::tcp::socket::wait_read,[h, this](std::error_code const&e){
        RESCHEDULE_HERE(h);
        ec = e;
    });
}

std::optional<std::unique_ptr<Connection::Socket_lock>> Connection::read_lock(){
    //try lock
    bool exchanged = read_lock_.exchange(true,std::memory_order_acquire);
    if(exchanged == true){return std::nullopt;}

    //give lock control out
    return {std::make_unique<Socket_lock>(read_lock_)};
}
std::optional<std::unique_ptr<Connection::Socket_lock>> Connection::write_lock(){
    //try lock
    bool exchanged = write_lock_.exchange(true,std::memory_order_acquire);
    if(exchanged == true){return std::nullopt;}

    //give lock control out
    return {std::make_unique<Socket_lock>(write_lock_)};
}
std::optional<std::pair<std::unique_ptr<Connection::Socket_lock>,std::unique_ptr<Connection::Socket_lock>>> Connection::context_lock(){
    //try lock
    bool exchanged = read_lock_.exchange(true,std::memory_order_acquire);
    if(exchanged == true){return std::nullopt;}

    exchanged = write_lock_.exchange(true,std::memory_order_relaxed);
    if(exchanged == true){
        read_lock_.store(false,std::memory_order_relaxed);
        return std::nullopt;}

    //give lock control out
    return {std::make_pair(std::make_unique<Socket_lock>(read_lock_),std::make_unique<Socket_lock>(write_lock_))};
}
std::optional<bool> Connection::is_active(){
    //try lock
    bool exchanged = read_lock_.exchange(true,std::memory_order_acquire);
    if(exchanged == true){return std::nullopt;}

    auto exp = cvk_asio::reliable_is_open(sock_); //perform sync read (short 1 byte peek) operation

    read_lock_.store(false, std::memory_order_release);

    if(exp){return *exp;}
    throw std::runtime_error(std::string("reliable is open unexpected error code: ") + exp.error().message());
}
bool Connection::is_active_nolock(){
    auto exp = cvk_asio::reliable_is_open(sock_); //perform sync read (short 1 byte peek) operation
    if(exp){return *exp;}
    throw std::runtime_error(std::string("reliable is open unexpected error code: ") + exp.error().message());
}

cvk::future<Unit> Connection::close(std::string_view reason){
    //only one can fire, even if run 100 from different threads
    bool exchanged = closed.exchange(true,std::memory_order_relaxed);
    if(exchanged == true){co_return{};}

    //lock
    while(true){
        bool exchanged = write_lock_.exchange(true,std::memory_order_acquire);
        if(exchanged == true){
            co_await reschedule{};
        }else{
            break;
        }
    }
    auto lifetime = Socket_lock{write_lock_};
    auto expe_ = cvk_asio::reliable_is_open(sock_);
    if(not expe_ or not *expe_){
        co_return {};
    }
    std::vector<uint8_t> buff;
    const char* _Error = "_Error: ";
    buff.resize(reason.size()+1 +std::strlen(_Error)+4);
    uint32_t instead_of_size = std::numeric_limits<uint32_t>::max();
    std::memcpy(buff.data(),&instead_of_size,4); // now i can differ error and actual packet
    std::memcpy(buff.data()+4,_Error,std::strlen(_Error)); //flat text
    std::memcpy(buff.data()+4+std::strlen(_Error),reason.data(),reason.size()); //flat text
    buff[reason.size()+std::strlen(_Error)+4] = 0;
    auto res = co_await send(buff); //reliable send everything
    if(not res){
        //idk just skip
    }
    //both may throw
    sock_.shutdown(asio::ip::tcp::socket::shutdown_both);//send fin
    //todo (unreliable source)
    //ai says, that i cant close when there anything on read buffer
    //keeping buffer busy is potential attack direction
    //tho i have no any limiters now, it is anyway possible attack
    //i cant just read, ai offered this
    sock_.non_blocking(true); //and sync read
          //i really not sure whenever it will work or not
          //i mean work as guard against dos
          //but well, at least try
    char drain[256]; std::error_code drain_ec;
    while(sock_.read_some(asio::buffer(drain),drain_ec)>0){}
    //this is really seems so unreliable
    //but anyway
    //after drain, close send fin (or just do nothing?) and not rst
    // (also info from ai)
    sock_.close();
    co_return{};
}
// std::optional<asio::ip::address_v4> Connection::ip()
// {
//     std::error_code ec;
//     auto res = sock_.remote_endpoint(ec).address().to_v4();
//     if(ec){
//         return std::nullopt;
//     }
//     return res;
// }
cvk::future<tl::expected<Unit,std::error_code>> Connection::send(std::span<const uint8_t> buff){
    co_return co_await cvk_asio::send(sock_, buff);
}
cvk::future<tl::expected<uint32_t/*amount*/,std::error_code>> Connection::read_some(std::span<uint8_t> out_buffer, uint32_t amount/*0 == max possible */){
    //co_return co_await cvk_asio::read_some(sock_, out_buffer, amount);
    co_return co_await cvk_asio::read_some_unreliable(sock_, out_buffer, amount);
}
cvk::future<tl::expected<Unit,std::error_code>> Connection::read_some_reliable(std::span<uint8_t> out_buffer, uint32_t amount/*0 == max possible */){
    co_return co_await cvk_asio::read_some(sock_, out_buffer, amount);
}


