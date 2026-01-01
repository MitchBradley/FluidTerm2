#pragma once
#include <string>
const std::string getFileName(const char* filter, bool save = false);
const std::string fileTail(const std::string path);

__int64_t fileSize(const std::string name);
