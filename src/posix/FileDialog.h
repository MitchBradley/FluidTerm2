#pragma once
#include <string>
#include <filesystem>

const std::filesystem::path getFileName(const char* filter, bool save = false);
const std::filesystem::path fileTail(const std::filesystem::path path);
