#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include "FileDialog.h"
#include "tinyfiledialogs.h"

// extern "C" std::vector<std::string> open_file_dialog_macos();

const std::filesystem::path getFileName(const char* filter, bool save) {
    const char* res = tinyfd_openFileDialog("Choose file", "", 0, NULL, NULL, 0);
    return res;
}

const std::filesystem::path fileTail(const std::filesystem::path path) {
    return path.filename();
}
