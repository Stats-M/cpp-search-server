#include <cmath>
#include <numeric>
#include <algorithm>

#include "string_processing.h"
#include "search_server.h"
#include "read_input_functions.h"
//#include "log_duration.h"


SearchServer::SearchServer(const std::string& stop_words_text)  // Invoke delegating constructor
    : SearchServer(SplitIntoWordsView(stop_words_text))             // from string container
{}


SearchServer::SearchServer(const std::string_view stop_words_view)  // Invoke delegating constructor
    : SearchServer(SplitIntoWordsView(stop_words_view))           // from string container
{}


void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
                               const std::vector<int>& ratings)
{
    using namespace std::string_literals;

    if ((document_id < 0) || (documents_.count(document_id) > 0))
    {
        throw std::invalid_argument("Invalid document_id"s);
    }

    const auto [it, inserted] = documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, std::string(document) });
    const auto words = SplitIntoWordsNoStop(it->second.doc_text);

    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words)
    {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        // Заполняем дополнительный словарь для подсистемы поиска дубликатов (кэш подсистемы)
        document_to_words_[document_id][word] = word_to_document_freqs_[word][document_id];
    }
    document_ids_.push_back(document_id);
}


std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::seq,
                            raw_query, [status](int document_id, DocumentStatus document_status, int rating)
                            {
                                return document_status == status;
                            });
}


std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const
{
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}


int SearchServer::GetDocumentCount() const
{
    return documents_.size();
}


int SearchServer::GetDocumentId(int index) const
{
    return document_ids_.at(index);
}


std::vector<int>::const_iterator SearchServer::begin()
{
    return document_ids_.cbegin();
}


std::vector<int>::const_iterator SearchServer::end()
{
    return document_ids_.cend();
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query,
                                                                                      int document_id) const
{
    using namespace std::string_literals;
    if (!documents_.count(document_id))
    {
        throw std::out_of_range("Invalid document_id"s);
    }

    // MatchDocument() без политики - вызываем последовательную версию
    const auto query = ParseQuery(std::execution::seq, raw_query);

    // Сначала проверим минус-слова.
    for (std::string_view word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            // Минус-слово из запроса есть в документе. Выходим с пустым результатом.
            return { {}, documents_.at(document_id).status };
        }
    }

    std::vector<std::string_view> matched_words;

    for (std::string_view word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.push_back(word);
        }
    }

    return { std::vector<std::string_view>(matched_words.begin(), matched_words.end()), documents_.at(document_id).status };
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument([[maybe_unused]] std::execution::sequenced_policy policy,
                                                                                      std::string_view raw_query,
                                                                                      int document_id)
{
    // Тело метода дублирует код метода без политик выполнения
    return SearchServer::MatchDocument(raw_query, document_id);
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy policy,
                                                                                      std::string_view raw_query,
                                                                                      int document_id)
{
    using namespace std::string_literals;
    if (!documents_.count(document_id))
    {
        throw std::out_of_range("Invalid document_id"s);
    }

    // Получаем векторы плюс- и минус-слов параллельным алгоритмом
    const auto query = ParseQuery(policy, raw_query);

    // Проверяем, есть ли среди минус-слов хотя бы 1, входящее в текущий документ
    if (std::any_of(std::execution::par, query.minus_words.cbegin(), query.minus_words.cend(),
                    [this, document_id](std::string_view word)
                    {
                        const auto it = word_to_document_freqs_.find(word);
                        // Если минус слово есть среди слов сервера И в заданном документе это слово встечается (== 1)  => true
                        return ((it != word_to_document_freqs_.end()) && (it->second.count(document_id)));
                    })
        )
    {
        // В запросе есть хотя бы 1 минус-слово, встречающееся в текущем документе.
        // Возвращаем пустой ответ
        return { {}, documents_.at(document_id).status };
    }

                    ///////////////////////////////////////////////
                    // Если мы здесь, то минус-слов в документе нет

                    // Пустой вектор совпавших слов
                    std::vector<std::string_view> matched_words{};
                    // Резервируем память (не более чем количество плюс-слов в запросе)
                    matched_words.reserve(query.plus_words.size());

                    // Матчинг плюс-слов, версия 2 для последовательной реализации (другой словарь)
                    const auto& this_doc_words = document_to_words_.at(document_id);
                    for (std::string_view word : query.plus_words)
                    {
                        if (this_doc_words.count(word))
                        {
                            matched_words.push_back(word);
                        }
                    }

                    // Удаляем дубликаты из результатов матчинга (версия для последовательной реализации)
                    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
                    auto last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
                    last = matched_words.erase(last, matched_words.end());

                    return { matched_words, documents_.at(document_id).status };
}


