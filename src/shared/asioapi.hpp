#pragma once

#include "expected"
#include "hpp/defines.h"
#include "hpp/future.h"
#include <asio/ip/tcp.hpp>


namespace cvk_asio{

    tl::expected<bool,std::error_code> reliable_is_open(asio::ip::tcp::socket& socket);

    cvk::future<tl::expected<Unit,std::error_code>> send(asio::ip::tcp::socket& socket, std::span<const uint8_t>);
    cvk::future<tl::expected<uint32_t/*amount*/,std::error_code>> read_some(asio::ip::tcp::socket& socket, std::span<uint8_t> out_buffer, uint32_t amount = 0 /*0 == max possible */);

    namespace await{
        struct send : std::suspend_always{
        send(asio::ip::tcp::socket&, std::span<const uint8_t> send_buffer);
        using expected_t = tl::expected<uint32_t,std::error_code>;
        void await_suspend(std::coroutine_handle<>);
        [[nodiscard]] expected_t await_resume(){return expected;}
        private: asio::ip::tcp::socket& socket_; expected_t expected; std::span<const uint8_t> buffer_just_to_actually_send_only_on_await_suspend;
        };
        struct read : std::suspend_always{
        read(asio::ip::tcp::socket&, std::span<uint8_t> read_buffer);
        using expected_t = tl::expected<uint32_t,std::error_code>;
        void await_suspend(std::coroutine_handle<>);
        [[nodiscard]] expected_t await_resume(){return expected;}
        private: asio::ip::tcp::socket& socket_; expected_t expected; std::span<uint8_t> buffer;
        };
    }
}//nms

