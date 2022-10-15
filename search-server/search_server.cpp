#include "search_server.h"

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0 || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("SearchServer::AddDocument, invalid document id");
    }
    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    sorted_document_id_.push_back(document_id);
    std::sort(sorted_document_id_.begin(), sorted_document_id_.end());
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus filter_status) const {
    return FindTopDocuments(raw_query, [filter_status](int document_id, DocumentStatus status, int rating) { return status == filter_status; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const { return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL); }

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

int SearchServer::GetDocumentId(int index) {
    if (index > documents_.size() || index < 0) {
        throw std::out_of_range("SearchServer::GetDocumentId, out of range");
    }
    else {
        return sorted_document_id_.at(index);
    }
    throw std::out_of_range("SearchServer::GetDocumentId, out of range");
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return tie(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
        if (text[0] == '-' || text.empty()) {
            throw std::invalid_argument("empty or incorrect minus word");
        }
    }
    return { text, is_minus, SearchServer::IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("invalid character(s)");
        }
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!SearchServer::IsStopWord(word)) {
            if (!SearchServer::IsValidWord(word)) {
                throw std::invalid_argument("invalid character(s)");
            }
            words.push_back(word);
        }
    }
    return words;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}