#include "read_input_functions.h"

// ��������� #include ��� ���������� ������ ����������� �������
#include <iostream>

// �������� ��������� � ������� �����-������
using namespace std;

// ������� ��������� ������ ����������������� ����� �������
std::string ReadLine()
{
    std::string s;
    std::getline(cin, s);
    return s;
}

// ������� ��������� ����� ����� � �������� ������ �������� ������
int ReadLineWithNumber()
{
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}