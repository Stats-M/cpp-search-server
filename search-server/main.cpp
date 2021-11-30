#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <cassert>
#include <sstream>
#include <numeric>
#include <iostream>

using namespace std;

// -------- Подставьте вашу реализацию класса SearchServer сюда ----------

// Максимальное число найденных документов
const int MAX_RESULT_DOCUMENT_COUNT = 5;

// Константа точности сравнения вещественных чисел (значений релевантности)
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);

    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();

    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.emplace(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
                           DocumentData{
                               ComputeAverageRating(ratings),
                               status
                           });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {

        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 // Code reviewer' remark 1/2 - refactored - use a constant instead of hard-typed value
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 }
                 else {
                     return lhs.relevance > rhs.relevance;
                 }
             });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    // Перегруженная версия метода FindTopDocuments, принимающая DocumentStatus
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus target_status) const {
        return FindTopDocuments(raw_query,
                                [target_status](int document_id, DocumentStatus status, int rating) { return status == target_status; });
    }

    // Перегруженная версия метода FindTopDocuments, принимающая только поисковую строку
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {

        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }

        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {

        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }

        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {

        if (ratings.empty()) {
            return 0;
        }

        // Code reviewer' remark 2/2 - refactored - switched to std::accumulate() algorithm
        //        int rating_sum = 0;
        //        for (const int rating : ratings) {
        //            rating_sum += rating;
        //        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {

        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }

        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {

        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }

        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicat) const {

        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (document_predicat(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                                        });
        }

        return matched_documents;
    }
};  // class SearchServer


/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/

// Шаблон для вывода пары перед Print() чтобы его увидел MSVC. В gcc тренажера все ОК.
template <typename T, typename S>
ostream& operator<<(ostream& out, const pair<T, S>& container) {
    out << container.first << ": "s << container.second;
    return out;
}

// Шаблон для вывода контейнера с разделителем элементов
template <typename Document>
ostream& Print(ostream& out, const Document& document) {

    bool bFirst = true;
    for (const auto& element : document) {
        if (bFirst) {
            out << element;
            bFirst = false;
        }
        else {
            out << ", "s << element;
        }
    }

    return out;
}

