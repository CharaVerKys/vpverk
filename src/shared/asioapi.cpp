#include "asioapi.hpp"
#include "cussert.hpp"
#include <limits>
#include <other_coroutinethings.hpp>

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

    cvk::future<tl::expected<Unit,std::error_code>> send(asio::ip::tcp::socket& socket, std::span<const uint8_t> buffer){
        cussert(buffer.size());
        if(buffer.empty() or buffer.size() > std::numeric_limits<uint32_t>::max()){co_return tl::unexpected{std::make_error_code(std::errc::message_size)};}
        if(auto ex = reliable_is_open(socket); not ex or not *ex){
            co_return ex.has_value() ? tl::unexpected{std::make_error_code(std::errc::connection_reset)} : tl::unexpected{ex.error()};
        }
        uint32_t offset = 0;
        while(offset not_eq buffer.size()){
            // time to create generic awaiters
            auto res = co_await await::send(socket, buffer.subspan(offset));
            if(not res){
                co_return tl::unexpected{res.error()};
            }
            offset+=res.value();
        }
        co_return {};
    }
    cvk::future<tl::expected<uint32_t/*amount*/,std::error_code>> read_some(asio::ip::tcp::socket& socket, std::span<uint8_t> out_buffer, uint32_t amount/*0 == max possible */){
        auto& buffer = out_buffer; 
        cussert(buffer.size());
        if(buffer.empty() or std::max(uint64_t(amount), buffer.size()) > std::numeric_limits<uint32_t>::max()){co_return tl::unexpected{std::make_error_code(std::errc::message_size)};} //idk maybe keep msg size but that kind of different //nah, will keep msg size fore clear api
        if(amount > buffer.size()){co_return tl::unexpected{std::make_error_code(std::errc::invalid_argument)};}
        if(auto ex = reliable_is_open(socket); not ex or not *ex){
            co_return ex.has_value() ? tl::unexpected{std::make_error_code(std::errc::connection_reset)} : tl::unexpected{ex.error()};
        }
        // time to create generic awaiters
        amount = amount == 0 ? (uint32_t)buffer.size() : amount;
        auto res = co_await await::read(socket, buffer.first(amount));
        // if(not res){
        //     co_return tl::unexpected{res.error()};
        // }
        // co_return *res;
        co_return res;
    }

    namespace await{
        send::send(asio::ip::tcp::socket& s, std::span<const uint8_t> send_buffer)
            :socket_(s),buffer_just_to_actually_send_only_on_await_suspend(send_buffer){}
        void send::await_suspend(std::coroutine_handle<> h){
            auto const& buf = buffer_just_to_actually_send_only_on_await_suspend;
            socket_.async_write_some(
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
                    );// async write
        } // await suspend
        read::read(asio::ip::tcp::socket& s, std::span<uint8_t> read_buffer)
            :socket_(s),buffer(read_buffer){}
        void read::await_suspend(std::coroutine_handle<> h){
            auto const& buf = buffer;
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
                    );// async read
        } // await suspend
    }//nms await

}//nms cvk_asio
