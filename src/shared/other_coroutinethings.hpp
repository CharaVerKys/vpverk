//todo name file better

#pragma once

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <coroutine>
#include <cussert.hpp>
#include <optional>

// should be initialized before any use of coroutine in current thread, otherwise will lead to errors
inline thread_local std::optional<asio::io_context*> t_ctx = std::nullopt;

#define RESCHEDULE_HERE(h) cussert(t_ctx.has_value()); asio::post(**t_ctx, [h]{ cussert(not h.done()); h.resume(); });

// no nms for easier usage, if any conflict just wrap this block yourself
// co_await reschedule{};
struct reschedule : std::suspend_always {
    void await_suspend(std::coroutine_handle<> h) const {
        // on current thread context
        RESCHEDULE_HERE(h);
    }
};
using resched = reschedule; //optional
// co_await resched{};


// it isnt belong to future, it possible for current future implementation only because of t_ctx
#include <hpp/future.h>
template<cvk::FutureValue T>
auto operator co_await(cvk::future<T> future) noexcept
    requires(not std::is_reference_v<T>)
{
    struct awaiter : cvk::future<T>
    {
        //? implicit move constructor not works
        awaiter(cvk::future<T>&&fut):cvk::future<T>(std::move(fut)){}
        bool await_ready()noexcept{
            cussert(t_ctx);
            return false;}
        void await_suspend(std::coroutine_handle<>cont){
            cvk::future<T>::subscribe([this,cont](tl::expected<T,std::exception_ptr>&& expected){
                result = std::move(expected);
                cont();
            },**t_ctx);
        }
        T await_resume(){
            if(result.has_value()){
                return std::move(result.value());
            }
            std::rethrow_exception(result.error());
        }
      private:
        tl::expected<T,std::exception_ptr> result;
    };
    return awaiter{std::move(future)};
}


