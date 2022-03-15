#include "test_example_functions.h"

#include <exception>

void PrintDocument(const Document& document)
{
    using namespace std;

    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, std::vector<std::string_view> words, DocumentStatus status)
{
    using namespace std;

    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (std::string_view word : words)
    {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document,
                 DocumentStatus status, const std::vector<int>& ratings)
{
    using namespace std;

    try
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const std::exception& e)
    {
        cout << "Error in adding document "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query)
{
    using namespace std;

    cout << "Results for request: "s << raw_query << endl;
    try
    {
        for (const Document& document : search_server.FindTopDocuments(raw_query))
        {
            PrintDocument(document);
        }
    }
    catch (const std::exception& e)
    {
        cout << "Error is seaching: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const std::string& query)
{
    using namespace std;

    try
    {
        cout << "Matching for request: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index)
        {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const std::exception& e)
    {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
}