#include "process_queries.h"

#include <algorithm>
#include <execution>
#include <functional>
#include <numeric>
#include <vector>
#include <string>


std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    // ����� ������ ����� ������� ������ ���� �� ������� ���� ����� ������, ��� � ������������ �������:
    //for (const std::string& query : queries)
    //{
    //    documents_lists.push_back(search_server.FindTopDocuments(query));
    //}

    std::vector<std::vector<Document>> results(queries.size());

    std::transform(std::execution::par, // ������ �++17!
                queries.begin(), queries.end(),  // ������� �������� 1
                results.begin(),             // ������� �������� 2
                //0,  // ��������� ��������
                //plus<>{},  // reduce-�������� (������������ �������)
                [&search_server](const std::string& param)    // ��� ������� &search_server � C++20 �������� ��������� � 1,5 ���� ����������!
                {
                    return search_server.FindTopDocuments(param);
                });  // map-��������

    return results;
}

// ����� ������ ����� ������� ������ ���� �� ������� ���� ����� ������, ��� � ������������ ������� DefaultProcess
std::vector<std::vector<Document>> DefaultProcess(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> results(queries.size());

    for (const std::string& query : queries)
    {
        results.push_back(search_server.FindTopDocuments(query));
    }

    return results;
}


std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
    //std::list<Document> results;
    std::vector<Document> results_v;
    //std::vector<std::vector<Document>> intermediate_results = ProcessQueries(search_server, queries);

/*    std::transform(std::execution::par, // ������ �++17!
                   queries.begin(), queries.end(),  // ������� �������� 1
                   intermediate_results.begin(),             // ������� �������� 2
                   //0,  // ��������� ��������
                   //plus<>{},  // reduce-�������� (������������ �������)
                   [&search_server](const std::string& param)    // ��� ������� &search_server � C++20 �������� ��������� � 1,5 ���� ����������!
                   {
                       return search_server.FindTopDocuments(param);
                   });  // map-��������
*/
    // ���������� ���������� ���������� � ������� ������ (�������� O(1))
    // �� const ����� ����������� ������ ��� ��������������� move placement
/*
    for (auto& vector : intermediate_results)
    {
        for (auto& element : vector)
        {
            results.push_back(element);
        }
    }
*/


    for (const auto& vector : ProcessQueries(search_server, queries)) //instead of intermediate_results
    {
        //results.splice(vector.begin(), vector.end());
        //results.insert (results.end(), vector.begin(), vector.end());
        for (const auto& document : vector)
        {
            results_v.push_back(document);
        }
    }

    //return results;
    return results_v;
}
