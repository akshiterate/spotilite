// Playback queue (plans.md Phase 3). Ordered URI list with a current index.
// Full management (remove/reorder) arrives in Phase 9; this covers what the
// Phase 3 player needs: add, navigate, select.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace spotilite {

class Queue {
public:
    Queue() = default;

    void add(const std::string& uri) { uris_.push_back(uri); }
    void clear() {
        uris_.clear();
        index_ = 0;
    }

    bool empty() const { return uris_.empty(); }
    std::size_t size() const { return uris_.size(); }
    std::size_t index() const { return index_; }

    const std::string& current() const {
        static const std::string kEmpty;
        return index_ < uris_.size() ? uris_[index_] : kEmpty;
    }

    // Advance/retreat when possible; false at the ends (or when empty).
    bool next() {
        if (index_ + 1 < uris_.size()) {
            ++index_;
            return true;
        }
        return false;
    }
    bool previous() {
        if (index_ > 0 && !uris_.empty()) {
            --index_;
            return true;
        }
        return false;
    }
    bool select(std::size_t i) {
        if (i < uris_.size()) {
            index_ = i;
            return true;
        }
        return false;
    }

private:
    std::vector<std::string> uris_;
    std::size_t index_ = 0;
};

}  // namespace spotilite
