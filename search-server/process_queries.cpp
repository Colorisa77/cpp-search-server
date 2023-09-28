#include "process_queries.h"
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
	std::vector<std::vector<Document>> result(queries.size());
	std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const std::string_view query) {
		return search_server.FindTopDocuments(query);
	});
	return result;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
	std::vector<std::vector<Document>> temp = ProcessQueries(search_server, queries);
	size_t count_size_v = 0;
	int curr_vector = 0;
	while (curr_vector != queries.size()) {
		count_size_v += temp[curr_vector].size();
		++curr_vector;
	}
	std::vector<Document> result;
	result.reserve(count_size_v);
	for (auto it = temp.begin(), end = temp.end(); it != end; ++it) {
		result.insert(result.end(), it->begin(), it->end());
	}
	return result;
} 