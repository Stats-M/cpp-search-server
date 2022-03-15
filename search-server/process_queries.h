#pragma once

#include "document.h"
#include "search_server.h"

#include <vector>
#include <execution>
#include <string>
#include <algorithm>
#include <functional>
#include <numeric>
#include <list>


std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer&,
    const std::vector<std::string>&);

std::vector<std::vector<Document>> DefaultProcess(
    const SearchServer&,
    const std::vector<std::string>&);

std::vector<Document> ProcessQueriesJoined(
    const SearchServer&,
    const std::vector<std::string>&);