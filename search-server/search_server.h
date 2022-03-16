#pragma once

// #include для type resolution в объявлениях функций:
#include <string>
#include <string_view>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <vector>
#include <tuple>
#include <map>
#include <iterator>
#include <execution>    // для std::execution::parallel_policy
#include <mutex>
#include <type_traits>
#include <future>

#include <ostream>      // для тестов
#include <iostream>     // для тестов
#include <ios>          // для тестов

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;

// Константа точности сравнения вещественных чисел (значений релевантности)
const double EPSILON = 1e-6;

// Число корзин для разбиения многопоточных словарей
const size_t BUCKETS_NUM = 8;

class SearchServer
{
public:
    // Шаблонный конструктор на основе контейнера со стоп-словами
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    // Конструктор на основе строки со стоп-словами (вызывает шаблонный конструктор)
    explicit SearchServer(const std::string&);

    // Конструктор на основе string_view со стоп-словами (вызывает шаблонный конструктор)
    explicit SearchServer(const std::string_view);

    // Метод добавляет новый документ в базу данных поискового сервера
    void AddDocument(int, std::string_view, DocumentStatus, const std::vector<int>&);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view,
                                           DocumentPredicate) const;
    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&,
                                           std::string_view,
                                           DocumentPredicate) const;

    std::vector<Document> FindTopDocuments(std::string_view, DocumentStatus) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view, DocumentStatus) const;

    std::vector<Document> FindTopDocuments(std::string_view) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view) const;

    int GetDocumentCount() const;

    int GetDocumentId(int) const;

    std::vector<int>::const_iterator begin();

    std::vector<int>::const_iterator end();

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view, int) const;

    // Версия MatchDocument() для политики последовательного выполнения
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, std::string_view, int);

    // Версия MatchDocument() для политики параллельного выполнения
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, std::string_view, int);

    // Метод удаляет документ под указанным id изо всех контейнеров
    void RemoveDocument(int);

    // Версия RemoveDocument() с поддержкой параллельного выполнения
    template <class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&&, int);

    // Метод возвращает словарь частоты слов для документа с указанным id
    const std::map<std::string_view, double>& GetWordFrequencies(int) const;

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
        std::string doc_text;   // Исходные строки документа. На их основе конструируются string_view
    };

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    // Структура запроса для обычных и последовательных алгоритмов
    struct Query
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;

        Query() = default;

        explicit Query(size_t, size_t);

        explicit Query(size_t);

        // Выполняет сортировку и оставление уникальных значений в векторе (эмуляция set).
        // true для сортировки плюс-слов, false для сортировки минус-слов
        void SortUniq(bool);

        // Выполняет сортировку и оставление уникальных значений в векторе (эмуляция set).
        void SortUniq();
    };

    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;

    //NEW
    // Словарь "номер документа - словарь частоты его слов"
    std::map<int, std::map<std::string_view, double>> document_to_words_;

    bool IsStopWord(std::string_view) const;

    static bool IsValidWord(std::string_view);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view) const;

    static int ComputeAverageRating(const std::vector<int>&);

    QueryWord ParseQueryWord(std::string_view) const;

    template <class ExecutionPolicy>
    Query ParseQuery(ExecutionPolicy&&, std::string_view) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    // Специализированный шаблон для последовательного выполнения
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, 
                                           const Query&,
                                           DocumentPredicate) const;
    // Специализированный шаблон для параллельного выполнения
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, 
                                           const Query&,
                                           DocumentPredicate) const;
    // Версия шаблона для вызова без указания политики выполнения (вызывает seq-версию)
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query&,
                                           DocumentPredicate) const;
};


// Определения шаблонных методов (вынесены вне класса)

template <typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(const ExecutionPolicy& policy, ForwardRange& range, Function function)
{
    if constexpr (
        std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>
        || std::is_same_v<typename std::iterator_traits<typename ForwardRange::iterator>::iterator_category,
        std::random_access_iterator_tag>
        )
    {
        std::for_each(policy, range.begin(), range.end(), function);
    }
    else
    {
        static constexpr int PART_COUNT = 16;
        const auto part_length = size(range) / PART_COUNT;
        auto part_begin = range.begin();
        auto part_end = next(part_begin, part_length);

        std::vector<std::future<void>> futures;
        for (int i = 0;
             i < PART_COUNT;
             ++i,
             part_begin = part_end,
             part_end = (i == PART_COUNT - 1
                         ? range.end()
                         : next(part_begin, part_length))
             )
        {
            futures.push_back(async([function, part_begin, part_end]
                                    {
                                        for_each(part_begin, part_end, function);
                                    }));
        }
    }
}


