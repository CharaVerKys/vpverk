#pragma once

#include "expected"
#include "hpp/future.h"
#include <asio/ip/tcp.hpp>
#include <mutex>

#ifndef Unit_Type
#define Unit_Type
struct Unit{};
#endif

namespace cvk_asio{

    tl::expected<bool,std::error_code> reliable_is_open(asio::ip::tcp::socket& socket);

    cvk::future<tl::expected<Unit,std::error_code>> send(asio::ip::tcp::socket& socket, std::span<const uint8_t>, std::recursive_mutex* socket_call_lock = nullptr);
    cvk::future<tl::expected<Unit,std::error_code>> read_some(asio::ip::tcp::socket& socket, std::span<uint8_t> out_buffer, uint32_t amount = 0 /*0 == max possible */, std::recursive_mutex* socket_call_lock = nullptr);
    cvk::future<tl::expected<uint32_t/*amount*/,std::error_code>> read_some_unreliable(asio::ip::tcp::socket& socket, std::span<uint8_t> out_buffer, uint32_t amount = 0 /*0 == max possible */, std::recursive_mutex* socket_call_lock = nullptr);

    namespace await{
        struct send : std::suspend_always{
        send(asio::ip::tcp::socket&, std::span<const uint8_t> send_buffer, std::recursive_mutex* socket_call_lock = nullptr);
        using expected_t = tl::expected<uint32_t,std::error_code>;
        void await_suspend(std::coroutine_handle<>);
        [[nodiscard]] expected_t await_resume(){return expected;}
        private: asio::ip::tcp::socket& socket_; expected_t expected; std::span<const uint8_t> buffer_just_to_actually_send_only_on_await_suspend; std::recursive_mutex* socket_call_lock;
        };
        struct read : std::suspend_always{
        read(asio::ip::tcp::socket&, std::span<uint8_t> read_buffer, std::recursive_mutex* socket_call_lock = nullptr);
        using expected_t = tl::expected<uint32_t,std::error_code>;
        void await_suspend(std::coroutine_handle<>);
        [[nodiscard]] expected_t await_resume(){return expected;}
        private: asio::ip::tcp::socket& socket_; expected_t expected; std::span<uint8_t> buffer; std::recursive_mutex* socket_call_lock;
        };
    }
}//nms

