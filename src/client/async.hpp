#pragma once

#include "coroutinesthings.hpp"
#include "cvkaes.hpp"
#include "hpp/future.h"
#include "tun.hpp"
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <stop_token>

namespace cvk{
struct args{
    std::string server_ip = "0";
    std::string login = "0";
    std::string key_path = "0";
    uint16_t port = 0;
};
}

cvk::future<Unit> startAsync(asio::io_context* ctx, cvk::args const& args);
cvk::future<uint32_t> read_block_size(asio::ip::tcp::socket& socket, aig::AesSession&, bool encryption_used = true);
void system_(std::string const&); //abort on error
void setupTun(asio::ip::address_v4 const&);

cvk::coroutine_t startTunRead(std::shared_ptr<asio::ip::tcp::socket> , std::shared_ptr<aig::AesSession>, std::stop_token);
cvk::coroutine_t startSocketRead(std::shared_ptr<asio::ip::tcp::socket> , std::shared_ptr<aig::AesSession>, std::stop_token);
