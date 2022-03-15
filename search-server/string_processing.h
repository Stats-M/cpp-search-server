#pragma once

// #include для type resolution в объявлениях функций:
#include <string>
#include <string_view>
#include <vector>
#include <set>

std::vector<std::string> SplitIntoWords(const std::string&);

std::vector<std::string_view> SplitIntoWordsView(std::string_view);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings)
{
    std::set<std::string, std::less<>> non_empty_strings;
    //for (std::string_view str_v : strings)
    for (auto str_v : strings)
    {
        if (!str_v.empty())
        {
            //non_empty_strings.insert(str_v.data());
            non_empty_strings.insert(std::string(str_v));
        }
    }
    return non_empty_strings;
}