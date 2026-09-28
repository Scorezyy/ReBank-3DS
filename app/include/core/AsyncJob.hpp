#pragma once

#include <3ds.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <utility>

class AsyncJob {
public:
    static constexpr std::size_t DefaultStackSize = 512 * 1024;

    explicit AsyncJob(std::size_t stackSize = DefaultStackSize) : stackSize_(stackSize) {}
    AsyncJob(const AsyncJob&) = delete;
    AsyncJob& operator=(const AsyncJob&) = delete;
    ~AsyncJob() { join(); }

    bool start(std::function<void()> fn) {
        if (running()) {
            return false;
        }
        join();
        state_.store(State::Running, std::memory_order_release);
        auto* context = new Context{this, std::move(fn)};
        thread_ = threadCreate(&AsyncJob::trampoline, context, stackSize_, Priority, Core, false);
        if (!thread_) {
            state_.store(State::Idle, std::memory_order_release);
            delete context;
            return false;
        }
        return true;
    }

    bool running() const {
        return state_.load(std::memory_order_acquire) == State::Running;
    }

    bool idle() const {
        return state_.load(std::memory_order_acquire) == State::Idle;
    }

    bool poll() {
        if (state_.load(std::memory_order_acquire) != State::Completed) {
            return false;
        }
        join();
        state_.store(State::Idle, std::memory_order_release);
        return true;
    }

private:
    enum class State { Idle, Running, Completed };
    struct Context {
        AsyncJob* self;
        std::function<void()> fn;
    };

    static void trampoline(void* argument) {
        auto* context = static_cast<Context*>(argument);
        context->fn();
        context->self->state_.store(State::Completed, std::memory_order_release);
        delete context;
    }

    void join() {
        if (thread_) {
            threadJoin(thread_, U64_MAX);
            threadFree(thread_);
            thread_ = nullptr;
        }
    }

    static constexpr int Priority = 0x30;
    static constexpr int Core = -2;

    std::size_t stackSize_;
    Thread thread_ = nullptr;
    std::atomic<State> state_{State::Idle};
};
