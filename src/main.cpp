#include <windows.h>
#include <iostream>
#include <fstream>
#include <conio.h>
#include <stdio.h>
#include <string>
#include "Colorize.h"
#include "SerialThread.h"
#include "FileDialog.h"
#include "Xmodem.h"
#include "Console.h"

VOID ErrorExit(LPSTR);

const char* getSaveName(const char* proposal) {
    editModeOn();

    static std::string saveName;

    std::cout << "FluidNC filename [" << proposal << "]: ";
    getline(std::cin, saveName);

    //    std::cin >> saveName;
    if (saveName.length() == 0) {
        saveName = proposal;
    }

    editModeOff();

    return saveName.c_str();
}

VOID ErrorExit(const char* lpszMessage) {
    fprintf(stderr, "%s\n", lpszMessage);
    Sleep(1000);

    // Restore input mode on exit.
    restoreConsoleModes();
    ExitProcess(0);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    if (!setConsoleColor()) {
        ErrorExit("setConsoleColor failed");
    }

    std::string comName;
    if (!selectComPort(comName)) {
        ErrorExit("");
    }

    std::cout << "Using " << comName << std::endl;

    // Do this after selecting the COM port because we want
    // to be able to echo console input during COM selection.
    if (!setConsoleModes()) {
        ErrorExit("setConsoleModes failed");
    }

    SerialThread comm;

    // Start a thread to read the serial port and send to the console
    if (!comm.Init(comName.c_str(), 115200)) {
        ErrorExit("Cannot open serial port");
    }

    Sleep(1000);
    comm.write("\x1b[C");  // Send right-arrow to enter FluidNC echo mode

    // In the main thread, read the console and send to the serial port
    while (true) {
        char c;
        c = getch();
        if (c == '\r') {
            c = '\n';
        }
        switch (c) {
            //            case 0x03:
            //                ErrorExit("Killed by ^C");
            //                break;
            case 0x15: {  // ^U
                const char* path = getFileName();
                if (*path == '\0') {
                    std::cout << "No file selected" << std::endl;
                } else {
                    const char* remoteName = getSaveName(fileTail(path));
                    std::cout << "XModem Upload " << path << " " << remoteName << std::endl;
                    std::string msg = "$Xmodem/Receive=";
                    msg += remoteName;
                    msg += '\n';
                    comm.write(msg);
                    Sleep(1000);
                    std::ifstream infile(path, std::ifstream::in);
                    int           ret = xmodemTransmit(comm, infile);
                    if (ret < 0) {
                        std::cout << "Returned " << ret << std::endl;
                    }
                }
            } break;

            case 0x1d:  // ^]
                ErrorExit("Killed by ^]");
                break;
            default:
                comm.write(&c, 1);
                break;
        }
    }
    return 0;
}
