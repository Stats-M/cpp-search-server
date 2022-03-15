#pragma once

#include <string>
#include <vector>

#include "document.h"
#include "search_server.h"


void PrintDocument(const Document&);


void PrintMatchDocumentResult(int, std::vector<std::string_view>, DocumentStatus);


void AddDocument(SearchServer&, int, const std::string&, DocumentStatus, const std::vector<int>&);


void FindTopDocuments(const SearchServer&, const std::string&);


void MatchDocuments(const SearchServer&, const std::string&);