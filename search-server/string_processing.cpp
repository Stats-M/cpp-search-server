#include "string_processing.h"

// Функция преобразует разделенный пробелами текст в вектор строк
std::vector<std::string> SplitIntoWords(const std::string& text)
{
    std::vector<std::string> words;
    std::string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }

    return words;
}

std::vector<std::string_view> SplitIntoWordsView(std::string_view str_v)
{
    using namespace std::string_literals;

    std::vector<std::string_view> result;

    str_v.remove_prefix(0);
    const int64_t pos_end = str_v.npos;
    while (true)
    {
        // Убираем лидирующие пробелы
        while ((!str_v.empty()) && (str_v.front() == ' '))
        {
            str_v.remove_prefix(1);
        }

        int64_t space = str_v.find(' ');
        result.push_back(space == pos_end ? str_v.substr() : str_v.substr(0, space));
        if (space == pos_end)
        {
            break;
        }
        else
        {
            //pos = space + 1;
            str_v.remove_prefix(space + 1);
        }
    }

    return result;
}