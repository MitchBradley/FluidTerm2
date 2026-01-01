#include <windows.h>
#include <string>

const std::string getFileName(const char* filter, bool save) {
    static OPENFILENAMEA ofn;          // common dialog box structure
    static char          szFile[260];  // buffer for file name
    HANDLE               hf;           // file handle

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = 0;  // Pop up a window
    ofn.lpstrFile   = szFile;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
    // use the contents of szFile to initialize itself.
    ofn.lpstrFile[0]   = '\0';
    ofn.nMaxFile       = sizeof(szFile);
    ofn.lpstrFilter    = filter;
    ofn.nFilterIndex   = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle  = 0;

    ofn.lpstrInitialDir = NULL;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;

    // Display the Open dialog box.
    BOOL res = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    return res ? szFile : "";
}
const std::string fileTail(const std::string path) {
    size_t pos = path.rfind("\\");
    if (pos != std::string::npos) {
        return path.substring(pos + 1);
    }
    return "";
}
