#include "search_server.h"

#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

//#include "log_duration.h"
#include "process_queries.h"

#include "test_example_functions.h" // for PrintDocument()

using namespace std;




#include <chrono>
#include <string_view>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)

/**
 * Макрос замеряет время, прошедшее с момента своего вызова
 * до конца текущего блока, и выводит в поток std::cerr.
 *
 * Пример использования:
 *
 *  void Task1() {
 *      LOG_DURATION("Task 1"s); // Выведет в cerr время работы функции Task1
 *      ...
 *  }
 *
 *  void Task2() {
 *      LOG_DURATION("Task 2"s); // Выведет в cerr время работы функции Task2
 *      ...
 *  }
 *
 *  int main() {
 *      LOG_DURATION("main"s);  // Выведет в cerr время работы функции main
 *      Task1();
 *      Task2();
 *  }
 */
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

 /**
  * Поведение аналогично макросу LOG_DURATION, при этом можно указать поток,
  * в который должно быть выведено измеренное время.
  *
  * Пример использования:
  *
  *  int main() {
  *      // Выведет время работы main в поток std::cout
  *      LOG_DURATION("main"s, std::cout);
  *      ...
  *  }
  */
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration
{
public:
    // заменим имя типа std::chrono::steady_clock
    // с помощью using для удобства
    using Clock = std::chrono::steady_clock;

    LogDuration(std::string_view id, std::ostream& dst_stream = std::cerr)
        : id_(id)
        , dst_stream_(dst_stream)
    {}

    ~LogDuration()
    {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        dst_stream_ << id_ << ": "sv << duration_cast<milliseconds>(dur).count() << " ms"sv << std::endl;
    }

private:
    const std::string id_;
    const Clock::time_point start_time_ = Clock::now();
    std::ostream& dst_stream_;
};

/*  Версия main() для тестирования корректности функционирования
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
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
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
    for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

   return 0;
}
*/


string GenerateWord(mt19937& generator, int max_length)
{
    const int length = uniform_int_distribution(1, max_length)(generator);
    std::uniform_int_distribution<int> distribution('a', 'z');
    std::string word(length, ' ');
    for (char& c : word)
    {
        c = char(distribution(generator));
    }
    return word;


    /*   НА ЭТУ ВЕРСИЮ ГЕНЕРАТОРА РУГАЕТСЯ VS2019
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i)
    {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
    */
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length)
{
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i)
    {
        words.push_back(GenerateWord(generator, max_length));
    }
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0)
{
    string query;
    for (int i = 0; i < word_count; ++i)
    {
        if (!query.empty())
        {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob)
        {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count)
{
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i)
    {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy)
{
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const string_view query : queries)
    {
        for (const auto& document : search_server.FindTopDocuments(policy, query))
        {
            total_relevance += document.relevance;
        }
    }
    cout << total_relevance << endl;
}

#define TEST(policy) Test(#policy, search_server, queries, execution::policy)

int main()
{
    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i)
    {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, { 1, 2, 3 });
    }

    const auto queries = GenerateQueries(generator, dictionary, 100, 70);

    TEST(seq);
    TEST(par);

    return 0;
}
