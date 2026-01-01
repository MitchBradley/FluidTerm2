#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include "tinyfiledialogs.h"

// extern "C" std::vector<std::string> open_file_dialog_macos();

const std::string getFileName(const char* filter, bool save) {
    // This does not work because the windows pattern list
    // has groups like "Legend\0*.yaml;*.flnc\0All\0*.*\0"
    // and Mac file selectors do not support alternate lists.
    //
    // int         nPatterns = 0;
    // const char* patterns[10];
    // const char* p = filter;
    // patterns[0]   = p;
    // while (nPatterns < 9) {
    //     if (*p == '\0') {
    //         patterns[++nPatterns] = ++p;
    //         if (*p == '\0') {
    //             break;
    //         }
    //     } else {
    //         ++p;
    //     }
    // }

    // const char* res = tinyfd_openFileDialog("Choose file", "", nPatterns, patterns, NULL, 0);
    const char* res = tinyfd_openFileDialog("Choose file", "", 0, NULL, NULL, 0);

    return res;
}

const std::string fileTail(const std::string path) {
    size_t pos = path.rfind("/");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return "";
}

long long fileSize(const std::string name) {
    struct stat st;

    if (stat(name.c_str(), &st) == 0) {
        // st_size is a 64-bit off_t type due to the macro definition
        return (long long)st.st_size;
    } else {
        // Handle error (e.g., file not found, permission issues)
        perror("stat");
        return -1;
    }
}
