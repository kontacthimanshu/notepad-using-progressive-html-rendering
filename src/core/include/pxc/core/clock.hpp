#pragma once
// Injectable monotonic clock so timing-dependent behaviour (undo coalescing, budgets) is testable.

#include <chrono>

namespace pxc::core {

struct IClock {
    virtual std::chrono::microseconds now() const = 0;
    virtual ~IClock() = default;
};

class SteadyClock final : public IClock {
public:
    std::chrono::microseconds now() const override {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    }
};

class ManualClock final : public IClock {
public:
    std::chrono::microseconds now() const override { return t_; }
    void advance(std::chrono::microseconds d) { t_ += d; }
    void set(std::chrono::microseconds t) { t_ = t; }

private:
    std::chrono::microseconds t_{0};
};

} // namespace pxc::core
