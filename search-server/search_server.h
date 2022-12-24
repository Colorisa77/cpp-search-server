#pragma once
#include <map>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <execution>
#include <set>
#include <deque>

#include "concurrent_map.h"
#include "string_processing.h"
#include "document.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:

    explicit SearchServer(const std::string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate, typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus filter_status) const;
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy, const std::string_view raw_query, DocumentStatus filter_status) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy, const std::string_view raw_query) const;


    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    using MatchDocumentFn = std::tuple<std::vector<std::string_view>, DocumentStatus>;

    MatchDocumentFn MatchDocument(const std::string_view raw_query, int document_id) const;
    MatchDocumentFn MatchDocument(const std::execution::sequenced_policy& policy, const std::string_view raw_query, int document_id) const;
    MatchDocumentFn MatchDocument(const std::execution::parallel_policy& policy, const std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy& policy, int document_id);
    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    std::deque<std::string> words_;
    std::set<std::string, std::less<>> const stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> word_to_document_freqs_by_id_;
    std::map<int, DocumentData> documents_;
    std::set<int> sorted_document_id_;

    bool IsStopWord(const std::string_view word) const;

    static bool IsValidWord(const std::string_view word) {
        return std::none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;

    };

    SearchServer::QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;

        void EraseDuplicates(std::vector<std::string_view>& words);
        void EraseDuplicates(const std::execution::sequenced_policy& policy, std::vector<std::string_view>& words);
        void EraseDuplicates(const std::execution::parallel_policy& policy, std::vector<std::string_view>& words);
    };

    Query ParseQuery(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& policy, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy& policy, const std::string_view raw_query, DocumentStatus filter_status) const {
    return FindTopDocuments(policy, raw_query, [filter_status](int document_id, DocumentStatus status, int rating) { return status == filter_status; });
}

template<typename Policy>
std::vector<Document> SearchServer::FindTopDocuments(const Policy& policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!std::all_of(stop_words.begin(), stop_words.end(), IsValidWord)) {
        throw std::invalid_argument("SearchServer (constructor): invalid stop word");
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy& policy, const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy& policy, const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(128);
    std::for_each(
        policy,
        query.plus_words.begin(),
        query.plus_words.end(),
        [this, document_predicate, &document_to_relevance](const std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        }
    );

    auto result = document_to_relevance.BuildOrdinaryMap();

    std::for_each(
        policy,
        query.minus_words.begin(),
        query.minus_words.end(),
        [this, document_predicate, &result](const std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    result.erase(document_id);
                }
            }
        }
    );

    std::vector<Document> matched_documents;
    matched_documents.reserve(result.size());

    for (const auto [document_id, relevance] : result) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}