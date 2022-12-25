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

    constexpr ConcurrentMap(size_t bucket_count) : mp_(bucket_count) {}

    Access operator[](const Key& key) {
        uint64_t k = key % mp_.size();
        return { std::lock_guard(mp_[k].mutex), mp_[k].data[key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (size_t i = 0; i < mp_.size(); ++i) {
            mp_[i].mutex.lock();
            result.insert(mp_[i].data.begin(), mp_[i].data.end());
            mp_[i].mutex.unlock();
        }
        return result;
    }

private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> data;
    };
    std::vector<Bucket> mp_;
};
