//#include <algorithm>
//#include <cmath>
//#include <deque>
//#include <iostream>
//#include <map>
//#include <set>
//#include <stdexcept>
//#include <string>
//#include <utility>
//#include <vector>

// NEW INCLUDES
#include "read_input_functions.h"
// string ReadLine();
// int ReadLineWithNumber();

#include "string_processing.h"
// vector<string> SplitIntoWords(const string&)

#include "document.h"
// struct Document;
// ostream& operator<<(ostream&, const Document&);
// enum class DocumentStatus;

#include "search_server.h"
// template <typename StringContainer>
//     set<string> MakeUniqueNonEmptyStrings(const StringContainer&);
// 
// class SearchServer
// {
// public:
//      template <typename StringContainer>
//          explicit SearchServer(const StringContainer& stop_words);
//      explicit SearchServer(const string& stop_words_text);
//      void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);
//      template <typename DocumentPredicate>
//          vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const;
//      vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const;
//      vector<Document> FindTopDocuments(const string& raw_query) const;
//      int GetDocumentCount() const;
//      int GetDocumentId(int index) const;
//      tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const;
// private:
//      struct DocumentData;
//      bool IsStopWord(const string& word) const;
//      static bool IsValidWord(const string& word);
//      vector<string> SplitIntoWordsNoStop(const string& text) const;
//      static int ComputeAverageRating(const vector<int>& ratings);
//      struct QueryWord;
//      QueryWord ParseQueryWord(const string& text) const;
//      struct Query;
//      Query ParseQuery(const string& text) const;
//      double ComputeWordInverseDocumentFreq(const string& word) const;
//      template <typename DocumentPredicate>
//          vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
// }

#include "paginator.h"

#include "request_queue.h"

using namespace std;

void PrintDocument(const Document& document)
{
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status)
{
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words)
    {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document,
                 DocumentStatus status, const vector<int>& ratings)
{
    try
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e)
    {
        cout << "Error in adding document "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query)
{
    cout << "Results for request: "s << raw_query << endl;
    try
    {
        for (const Document& document : search_server.FindTopDocuments(raw_query))
        {
            PrintDocument(document);
        }
    }
    catch (const exception& e)
    {
        cout << "Error is seaching: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query)
{
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
    catch (const exception& e)
    {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
}


int main()
{
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i)
    {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
}