template <typename ForwardRange, typename Function>
void ForEach(ForwardRange& range, Function function)
{
    ForEach(std::execution::seq, range, function);
}


template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    using namespace std::string_literals;

    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord))
    {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }

}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const
{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}


template <class ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy,
                                                     std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const
{
    const auto query = ParseQuery(policy, raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
              [](const Document& lhs, const Document& rhs)
              {
                  return lhs.relevance > rhs.relevance
                      || (std::abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
              });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}


template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(policy,
                            raw_query, [status](int document_id, DocumentStatus document_status, int rating)
                            {
                                return document_status == status;
                            });
}


template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const
{
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}


template <typename ExecutionPolicy>
SearchServer::Query SearchServer::ParseQuery(ExecutionPolicy&& policy, std::string_view text) const
{
    // Разбиваем запрос на слова
    const std::vector<std::string_view> query_words = SplitIntoWordsView(text);
    // Резервируем память только для плюс-слов
    SearchServer::Query result;
    result.plus_words.reserve(query_words.size());

    for (std::string_view word : query_words)
    {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                result.minus_words.push_back(query_word.data);
            }
            else
            {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    if constexpr (!std::is_same_v<ExecutionPolicy, std::execution::parallel_policy>)
    {
        result.SortUniq();
    }

    return result;
}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy policy, 
                                                     const SearchServer::Query& query,
                                                     DocumentPredicate document_predicate) const
{
    // Вектор результатов
    std::vector<Document> matched_documents;

    // Стандартный однопоточный словарь
    std::map<int, double> document_to_relevance;

    // Обрабатываем плюс-слова
    for (std::string_view word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating))
            {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    // Обрабатываем минус-слова, удаляем из найденных документы с минус-словами
    for (std::string_view word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    }

    // Заполняем вектор с найденными документами
    for (const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }

    return matched_documents;
}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy policy, 
                                                     const SearchServer::Query& query,
                                                     DocumentPredicate document_predicate) const
{
    // Вектор результатов
    std::vector<Document> matched_documents;

    // Словарь с поддержкой параллельных алгоритмов
    ConcurrentMap<int, double> document_to_relevance(BUCKETS_NUM);

    // Обработка плюс-слов
    // Кастомный алгоритм с улучшенной параллелизацией
    ForEach(policy,
            query.plus_words,
            [this, &document_to_relevance, &document_predicate](std::string_view word)
            {
                // Если плюс-слово есть в словаре частот слов
                if (!word_to_document_freqs_.count(word) == 0)
                {
                    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                    for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
                    {
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating))
                        {
                            document_to_relevance[document_id] += term_freq * inverse_document_freq;
                        }
                    }
                }
            }
    );

    // Обработка минус-слов. Модификация словаря document_to_relevance, полученного по плюс-словам
    // Кастомный алгоритм с улучшенной параллелизацией
    ForEach(policy,
            query.minus_words,
            [this, &document_to_relevance](std::string_view word)
            {
                if (!word_to_document_freqs_.count(word) == 0)
                {
                    for (const auto [document_id, _] : word_to_document_freqs_.at(word))
                    {
                        // Erase у ConcurrentMap потокобезопасный
                        document_to_relevance.Erase(document_id);
                    }
                }
            }
    );

    for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap())
    {
        matched_documents.emplace_back(
            Document ( document_id, relevance, documents_.at(document_id).rating )
        );
    }

    return matched_documents;
}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query,
                                                     DocumentPredicate document_predicate) const
{
    return SearchServer::FindAllDocuments(std::execution::seq, query, document_predicate);
}


template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id)
{
    // Берем только ссылки на слова удаляемого документа
    const auto& word_freqs = document_to_words_.at(document_id);  // map<string, double>

    // Создаём вектор указателей на слова необходимого размера
    std::vector<const std::string*> words(word_freqs.size());
    std::transform(
        policy,
        word_freqs.begin(), word_freqs.end(),
        words.begin(),
        [](const auto& word)
        {
            // word - map<string, double>
            // Кладем указатель на слово
            return &word.first;
        });

    std::mutex mutex_;
    // Удаляем слова, перебирая указатели на них
    std::for_each(policy, words.begin(), words.end(),
                  [this, document_id](const std::string* word)
                  {
                      const std::lock_guard<std::mutex> lock(mutex_);
                      word_to_document_freqs_.at(*word).erase(document_id);
                  });

    documents_.erase(document_id);
    document_to_words_.erase(document_id);

}
