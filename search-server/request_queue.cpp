#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
    , emptyResults_(0)
    , current_time_(0)
{}


std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status)
{
    const auto result = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(result.size());
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query)
{
    //const auto result = search_server_.FindTopDocuments(raw_query);
    //AddRequest(result.size());
    //return result;
    
    // Замечание исправлено, см. Fixes.txt
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const
{
    return emptyResults_;
}

void RequestQueue::AddRequest(int results_num)
{
    // новый запрос - новая минута
    ++current_time_;
    // удаляем все результаты поиска, которые устарели
    while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp)
    {
        if (requests_.front().results == 0)
        {
            --emptyResults_;
        }
        requests_.pop_front();
    }
    // сохраняем новый результат поиска
    requests_.push_back({ current_time_, results_num });
    if (results_num == 0)
    {
        ++emptyResults_;
    }
}
