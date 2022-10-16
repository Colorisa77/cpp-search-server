#pragma once
#include <stack>

#include "search_server.h"
#include "document.h"

class RequestQueue {
public:
    RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , no_result_requests_(0)
        , current_time_(0) {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        int time;
        int result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int no_result_requests_;
    int current_time_;

    void AddRequest(int results_num);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
    RequestQueue::AddRequest(result.size());
    return result;
}