#pragma once
#include <coroutine>
namespace cvk{
struct co_getHandle
{
    std::coroutine_handle<> _handle;

    bool await_ready() const noexcept { return false; }
    bool await_suspend(std::coroutine_handle<> handle) noexcept { _handle = handle; return false; }
    [[nodiscard]] std::coroutine_handle<> await_resume() noexcept { return _handle; }
};

// ? get handle with promise_type not tested, but it should work!
template <class promise_type>
struct co_getHandleT // T mean templated
{
    std::coroutine_handle<promise_type> _handle;

    bool await_ready() const noexcept { return false; }
    bool await_suspend(std::coroutine_handle<promise_type> handle) noexcept { _handle = handle; return false; }
    [[nodiscard]] std::coroutine_handle<promise_type> await_resume() noexcept { return _handle; }
};
struct coroutine_t{};
template <typename... Args>
concept NoReferenceArgs = 
    (sizeof...(Args) == 0) or
    []<typename Class, typename... Rest>() {
        return ((not std::is_reference_v<Rest>) and ...);
    }.template operator()<Args...>();
} //nms cvk



#include <exception>
template <typename... Args>
struct std::coroutine_traits<cvk::coroutine_t, Args...> {
    static_assert(cvk::NoReferenceArgs<Args...>,
              "Cannot use references for non-scope lifetime coroutine");
    struct promise_type {
        cvk::coroutine_t get_return_object(){return {};}
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void(){}
        void unhandled_exception(){
            std::terminate();
        }
    };
};

// #if __has_include(<asio/io_context.hpp>)
// #include <asio/io_context.hpp>
// #include "asio/post.hpp"
// #if USE_CHECK_THREAD_ASIO_IN_SWITCH_CONTEXT 
//     #include CHECK_THREAD_HEADER
// #endif
// namespace cvk::details{
// struct MoveToAsioThreadAwaiter : std::suspend_always {
//     MoveToAsioThreadAwaiter(asio::io_context& ctx)
//         : ctx_(ctx) {}
//
//     void await_suspend(std::coroutine_handle<> handle) const {
//         #if USE_CHECK_THREAD_ASIO_IN_SWITCH_CONTEXT 
//         checkThreadIsNOT(&asioThreadID);
//         #endif
//         asio::post(ctx_, [handle]() mutable {
//             handle.resume();
//             #if USE_CHECK_THREAD_ASIO_IN_SWITCH_CONTEXT 
//             checkThread(&asioThreadID);
//             #endif
//         });
//     }
// private:
//     asio::io_context& ctx_;
// };
// }
// #endif
