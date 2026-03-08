#pragma once
#include "connection.hpp"
#include <cvkaes.hpp>
#include <asio/ip/address_v4.hpp>
#include <atomic_queue/atomic_queue.h>

struct aq_route{
    bool add_or_remove = false;
    asio::ip::address_v4 addr;
    std::shared_ptr<Connection> con;
    std::shared_ptr<aig::AesSession> aes;
};

namespace routes_q{
unsigned constexpr CAPACITY = 1024*4; // Queue capacity. Since there are more consumers than producers the queue doesn't need to be large.
                        // in case when there nothing on tun, i want handle it? i guess so, but it would be critical issue
                        // i should close connection with some sort of 500 issue and discard


using Element = aq_route*; // Queue element type.
//Element constexpr NIL = static_cast<aq_route>(0ll); // Atomic elements require a special value that cannot be pushed/popped.
Element constexpr NIL = nullptr; // Atomic elements require a special value that cannot be pushed/popped.
}//nms
using routes_queue = atomic_queue::AtomicQueueB<routes_q::Element, std::allocator<routes_q::Element>, routes_q::NIL>; // Use heap-allocated buffer.
