#pragma once

#include "cvkaes.hpp"
#include "hpp/future.h"
#include "tun.hpp"
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>

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
void setupTun(asio::ip::address_v4 const&);
