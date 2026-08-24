#pragma once

#include <3ds.h>

#include <atomic>
#include <functional>
#include <utility>

// Runs one blocking job on a background thread and hands the result back to
// the main thread via poll(). Replaces the hand-rolled
// Thread+atomic<State>+result triples that used to be duplicated per feature.
template <typename Result>
class AsyncTask {
public:
    AsyncTask() = default;
    AsyncTask(const AsyncTask&) = delete;
    AsyncTask& operator=(const AsyncTask&) = delete;
    ~AsyncTask() { join(); }

    // Starts `fn` on a new thread. Returns false (and does nothing) if a job
    // is already running. Any previous, already-finished thread is joined
    // first so the Thread handle can be reused.
    bool start(std::function<Result()> fn) {
        if (running()) {
            return false;
        }
        join();
        state_.store(State::Running, std::memory_order_release);
        auto* context = new Context{this, std::move(fn)};
        thread_ = threadCreate(&AsyncTask::trampoline, context, StackSize, Priority, Core, false);
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

    // Returns true exactly once, when a finished job's result is ready, and
    // moves it into `out`. Safe to call every frame.
    bool poll(Result& out) {
        if (state_.load(std::memory_order_acquire) != State::Completed) {
            return false;
        }
        join();
        out = std::move(result_);
        state_.store(State::Idle, std::memory_order_release);
        return true;
    }

private:
    enum class State { Idle, Running, Completed };
    struct Context {
        AsyncTask* self;
        std::function<Result()> fn;
    };

    static void trampoline(void* argument) {
        auto* context = static_cast<Context*>(argument);
        context->self->result_ = context->fn();
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

    static constexpr std::size_t StackSize = 128 * 1024;
    static constexpr int Priority = 0x30;
    static constexpr int Core = -2;

    Thread thread_ = nullptr;
    std::atomic<State> state_{State::Idle};
    Result result_{};
};
