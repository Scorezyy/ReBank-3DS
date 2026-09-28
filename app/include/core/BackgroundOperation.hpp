#pragma once

#include "core/AsyncJob.hpp"
#include "core/Logger.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <utility>

template <typename Operation, typename Phase>
class BackgroundOperation {
public:
    bool running() const { return job_.running(); }
    bool busy() const { return operation_ != Operation::None; }
    Operation operation() const { return operation_; }
    Phase phase() const { return phase_.load(std::memory_order_acquire); }
    int progress() const { return progress_.load(std::memory_order_acquire); }

protected:
    explicit BackgroundOperation(const char* name) : name_(name) {}
    ~BackgroundOperation() = default;

    bool claim(Operation operation) {
        if (busy()) {
            Logger::instance().warning(std::string(name_) + "::begin: rejected op="
                                       + std::to_string(static_cast<int>(operation)) + ", loader busy with op="
                                       + std::to_string(static_cast<int>(operation_)));
            return false;
        }
        operation_ = operation;
        progress_.store(0, std::memory_order_release);
        return operation != Operation::None;
    }

    void launch(std::function<void()> work) {
        if (!job_.start(std::move(work))) {
            setPhase(Phase::Idle);
            operation_ = Operation::None;
            Logger::instance().error(std::string(name_) + " worker creation failed");
        }
    }

    Operation takeCompleted() {
        if (operation_ == Operation::None || !job_.poll()) {
            return Operation::None;
        }
        setProgress(100);
        setPhase(Phase::Idle);
        return std::exchange(operation_, Operation::None);
    }

    void setPhase(Phase phase) { phase_.store(phase, std::memory_order_release); }
    void setProgress(int progress) { progress_.store(progress, std::memory_order_release); }

private:
    const char* name_;
    AsyncJob job_;
    Operation operation_ = Operation::None;
    std::atomic<Phase> phase_{Phase::Idle};
    std::atomic<int> progress_{0};
};
