#include <windows.h>
#include <string>

const char* getFileName() {
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
    ofn.lpstrFilter    = "All\0*.*\0FluidNC Config\0*.yaml,*.flnc\0";
    ofn.nFilterIndex   = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle  = 0;

    ofn.lpstrInitialDir = NULL;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box.

    if (GetOpenFileNameA(&ofn)) {
        return szFile;
    } else {
        return "";
    }
}
const char* fileTail(const char* path) {
    const char* endp = path + strlen(path);
    while (endp != path) {
        --endp;
        if (*endp == '\\') {
            return endp + 1;
        }
    }
    return endp;
}
