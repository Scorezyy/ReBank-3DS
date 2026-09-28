#pragma once

#include "core/AsyncJob.hpp"

#include <functional>
#include <utility>

template <typename Result>
class AsyncTask {
public:
    static constexpr std::size_t StackSize = 128 * 1024;

    bool start(std::function<Result()> fn) {
        return job_.start([this, fn = std::move(fn)]() { result_ = fn(); });
    }

    bool running() const { return job_.running(); }

    bool poll(Result& out) {
        if (!job_.poll()) {
            return false;
        }
        out = std::move(result_);
        return true;
    }

private:
    Result result_{};
    AsyncJob job_{StackSize};
};