// Шаблон для вывода словаря
template <typename T, typename S>
ostream& operator<<(ostream& out, const map<T, S>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

// Шаблон для вывода вектора
template <typename T>
ostream& operator<<(ostream& out, const vector<T>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

// Шаблон для вывода набора
template <typename T>
ostream& operator<<(ostream& out, const set<T>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

// Перегрузка оператора вывода для статуса документа
ostream& operator<<(ostream& out, const DocumentStatus& status) {
    switch (status) {
    case (DocumentStatus::ACTUAL):
        out << "Status: ACTUAL"s;
        break;
    case (DocumentStatus::IRRELEVANT):
        out << "Status: IRRELEVANT"s;
        break;
    case (DocumentStatus::BANNED):
        out << "Status: BANNED"s;
        break;
    case (DocumentStatus::REMOVED):
        out << "Status: REMOVED"s;
        break;
    }
    return out;
}

// Шаблон функции проверки равенства двух значений
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

// Шаблон функции проверки bool значения
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// Шаблон функции запуска тестов
template <typename T>
void RunTestImpl(T func, string funcName) {
    func();
    cerr << funcName << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func);


// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Проверка корректности вычисления релевантности найденных документов.
void TestDocumentRelevance() {
    const int doc_id2 = 5;
    const string content2 = "one cat two cat"s;
    const vector<int> ratings2 = { 7, 8, 9 };
    const int doc_id3 = 7;
    const string content3 = "jet from a town"s;
    const vector<int> ratings3 = { 8, 10, 12 };
    const int doc_id4 = 47;
    const string content4 = "cat from a port"s;
    const vector<int> ratings4 = { 8, 10, 12 };
    const double EPSILON = 1e-6;

    {
        SearchServer server;

        // Проверяем случай 2 документов и 1 совпадения (точность расчета)
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        //IDF для "cat" = ln(2 / 1) = 0, 69314718055994530941723212145818
        //TF1 = 2 / 4 = 0.5
        //TF2 = 0 / 4 = 0
        double testRelevance = log(2 / 1) * 0.5 + log(2 / 1) * 0; // = 0,34657359
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_HINT((found_docs[0].relevance - testRelevance) < EPSILON, "Relevance calculation is wrong."s);

        // Добавляем еще документы с совпадениями, проверка сортировки по релевантности
        server.AddDocument(doc_id4, content4, DocumentStatus::ACTUAL, ratings4);
        const auto found_docs2 = server.FindTopDocuments("cat"s);
        ASSERT_HINT(found_docs2[0].relevance > found_docs2[1].relevance, "Documents must be sorted by relevance in descending order."s);
    }
}

// Проверка корректности матчинга документов.
void TestDocumentMatching() {
    const int doc_id1 = 0;
    const string content1 = "cat in the city"s;
    const vector<int> ratings1 = { 1, 2, 3 };
    const int doc_id2 = 1;
    const string content2 = "one cat two cat"s;
    const vector<int> ratings2 = { 7, 8, 9 };
    const int doc_id3 = 2;
    const string content3 = "jet from a port"s;
    const vector<int> ratings3 = { 8, 10, 12 };

    {
        SearchServer server;
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

        const auto [words, status] = server.MatchDocument("jet"s, doc_id3);
        ASSERT_EQUAL_HINT(words.size(), 1u, "1 document should be found"s);
        ASSERT_EQUAL(words[0], "jet"s);
        ASSERT_EQUAL(doc_id3, 2);
        ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

        const auto [words, status] = server.MatchDocument("cat city"s, doc_id1);
        ASSERT_EQUAL_HINT(words.size(), 2u, "2 documents should be found"s);
        ASSERT_EQUAL(words[0], "cat"s);
        ASSERT_EQUAL(words[1], "city"s);
        ASSERT_EQUAL(doc_id1, 0);
        ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

        const auto [words, status] = server.MatchDocument("dog"s, doc_id2);
        ASSERT_EQUAL_HINT(words.size(), 0u, "No documents should be found"s);
    }
}

// Проверка корректности добавления документов + поиск.
void TestDocumentAdd() {
    const int doc_id2 = 45;
    const string content2 = "one cat two cat"s;
    const vector<int> ratings2 = { 7, 8, 9 };
    const int doc_id3 = 47;
    const string content3 = "jet from a town"s;
    const vector<int> ratings3 = { 8, 10, 12 };

    {
        SearchServer server;

        // Ни одного документа пока не добавлено
        const auto found_docs = server.FindTopDocuments("two"s);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 0, "0 documents has been added so far"s);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);

        // 1 документ добавлен, ожидается только 1 совпадение
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1, "1 document has been added so far"s);
        const auto found_docs1 = server.FindTopDocuments("two"s);
        ASSERT_EQUAL(found_docs1.size(), 1u);
        const auto found_docs2 = server.FindTopDocuments("jet"s);
        ASSERT_EQUAL(found_docs2.size(), 0u);

        // 2 документа добавлены, ожидается 2 совпадения
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 2, "2 documents have been added so far"s);
        const auto found_docs3 = server.FindTopDocuments("two"s);
        ASSERT_EQUAL(found_docs3.size(), 1u);
        const auto found_docs4 = server.FindTopDocuments("jet"s);
        ASSERT_EQUAL(found_docs4.size(), 1u);
    }
}

// Проверка корректности учета минус слов
void TestMinusWords() {
    const int doc_id1 = 43;
    const string content1 = "one dog two dog"s;
    const vector<int> ratings1 = { 1, 2, 3 };
    const int doc_id2 = 45;
    const string content2 = "one cat two cat"s;
    const vector<int> ratings2 = { 7, 8, 9 };
    const int doc_id3 = 47;
    const string content3 = "jet cat from a port"s;
    const vector<int> ratings3 = { 8, 10, 12 };

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

        // Постепенно уменьшаем количество совпадений, добавляя все больше стоп-слов
        const auto found_docs1 = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL_HINT(found_docs1.size(), 2u, "2 documents should be matched"s);
        const auto found_docs2 = server.FindTopDocuments("cat -jet"s);
        ASSERT_EQUAL_HINT(found_docs2.size(), 1u, "1 document should be matched"s);
        const auto found_docs3 = server.FindTopDocuments("cat -jet -two"s);
        ASSERT_EQUAL_HINT(found_docs3.size(), 0u, "0 documents should be matched"s);
    }
}

// Проверка корректности расчета рейтинга документов
void TestRatingCalc() {

    SearchServer server;
    server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 4, 7, 9, 5 });
    const auto found_docs = server.FindTopDocuments("кот"s);
    const int testRating = (4 + 7 + 9 + 5) / 4;    // ==6 (округлено)
    ASSERT_EQUAL(found_docs[0].rating, testRating);
}

