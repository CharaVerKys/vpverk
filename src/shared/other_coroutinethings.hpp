//todo name file better

#pragma once

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <coroutine>
#include <cussert.hpp>

// should be initialized before any use of coroutine in current thread, otherwise will lead to errors
inline thread_local std::optional<asio::io_context*> t_ctx = std::nullopt;

// no nms for easier usage, if any conflict just wrap this block yourself
// co_await reschedule{};
struct reschedule : std::suspend_always {
    void await_suspend(std::coroutine_handle<> h) const {
        // on current thread context
        cussert(t_ctx.has_value());
        asio::post(**t_ctx, [h]{ h.resume(); });
    }
};
using resched = reschedule; //optional
// co_await resched{};
