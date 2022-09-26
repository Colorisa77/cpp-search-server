#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <iomanip>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template<typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_filter) const {
        return FindTopDocuments(raw_query, [status_filter](int document_id, DocumentStatus status, int rating) {return status == status_filter; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

template <typename F>
void RunTestImpl(F& func, const string& str_func) {
    func();
    cerr << str_func << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

template<typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& str_t, const string& str_u, const string& file, const string& func, unsigned line, const string& hint) {
    if (u != t) {
        cerr << boolalpha;
        cerr << file << "(" << line << "): " << func << ": ";
        cerr << "ASSERT_EQUAL(" << str_t << ", " << str_u << ") failed: ";
        cerr << t << " != " << u << ".";
        if (!hint.empty()) {
            cerr << " Hint: " << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, "")
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool val, const string& expr, const string& file, const string& func, unsigned line, const string& hint) {
    if (!val) {
        cerr << boolalpha;
        cerr << file << "(" << line << "): " << func << ": ";
        cerr << "ASSERT(" << expr << ") failed.";
        if (!hint.empty()) {
            cerr << " Hint: " << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, "")
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestAddingDocuments() {
    const int doc_id = 1;
    const string content = "red tomato in the bubble"s;
    const vector<int> rating = { 15, 10, 14 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, rating);
        auto testing = server.FindTopDocuments(content);
        ASSERT_EQUAL(testing[0].id, doc_id);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
    }
}

void TestMinusWords() {
    const int first_doc_id = 5;
    const string first_content = "red tomato in the bubble"s;
    const vector<int> first_rating = { 15, 10, 14 };

    const int second_doc_id = 14;
    const string second_content = "blue tomato outside of the bucket"s;
    const vector<int> second_rating = { 5, 12, 11 };
    {
        SearchServer server;
        server.AddDocument(first_doc_id, first_content, DocumentStatus::ACTUAL, first_rating);
        server.AddDocument(second_doc_id, second_content, DocumentStatus::ACTUAL, second_rating);
        auto found_doc = server.FindTopDocuments("tomato -blue"s);
        ASSERT_EQUAL(found_doc[0].id, first_doc_id);
        found_doc = server.FindTopDocuments("red and blue -tomato"s);
        ASSERT(found_doc.empty());
    }
}

void TestMatchingWords() {
    const int doc_id = 5;
    const string content = "red tomato in the bubble"s;
    const vector<int> rating = { 15, 10, 14 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, rating);
        vector<string> matched_words;
        DocumentStatus status;
        tie(matched_words, status) = server.MatchDocument("red tomato", doc_id);
        ASSERT_EQUAL(matched_words.size(), 2);
        tie(matched_words, status) = server.MatchDocument("red bubble -tomato", doc_id);
        ASSERT_EQUAL(matched_words.size(), 0);
    }
}

void TestDocumentRelevationSort() {
    const int first_doc_id = 5;
    const string first_content = "red tomato in the bubble"s;
    const vector<int> first_rating = { 15, 10, 14 };

    const int second_doc_id = 14;
    const string second_content = "blue tomato outside of the bucket"s;
    const vector<int> second_rating = { 5, 12, 11 };

    const int third_doc_id = 28;
    const string third_content = "cat eating tomato"s;
    const vector<int> third_rating = { 2, 4, 8 };
    {
        SearchServer server;
        server.SetStopWords("in, the, of, outside");
        server.AddDocument(first_doc_id, first_content, DocumentStatus::ACTUAL, first_rating);
        server.AddDocument(second_doc_id, second_content, DocumentStatus::ACTUAL, second_rating);
        server.AddDocument(third_doc_id, third_content, DocumentStatus::ACTUAL, third_rating);
        auto found_doc = server.FindTopDocuments("cat eating red tomato");
        ASSERT_EQUAL(found_doc[0].id, third_doc_id);
        ASSERT_EQUAL(found_doc[1].id, first_doc_id);
        ASSERT_EQUAL(found_doc[2].id, second_doc_id);
        ASSERT((found_doc[0].relevance > found_doc[1].relevance) && (found_doc[0].relevance > found_doc[2].relevance));
        ASSERT((found_doc[1].relevance > found_doc[2].relevance));
    }
}

void TestDocumentRating() {
    const int doc_id = 28;
    const string content = "cat eating tomato"s;
    const vector<int> rating = { 2, 4, 8 };
    int rating_sum = 14;
    double doc_rating = rating_sum / static_cast<int>(rating.size());
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, rating);
        auto found_doc = server.FindTopDocuments("cat"s);
        ASSERT(found_doc[0].rating == doc_rating);
    }
}

void TestByDocumentPredicateFilter() {
    const int first_doc_id = 5;
    const string first_content = "red tomato in the bubble"s;
    const vector<int> first_rating = { 15, 10, 14 };

    const int second_doc_id = 14;
    const string second_content = "blue tomato outside of the bucket"s;
    const vector<int> second_rating = { 5, 12, 11 };

    const int third_doc_id = 28;
    const string third_content = "cat eating tomato"s;
    const vector<int> third_rating = { 2, 4, 8 };
    {
        SearchServer server;
        server.AddDocument(first_doc_id, first_content, DocumentStatus::ACTUAL, first_rating);
        server.AddDocument(second_doc_id, second_content, DocumentStatus::IRRELEVANT, second_rating);
        server.AddDocument(third_doc_id, third_content, DocumentStatus::REMOVED, third_rating);
        auto found_doc = server.FindTopDocuments("tomato", DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_doc.size(), 1);
        ASSERT_EQUAL(found_doc[0].id, first_doc_id);
        found_doc = server.FindTopDocuments("tomato", DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_doc.size(), 1);
        ASSERT_EQUAL(found_doc[0].id, second_doc_id);
        found_doc = server.FindTopDocuments("tomato", DocumentStatus::BANNED);
        ASSERT_EQUAL(found_doc.size(), 0);
        ASSERT(found_doc.empty());
        found_doc = server.FindTopDocuments("tomato", DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_doc.size(), 1);
        ASSERT_EQUAL(found_doc[0].id, third_doc_id);
    }
}

void TestCorrectDocumentRelevation() {
    SearchServer server;
    server.AddDocument(22, "house cristall and gold"s, DocumentStatus::ACTUAL, { 1,2,3 });
    server.AddDocument(23, "car green sky house"s, DocumentStatus::ACTUAL, { 1,2,3 });
    server.AddDocument(24, "yellow black white house"s, DocumentStatus::ACTUAL, { 1,2,3 });
    const auto found_docs = server.FindTopDocuments("cristall house gold"s);
    double ans1, ans2, ans3;
    ans1 = 2.0 * log(3.0) * (0.25) + log(1.0) * (0.25);
    ans2 = log(1.0) * (0.25);
    ans3 = log(3.0) * (0.25) + log(1.0) * (0.25);
    ASSERT_EQUAL(found_docs[0].relevance, ans1);
    ASSERT_EQUAL(found_docs[1].relevance, ans2);
    const auto found_docs1 = server.FindTopDocuments("house green"s);
    ASSERT_EQUAL_HINT(found_docs1[0].relevance, ans3, "Error");
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddingDocuments);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchingWords);
    RUN_TEST(TestDocumentRelevationSort);
    RUN_TEST(TestDocumentRating);
    RUN_TEST(TestByDocumentPredicateFilter);
    RUN_TEST(TestCorrectDocumentRelevation);
}

// --------- Окончание модульных тестов поисковой системы -----------

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

int main() {
    TestSearchServer();
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; })) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}