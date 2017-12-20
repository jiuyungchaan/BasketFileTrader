#include "utils.h"

#include <time.h>
#include <string.h>
#include <windows.h>

using namespace std;

const char *time_now() {
	static char timestamp_str[32];
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	//timeinfo = localtime(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	snprintf(timestamp_str, sizeof(timestamp_str),
		"%d%02d%02d-%02d:%02d:%02d",
		1900 + timeinfo.tm_year, 1 + timeinfo.tm_mon,
		timeinfo.tm_mday, timeinfo.tm_hour,
		timeinfo.tm_min, timeinfo.tm_sec);
	return timestamp_str;
}

const char *time_now_ms() {
	static char timestamp_str[32];
	SYSTEMTIME t;
	GetSystemTime(&t);

	snprintf(timestamp_str, sizeof(timestamp_str),
		"%d%02d%02d-%02d:%02d:%02d.%03d",
		t.wYear, t.wMonth, t.wDay, t.wHour+8,
		t.wMinute, t.wSecond, t.wMilliseconds);
	return timestamp_str;
}

const char *time_now_long() {
	static char timestamp_str[32];
	SYSTEMTIME t;
	GetSystemTime(&t);

	snprintf(timestamp_str, sizeof(timestamp_str),
		"%d%02d%02d%02d%02d%02d%03d",
		t.wYear, t.wMonth, t.wDay, t.wHour+8,
		t.wMinute, t.wSecond, t.wMilliseconds);
	return timestamp_str;
}

int date_now() {
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	//timeinfo = localtime(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	int date = (1900 + timeinfo.tm_year) * 10000 + (1 + timeinfo.tm_mon) * 100 + timeinfo.tm_mday;
	return date;
}

void split(const string& str, const string& del, vector<string>& v) {
	string::size_type start, end;
	start = 0;
	end = str.find(del);
	while (end != string::npos) {
		v.push_back(str.substr(start, end - start));
		start = end + del.size();
		end = str.find(del, start);
	}
	if (start != str.length()) {
		v.push_back(str.substr(start));
	}
}

/*
void wsplit(const wstring& str, const wstring& del, vector<wstring>& v)
{
	wstring::size_type start, end;
	start = 0;
	end = str.find(del);
	while (end != wstring::npos) {
		v.push_back(str.substr(start, end - start));
		start = end + del.size();
		end = str.find(del, start);
	}
	if (start != str.length()) {
		v.push_back(str.substr(start));
	}
}
*/
