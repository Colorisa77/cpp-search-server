#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::set<int> duplicates;
	std::set<std::vector<std::string>> matched_words;
	for (int document_id : search_server) {
		std::vector<std::string> words_in_doc;
		for (auto& [words, _] : search_server.GetWordFrequencies(document_id)) {
			words_in_doc.push_back(words);
		}
		if (matched_words.count(words_in_doc) > 0) {
			duplicates.insert(document_id);
		}
		else {
			matched_words.insert(words_in_doc);
		}
	}
	for (auto id = duplicates.begin(); id != duplicates.end(); ++id) {
		search_server.RemoveDocument(*id);
		std::cout << "Found duplicate document id " << (*id) << "\n";
	}
}