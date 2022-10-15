#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    auto result = search_server_.FindTopDocuments(raw_query, status);
    RequestQueue::AddRequest(result.size());
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    auto result = search_server_.FindTopDocuments(raw_query);
    RequestQueue::AddRequest(result.size());
    return result;
}

int RequestQueue::GetNoResultRequests() const {
    return no_result_requests_;
}

void RequestQueue::AddRequest(int results_num) {
    ++current_time_;
    while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().time) {
        if (0 == requests_.front().result) {
            --no_result_requests_;
        }
        requests_.pop_front();
    }
    requests_.push_back({ current_time_, results_num });
    if (0 == results_num) {
        ++no_result_requests_;
    }
}
