#pragma once

#include <string>
#include <vector>

const char *time_now();
const char *time_now_ms();
const char *time_now_long();
int date_now();
void split(const std::string& str, const std::string& del, std::vector<std::string>& v);
//void wsplit(const std::wstring& str, const std::wstring& del, std::vector<wstring>& v);