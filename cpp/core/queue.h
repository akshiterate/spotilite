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

    // Row access for list UIs; empty string when out of range.
    const std::string& at(std::size_t i) const {
        static const std::string kEmpty;
        return i < uris_.size() ? uris_[i] : kEmpty;
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

    // Erase row i; false when out of range. The current index follows its
    // track: rows before it shift it down, erasing it keeps the position
    // (now the next track), clamped at the end. Playback itself is
    // untouched — removing the playing track does not stop it.
    bool removeAt(std::size_t i) {
        if (i >= uris_.size()) {
            return false;
        }
        uris_.erase(uris_.begin() + static_cast<std::ptrdiff_t>(i));
        if (uris_.empty()) {
            index_ = 0;
            return true;
        }
        if (i < index_) {
            --index_;
        } else if (index_ >= uris_.size()) {
            index_ = uris_.size() - 1;
        }
        return true;
    }

    // Move row `from` to position `to` (`to` clamped to the end); the
    // current index re-resolves onto the same track. False when `from`
    // is out of range.
    bool move(std::size_t from, std::size_t to) {
        if (from >= uris_.size()) {
            return false;
        }
        if (to >= uris_.size()) {
            to = uris_.size() - 1;
        }
        if (from == to) {
            return true;
        }
        const std::string currentTrack = current();
        std::string uri = uris_[from];
        uris_.erase(uris_.begin() + static_cast<std::ptrdiff_t>(from));
        uris_.insert(uris_.begin() + static_cast<std::ptrdiff_t>(to), uri);
        for (std::size_t k = 0; k < uris_.size(); ++k) {
            if (uris_[k] == currentTrack) {
                index_ = k;
                break;
            }
        }
        return true;
    }

    // Insert at position i (clamped to the end); rows at or after i shift
    // down along with the current index when affected.
    void insertAt(std::size_t i, const std::string& uri) {
        if (i > uris_.size()) {
            i = uris_.size();
        }
        uris_.insert(uris_.begin() + static_cast<std::ptrdiff_t>(i), uri);
        if (i <= index_) {
            ++index_;
        }
    }

private:
    std::vector<std::string> uris_;
    std::size_t index_ = 0;
};

}  // namespace spotilite
