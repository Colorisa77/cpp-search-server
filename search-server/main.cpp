#include "request_queue.h"
#include "process_queries.h"
#include <iostream>
#include <string>
#include <vector>


using namespace std;
void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}
int main() {
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }

    {
        cout << "seq"s << endl;
        for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
            PrintDocument(document);
        }
        std::cout << "=====================" << std::endl;
        for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s)) {
            PrintDocument(document);
        }
        cout << "par"s << endl;
        for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s)) {
            PrintDocument(document);
        }

        
        std::cout<< "=======================================" << std::endl;
        std::cout << "test v2" << std::endl;


        cout << "seq"s << endl;
        for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
            PrintDocument(document);
        }
        cout << "par"s << endl;
        for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, DocumentStatus::BANNED)) {
            PrintDocument(document);
        }

        std::cout << "=======================================" << std::endl;
        std::cout << "test v3" << std::endl;

        cout << "seq"s << endl;
        for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty -cat"s, [](int document_id, DocumentStatus status, int rating) {return document_id % 2 == 0; })) {
            PrintDocument(document);
        }
        cout << "par"s << endl;
        for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty -cat"s, [](int document_id, DocumentStatus status, int rating) {return document_id % 2 == 0; })) {
            PrintDocument(document);
        }

    }




    cout << "ACTUAL by default:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    // параллельная версия
    for (const Document& document : search_server.FindTopDocuments(execution::par, "-yellow"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}