// Проверка фильтрации документов при помощи предиката
void TestPredicate() {
    const int doc_id1 = 43;
    const string content1 = "one dog two dog"s;
    const vector<int> ratings1 = { 1, 2, 3 };       // рейтинг == 2
    const int doc_id2 = 45;
    const string content2 = "one cat two cat"s;
    const vector<int> ratings2 = { 7, 8, 9 };
    const int doc_id3 = 47;
    const string content3 = "dog from a port"s;
    const vector<int> ratings3 = { 2, 4, 6 };       // рейтинг == 4
    const int doc_id4 = 49;
    const string content4 = "dog with a collar"s;
    const vector<int> ratings4 = { 2, 3, 4 };       // рейтинг == 3
    const int doc_id5 = 51;
    const string content5 = "green snake with yellow head"s;
    const vector<int> ratings5 = { 8, 10, 12 };

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
        server.AddDocument(doc_id4, content4, DocumentStatus::ACTUAL, ratings4);
        server.AddDocument(doc_id5, content5, DocumentStatus::ACTUAL, ratings5);

        // Найдем документы, содержащие слово "dog" (совпадений должно быть 3 из 5)...
        // ... и из них предикатом выберем только те, чей рейтинг равен 3 или выше (совпадений должно быть 2 из найденных 3)
        // В итоговую выборку должны попасть документы doc_id3, doc_id4
        const auto found_docs = server.FindTopDocuments("dog"s, 
                                    [](int document_id, DocumentStatus status, int rating) { return rating >= 3; });
        ASSERT_EQUAL_HINT(found_docs.size(), 2, "2 documents should be matched"s);
        ASSERT_EQUAL(found_docs[0].id, doc_id3);
        ASSERT_EQUAL(found_docs[1].id, doc_id4);
    }
}

// Проверка поиска документов с заданным статусом
void TestStatusMatching() {
    const int doc_id1 = 43;
    const string content1 = "one dog two dog"s;
    const vector<int> ratings1 = { 1, 2, 3 };
    const int doc_id2 = 45;
    const string content2 = "one cat two cat"s;
    const vector<int> ratings2 = { 7, 8, 9 };
    const int doc_id3 = 47;
    const string content3 = "dog from a port"s;
    const vector<int> ratings3 = { 2, 4, 6 };
    const int doc_id4 = 49;
    const string content4 = "dog with a collar"s;
    const vector<int> ratings4 = { 2, 3, 4 };
    const int doc_id5 = 51;
    const string content5 = "green snake with yellow head"s;
    const vector<int> ratings5 = { 8, 10, 12 };

    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::BANNED, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings3);
        server.AddDocument(doc_id4, content4, DocumentStatus::BANNED, ratings4);
        server.AddDocument(doc_id5, content5, DocumentStatus::REMOVED, ratings5);

        // Найдем документы по запросу "dog" со статусом BANNED (doc_id1, doc_id4)
        const auto found_docs1 = server.FindTopDocuments("dog", DocumentStatus::BANNED);
        ASSERT_EQUAL_HINT(found_docs1.size(), 2, "2 documents should be matched"s);
        ASSERT_EQUAL(found_docs1[0].id, doc_id1);
        ASSERT_EQUAL(found_docs1[1].id, doc_id4);

        // Найдем документы по запросу "snake" со статусом REMOVED (doc_id5)
        const auto found_docs2 = server.FindTopDocuments("snake", DocumentStatus::REMOVED);
        ASSERT_EQUAL_HINT(found_docs2.size(), 1, "1 document should be matched"s);
        ASSERT_EQUAL(found_docs2[0].id, doc_id5);

        // Найдем документы по запросу "cat" со статусом IRRELEVANT (нет совпадений)
        const auto found_docs3 = server.FindTopDocuments("cat", DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_docs3.size(), 0, "No document(s) should be matched"s);
        // Проверим, что "cat" в базе документов имеется и с корректным статусом находится
        const auto found_docs4 = server.FindTopDocuments("cat", DocumentStatus::ACTUAL);
        ASSERT_EQUAL_HINT(found_docs4.size(), 1, "1 document should be matched"s);
        ASSERT_EQUAL(found_docs4[0].id, doc_id2);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestDocumentRelevance);
    RUN_TEST(TestDocumentMatching);
    RUN_TEST(TestDocumentAdd);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestRatingCalc);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatusMatching);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
