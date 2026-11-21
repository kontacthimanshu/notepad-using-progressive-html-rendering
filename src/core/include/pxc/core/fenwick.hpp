#pragma once
// Fenwick (binary indexed) tree over signed 64-bit values: O(log n) prefix sums and point updates,
// plus O(log n) "find the index whose prefix range contains a value".

#include <cstdint>
#include <span>
#include <vector>

namespace pxc::core {

class Fenwick {
public:
    Fenwick() = default;
    explicit Fenwick(std::span<const std::int64_t> values) { assign(values); }

    void assign(std::span<const std::int64_t> values) {
        n_ = values.size();
        tree_.assign(n_ + 1, 0);
        for (std::size_t i = 0; i < n_; ++i) {
            tree_[i + 1] += values[i];
            const std::size_t j = (i + 1) + ((i + 1) & (~(i + 1) + 1));
            if (j <= n_) tree_[j] += tree_[i + 1];
        }
    }
    std::size_t size() const noexcept { return n_; }

    void add(std::size_t i, std::int64_t delta) {
        for (std::size_t k = i + 1; k <= n_; k += k & (~k + 1)) tree_[k] += delta;
    }
    // Sum of values [0, i).
    std::int64_t prefix(std::size_t i) const {
        std::int64_t s = 0;
        for (std::size_t k = i < n_ ? i : n_; k > 0; k -= k & (~k + 1)) s += tree_[k];
        return s;
    }
    std::int64_t total() const { return prefix(n_); }

    // Largest index i such that prefix(i) <= target (values must be non-negative). Returns n for target >= total.
    std::size_t upperIndex(std::int64_t target) const {
        std::size_t pos = 0;
        std::size_t step = 1;
        while (step * 2 <= n_) step *= 2;
        for (; step > 0; step /= 2) {
            if (pos + step <= n_ && tree_[pos + step] <= target) {
                pos += step;
                target -= tree_[pos];
            }
        }
        return pos; // number of leading elements whose sum <= original target
    }

private:
    std::size_t n_ = 0;
    std::vector<std::int64_t> tree_;
};

} // namespace pxc::core
