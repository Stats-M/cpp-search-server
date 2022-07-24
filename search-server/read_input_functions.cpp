#include "read_input_functions.h"

// Локальные #include для корректной работы определений функций
#include <iostream>

// Упрощаем обращения к потокам ввода-вывода
using namespace std;

// Функция считывает строку пользовательского ввода целиком
std::string ReadLine()
{
    std::string s;
    std::getline(cin, s);
    return s;
}

// Функция считывает целое число и отдельно символ перевода строки
int ReadLineWithNumber()
{
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}
