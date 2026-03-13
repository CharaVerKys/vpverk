#include "asioapi.hpp"
#include "cussert.hpp"
#include <limits>
#include <other_coroutinethings.hpp>

//need only move constructor, no reference support, any other type should be fine
#define CALL_LOCK(return_value, ...)\
    std::optional<decltype(__VA_ARGS__)> socket_call_lock_##return_value; \
    if(socket_call_lock){ \
        socket_call_lock->lock(); \
        try{ \
            socket_call_lock_##return_value.emplace(__VA_ARGS__); \
        }catch(...){ \
            socket_call_lock->unlock(); \
            throw; \
        } \
        socket_call_lock->unlock(); \
    }else{ \
        socket_call_lock_##return_value.emplace(__VA_ARGS__); \
    } \
    decltype(__VA_ARGS__) return_value{std::move(*socket_call_lock_##return_value)};

#define CALL_LOCK_void(...)\
    if(socket_call_lock){ \
        socket_call_lock->lock(); \
        try{ \
            __VA_ARGS__; \
        }catch(...){ \
            socket_call_lock->unlock(); \
            throw; \
        } \
        socket_call_lock->unlock(); \
    }else{ \
        __VA_ARGS__; \
    } \

namespace cvk_asio{
    tl::expected<bool,std::error_code> reliable_is_open(asio::ip::tcp::socket& socket){
        if (!socket.is_open()) {
            return false;
        }
        std::byte buffer;
        asio::error_code ec;
        socket.receive(asio::buffer(&buffer, 1),asio::socket_base::message_peek,ec);
        if (not ec or
            ec == asio::error::would_block or
            ec == asio::error::try_again) {
            return true;
        }
        if (ec == asio::error::eof or ec == asio::error::connection_reset) {
            return false;
        }
        return tl::unexpected(ec);
    }//reliable is open

    cvk::future<tl::expected<Unit,std::error_code>> send(asio::ip::tcp::socket& socket, std::span<const uint8_t> buffer, std::recursive_mutex* socket_call_lock){
        cussert(buffer.size());
        if(buffer.empty() or buffer.size() > std::numeric_limits<uint32_t>::max()){co_return tl::unexpected{std::make_error_code(std::errc::message_size)};}
        CALL_LOCK(ex, reliable_is_open(socket)) // i not remember why i even wrote this, will be same result as 'send -> error code' anyway
        if(not ex or not *ex){
            co_return ex.has_value() ? tl::unexpected{std::make_error_code(std::errc::connection_reset)} : tl::unexpected{ex.error()};
        }
        uint32_t offset = 0;
        while(offset not_eq buffer.size()){
            // time to create generic awaiters
            auto res = co_await await::send(socket, buffer.subspan(offset), socket_call_lock);
            if(not res){
                co_return tl::unexpected{res.error()};
            }
            offset+=res.value();
        }
        co_return {};
    }

    cvk::future<tl::expected<Unit,std::error_code>> read_some(asio::ip::tcp::socket& socket, std::span<uint8_t> out_buffer, uint32_t amount/*0 == max possible */, std::recursive_mutex* socket_call_lock){
        uint32_t was_read = 0;
        if(amount > out_buffer.size() and amount not_eq 0) {
            co_return tl::unexpected{std::make_error_code(std::errc::no_buffer_space)};
        }
        uint32_t need_read = amount == 0 ? (uint32_t)out_buffer.size() : amount;
        while(was_read not_eq need_read){
            auto exp = co_await read_some_unreliable(socket,out_buffer.subspan(was_read),need_read-was_read, socket_call_lock);
            if(not exp){co_return tl::unexpected{exp.error()};} // only if there was better way...
                                                    // i start to thinking that and_then and or_else isnt such useful for inner code
            was_read += exp.value();
        }
        co_return {};
    }

    cvk::future<tl::expected<uint32_t/*amount*/,std::error_code>> read_some_unreliable(asio::ip::tcp::socket& socket, std::span<uint8_t> out_buffer, uint32_t amount/*0 == max possible */, std::recursive_mutex* socket_call_lock){
        auto& buffer = out_buffer; 
        cussert(buffer.size());
        if(buffer.empty() or std::max(uint64_t(amount), buffer.size()) > std::numeric_limits<uint32_t>::max()){co_return tl::unexpected{std::make_error_code(std::errc::message_size)};} //idk maybe keep msg size but that kind of different //nah, will keep msg size fore clear api
        if(amount > buffer.size()){co_return tl::unexpected{std::make_error_code(std::errc::invalid_argument)};}
        CALL_LOCK(ex, reliable_is_open(socket)) // i not remember why i even wrote this, will be same result as 'read -> error code' anyway
        if(not ex or not *ex){
            co_return ex.has_value() ? tl::unexpected{std::make_error_code(std::errc::connection_reset)} : tl::unexpected{ex.error()};
        }
        // time to create generic awaiters
        amount = amount == 0 ? (uint32_t)buffer.size() : amount;
        auto res = co_await await::read(socket, buffer.first(amount), socket_call_lock);
        // if(not res){
        //     co_return tl::unexpected{res.error()};
        // }
        // co_return *res;
        co_return res;
    }

    namespace await{
        send::send(asio::ip::tcp::socket& s, std::span<const uint8_t> send_buffer, std::recursive_mutex* socket_call_lock)
            :socket_(s),buffer_just_to_actually_send_only_on_await_suspend(send_buffer),socket_call_lock{socket_call_lock}{}
        void send::await_suspend(std::coroutine_handle<> h){
            auto const& buf = buffer_just_to_actually_send_only_on_await_suspend;
            CALL_LOCK_void(
            socket_.async_write_some( //init call (may be) protected by mutex
                        asio::buffer(buf.data(),buf.size()),
                        [h, this]
                         (std::error_code const& ec,
                         size_t sendBytes)
                         {
                            RESCHEDULE_HERE(h); // somehow it more useful than reschedule awaiter
                                                // also hope that asio do really fast (re)scheduling
                            if(ec){
                                this->expected = tl::unexpected{ec};
                                return;
                            }
                            this->expected = (uint32_t)sendBytes;
                            // erm, literally everything done

                         } // callback
                    )//;// async write
            ); // CALL_LOCK_void 
        } // await suspend
        read::read(asio::ip::tcp::socket& s, std::span<uint8_t> read_buffer, std::recursive_mutex* socket_call_lock)
            :socket_(s),buffer(read_buffer),socket_call_lock{socket_call_lock}{}
        void read::await_suspend(std::coroutine_handle<> h){
            auto const& buf = buffer;
            CALL_LOCK_void(
            socket_.async_read_some(
                        asio::buffer(buf.data(),buf.size()),
                        [h, this]
                         (std::error_code const& ec,
                         size_t readBytes)
                         {
                            RESCHEDULE_HERE(h); // somehow it more useful than reschedule awaiter
                                                // also hope that asio do really fast (re)scheduling
                            if(ec){
                                this->expected = tl::unexpected{ec};
                                return;
                            }
                            this->expected = (uint32_t)readBytes;
                            // erm, literally everything done

                         } // callback
                    )// async read
            ) //CALL_LOCK_void
        } // await suspend
    }//nms await

}//nms cvk_asio
