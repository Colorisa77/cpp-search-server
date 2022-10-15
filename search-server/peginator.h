#pragma once
#include <vector>
#include <iostream>
#include "document.h"

template<typename It>
class ItRange {
public:
    ItRange(It begin, It end) {
        for (auto iter = begin; iter != end;) {
            doc_.push_back(*iter);
            advance(iter, 1);
        }
    }

    It begin() const {
        return doc_.begin();
    }

    It end() const {
        return doc_.end();
    }

    size_t size() {
        return doc_.size();
    }

private:
    std::vector<Document> doc_;
};

template<typename It>
class Paginator {
public:
    Paginator(It begin, It end, size_t page_size) {
        for (auto iter = begin; iter != end;) {
            size_t curr_page_size = distance(iter, end);
            if (page_size <= curr_page_size) {
                pages_.push_back(ItRange(iter, next(iter, page_size)));
                advance(iter, page_size);
            }
            else {
                pages_.push_back(ItRange(iter, next(iter, curr_page_size)));
                advance(iter, curr_page_size);
            }
        }
    }

    template<typename Container>
    It begin(Container c) {
        return c.begin();
    }

    template<typename Container>
    It end(Container c) {
        return c.begin();
    }

    size_t size() const {
        return pages_.size();
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

private:
    std::vector<ItRange<It>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}


template<typename It>
std::ostream& operator<<(std::ostream& os, const ItRange<It>& page) {
    for (auto iterator = page.begin(); iterator != page.end(); ++iterator) {
        os << *iterator;
    }
    return os;
}