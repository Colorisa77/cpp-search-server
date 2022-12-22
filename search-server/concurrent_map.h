#pragma once
#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap support only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> lock;
        Value& ref_to_value;
    };

    constexpr ConcurrentMap(size_t bucket_count) : size_(bucket_count), v_lock_(size_), mp_(size_) {}

    Access operator[](const Key& key) {
        uint64_t k = key % size_;
        return { std::lock_guard(v_lock_[k]), mp_[k][key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (size_t i = 0; i < size_; ++i) {
            v_lock_[i].lock();
            result.insert(mp_[i].begin(), mp_[i].end());
            v_lock_[i].unlock();
        }
        return result;
    }

private:
    size_t size_;
    std::vector<std::mutex> v_lock_;
    std::vector<std::map<Key, Value>> mp_;
};
