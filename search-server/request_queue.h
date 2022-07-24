#pragma once

// #include для type resolution в объявлениях функций:
#include "search_server.h"
#include "document.h"
#include <vector>
#include <string>
#include <deque>

class RequestQueue
{
public:
    explicit RequestQueue(const SearchServer&);

    // "обертки" для всех методов поиска, чтобы сохранять результаты для статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string&, DocumentPredicate);

    std::vector<Document> AddFindRequest(const std::string&, DocumentStatus);

    std::vector<Document> AddFindRequest(const std::string&);

    int GetNoResultRequests() const;

private:
    struct QueryResult
    {
        uint64_t timestamp;
        int results;
    };

    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;
    int emptyResults_;
    uint64_t current_time_;
    const static int min_in_day_ = 1440;

    void AddRequest(int);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate)
{
    const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddRequest(result.size());
    return result;
}
