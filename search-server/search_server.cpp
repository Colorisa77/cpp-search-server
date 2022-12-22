#include "search_server.h"

std::set<int>::const_iterator SearchServer::begin() const {
    return sorted_document_id_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return sorted_document_id_.end();
}

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0 || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("SearchServer::AddDocument, invalid document id");
    }
    words_.emplace_back(document);
    const auto words = SplitIntoWordsNoStop(words_.back());
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_to_document_freqs_by_id_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    sorted_document_id_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus filter_status) const {
    return FindTopDocuments(raw_query, [filter_status](int document_id, DocumentStatus status, int rating) { return status == filter_status; });
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query, DocumentStatus filter_status) const {
    return FindTopDocuments(raw_query, filter_status);
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& policy, const std::string_view raw_query, DocumentStatus filter_status) const {
    return FindTopDocuments(policy, raw_query, [filter_status](int document_id, DocumentStatus status, int rating) { return status == filter_status; });
}


std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& policy, const std::string_view raw_query) const {
    return FindTopDocuments(raw_query);
}
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& policy, const std::string_view raw_query) const {
    return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {

    const SearchServer::Query query = SearchServer::ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { std::vector<std::string_view>{}, documents_.at(document_id).status };
        }
    }
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return tie(matched_words, documents_.at(document_id).status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy& policy, const std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy& policy, const std::string_view raw_query, int document_id) const {
    const SearchServer::Query query = SearchServer::ParseQuery(policy, raw_query);
    if (std::any_of(policy, query.minus_words.begin(), query.minus_words.end(),
        [this, document_id](const std::string_view word) {
            return word_to_document_freqs_by_id_.at(document_id).count(word) > 0;
        })) {
        return { std::vector<std::string_view>{}, documents_.at(document_id).status };
    };

    std::vector<std::string_view> matched_words(query.plus_words.size());

    auto it = std::copy_if(policy,
        query.plus_words.begin(),
        query.plus_words.end(),
        matched_words.begin(),
        [this, document_id](const std::string_view word) {
            return word_to_document_freqs_by_id_.at(document_id).count(word) > 0;
        });
    std::sort(policy, matched_words.begin(), it);
    it = std::unique(policy, matched_words.begin(), it);
    matched_words.erase(it, matched_words.end());
    return tie(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(word) > 0;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::execution::sequenced_policy& policy, std::string_view text) const {
    return ParseQueryWord(text);
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::execution::parallel_policy& policy, std::string_view text) const {
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

SearchServer::Query SearchServer::ParseQuery(const std::string_view text) const {
    Query query;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("invalid character(s)");
        }
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    query.EraseDuplicates(query.minus_words);
    query.EraseDuplicates(query.plus_words);
    return query;
}

SearchServer::Query SearchServer::ParseQuery(const std::execution::sequenced_policy& policy, const std::string_view text) const {
    return ParseQuery(text);
}

SearchServer::Query SearchServer::ParseQuery(const std::execution::parallel_policy& policy, const std::string_view text) const {
    Query query;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("invalid character(s)");
        }
        const QueryWord query_word = ParseQueryWord(policy, word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    query.EraseDuplicates(query.minus_words);
    query.EraseDuplicates(query.plus_words);
    return query;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text)) {
        if (!SearchServer::IsStopWord(word)) {
            if (!SearchServer::IsValidWord(word)) {
                throw std::invalid_argument("invalid character(s)");
            }
            words.emplace_back(word);
        }
    }
    return words;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const {
    return log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (word_to_document_freqs_by_id_.count(document_id) > 0) {
        return word_to_document_freqs_by_id_.at(document_id);
    }
    static const std::map<std::string_view, double> empty_map;
    return empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }
    documents_.erase(document_id);
    sorted_document_id_.erase(document_id);
    auto word_freq = GetWordFrequencies(document_id);
    if (word_freq.empty()) {
        return;
    }
    for (const auto& [word, freq] : word_freq) {
        word_to_document_freqs_[word].erase(document_id);
        if (word_to_document_freqs_[word].empty()) {
            word_to_document_freqs_.erase(word);
        }
    }
    word_to_document_freqs_by_id_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy& policy, int document_id) {
    return RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }
    documents_.erase(document_id);
    sorted_document_id_.erase(document_id);
    const std::map<std::string_view, double>& doc_to_freq = word_to_document_freqs_by_id_.at(document_id);
    std::vector<const std::string_view*> words(doc_to_freq.size());
    std::transform(policy,
        doc_to_freq.begin(),
        doc_to_freq.end(),
        words.begin(),
        [](const std::pair<const std::string_view, double>& word_to_freq) {
            return &word_to_freq.first;
        });
    std::for_each(policy,
        words.begin(),
        words.end(),
        [this, document_id](const std::string_view* del) {
            word_to_document_freqs_[*del].erase(document_id);
        });
    word_to_document_freqs_by_id_.erase(document_id);
}

void SearchServer::Query::EraseDuplicates(std::vector<std::string_view>& words) {
    std::sort(words.begin(), words.end());
    auto last = std::unique(words.begin(), words.end());
    words.resize(std::distance(words.begin(), last));
}

void SearchServer::Query::EraseDuplicates(const std::execution::sequenced_policy& policy, std::vector<std::string_view>& words) {
    return EraseDuplicates(words);
}

void SearchServer::Query::EraseDuplicates(const std::execution::parallel_policy& policy, std::vector<std::string_view>& words) {
    std::sort(policy, words.begin(), words.end());
    auto last = std::unique(policy, words.begin(), words.end());
    words.resize(std::distance(words.begin(), last));
}