void SearchServer::RemoveDocument(int document_id)
{
    // Сначала проверяем есть ли документ с таким id. Проверять будем через быстрый map<>
    if (documents_.count(document_id) == 0)
    {
        // такого документа нет, выходим
        return;
    }

    // Считаем что данные в контейнерах корректны и если id присутствует в documents_,
    // то такой документ есть и в остальных контейнерах. Удаляем отовсюду.
    documents_.erase(document_id);
    // erase-remove для вектора
    auto new_end_it = std::remove(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(new_end_it, document_ids_.end());

    for (auto& [word, submap] : word_to_document_freqs_)
    {
        if (submap.count(document_id) > 0)
        {
            // Текущее word присутствует в указанном документе document_id. Удаляем.
            submap.erase(document_id);
            // Если для слова word больше нет ссылок, удаляем и само слово
            // СЛЕДУЮЩИЙ КОД вызывает провал проверки задания тренажером, отключено
//            if (submap.size() == 0)
//            {
//                word_to_document_freqs_.erase(word);
//            }
        }
    }

    document_to_words_.erase(document_id);
}


const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    // Статические переменные инициализируются при первом обращении, а потом просто используются
    static std::map<std::string_view, double> word_freqs_;
    word_freqs_.clear();

    if (documents_.count(document_id) > 0)
    {
        word_freqs_ = document_to_words_.at(document_id);
    }

    return word_freqs_;
}

SearchServer::Query::Query(size_t size_plus, size_t size_minus) : plus_words(size_plus), minus_words(size_minus)
{}

SearchServer::Query::Query(size_t size) : plus_words(size), minus_words(size)
{}

void SearchServer::Query::SortUniq(bool sort_plus)
{
    if (sort_plus)
    {
        // Сортировка словаря плюс-слов
        std::sort(std::execution::par, plus_words.begin(), plus_words.end());
        auto last = std::unique(std::execution::par, plus_words.begin(), plus_words.end());
        last = plus_words.erase(last, plus_words.end());
    }
    else
    {
        // Сортировка словаря минус-слов
        std::sort(std::execution::par, minus_words.begin(), minus_words.end());
        auto last = std::unique(std::execution::par, minus_words.begin(), minus_words.end());
        last = minus_words.erase(last, minus_words.end());
    }
}

void SearchServer::Query::SortUniq()
{
    SortUniq(true);
    SortUniq(false);

    // Сортировка словаря плюс-слов
//    std::sort(std::execution::par, plus_words.begin(), plus_words.end());
//    auto last = std::unique(std::execution::par, plus_words.begin(), plus_words.end());
//    last = plus_words.erase(last, plus_words.end());

    // Сортировка словаря минус-слов
//    std::sort(std::execution::par, minus_words.begin(), minus_words.end());
//    last = std::unique(std::execution::par, minus_words.begin(), minus_words.end());
//    last = minus_words.erase(last, minus_words.end());
}


bool SearchServer::IsStopWord(std::string_view word) const
{
    //return stop_words_.count(word.data()) > 0;
    return stop_words_.count(word) > 0;
}


bool SearchServer::IsValidWord(std::string_view word)
{
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c)
                        {
                            return c >= '\0' && c < ' ';
                        });
}


std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const
{
    using namespace std::string_literals;

    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWordsView(text))
    {
        if (!IsValidWord(word))
        {
            throw std::invalid_argument("Word "s + word.data() + " is invalid"s);
        }
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    return words;
}


int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}


SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
{
    using namespace std::string_literals;

    if (text.empty())
    {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-')
    {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word))
    {
        throw std::invalid_argument("Query word "s + text.data() + " is invalid"s);
    }

    return { word, is_minus, IsStopWord(word) };
}


// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const
{
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
