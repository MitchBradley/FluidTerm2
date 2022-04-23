#include <iostream>
#include <fstream>
#include <conio.h>
#include <stdio.h>
#include <string>
#include "Main.h"
#include "Colorize.h"
#include "SerialPort.h"
#include "FileDialog.h"
#include "Xmodem.h"
#include "Console.h"

static void errorExit(const char* msg) {
    std::cerr << msg << std::endl;
    std::cerr << "..press any key to continue" << std::endl;
    getch();

    // Restore input mode on exit.
    restoreConsoleModes();
    exit(1);
}

static void okayExit(const char* msg) {
    std::cerr << msg << std::endl;
    Sleep(1000);

    // Restore input mode on exit.
    restoreConsoleModes();
    exit(0);
}

static SerialPort comport;

static void enableFluidEcho() {
    comport.write("\x1b[C");  // Send right-arrow to enter FluidNC echo mode
}

void resetFluidNC() {
    std::cout << "Resetting MCU" << std::endl;
    comport.setRts(true);
    Sleep(500);
    comport.setRts(false);
    Sleep(2000);
    enableFluidEcho();
}

static const char* getSaveName(const char* proposal) {
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

struct cmd {
    const char* code;
    uint8_t     value;
    const char* help;
} realtime_commands[] = {
    { "sd", 0x84, "Safety Door" },
    { "jc", 0x85, "JogCancel" },
    { "dr", 0x86, "DebugReport" },
    { "fr", 0x90, "FeedOvrReset" },
    { "f>", 0x91, "FeedOvrCoarsePlus" },
    { "f<", 0x92, "FeedOvrCoarseMinus" },
    { "f+", 0x93, "FeedOvrFinePlus" },
    { "f-", 0x94, "FeedOvrFineMinus" },
    { "rr", 0x95, "RapidOvrReset" },
    { "rm", 0x96, "RapidOvrMedium" },
    { "rl", 0x97, "RapidOvrLow" },
    { "rx", 0x98, "RapidOvrExtraLow" },
    { "sr", 0x99, "SpindleOvrReset" },
    { "s>", 0x9A, "SpindleOvrCoarsePlus" },
    { "s<", 0x9B, "SpindleOvrCoarseMinus" },
    { "s+", 0x9C, "SpindleOvrFinePlus" },
    { "s-", 0x9D, "SpindleOvrFineMinus" },
    { "ss", 0x9E, "SpindleOvrStop" },
    { "ft", 0xA0, "CoolantFloodOvrToggle" },
    { "mt", 0xA1, "CoolantMistOvrToggle" },
    { NULL, 0, NULL },
};

char get_character() {
    int res = getConsoleChar();
    if (res < 0) {
        errorExit("Input error");
    }
    return res;
}
void sendOverride() {
    std::cout << "Enter 2-character code - xx for help: ";

    char c[3];
    c[0] = tolower(get_character());
    std::cout << c[0];
    c[1] = tolower(get_character());
    std::cout << c[1] << ' ';
    c[2] = '\0';

    for (struct cmd* p = realtime_commands; p->code; p++) {
        if (!strcmp(c, p->code)) {
            char ch = p->value;
            std::cout << '<' << p->help << '>' << std::endl;
            comport.write(&ch, 1);
            return;
        }
    }
    std::cout << std::endl << "The codes are:" << std::endl;
    for (struct cmd* p = realtime_commands; p->code; p++) {
        std::cout << p->code << " " << p->help << std::endl;
    }
}

int main(int argc, char** argv) {
    if (!setConsoleColor()) {
        errorExit("setConsoleColor failed");
    }

    if (!setConsoleModes()) {
        errorExit("setConsoleModes failed");
    }

    std::string comName;
    editModeOn();
    if (!selectComPort(comName)) {
        editModeOff();
        errorExit("No COM port found");
    }
    editModeOff();

    std::cout << "Using " << comName << std::endl;
    std::cout << "Ctrl-] to exit, Ctrl-U to upload, Ctrl-R to reset" << std::endl;

    // Start a thread to read the serial port and send to the console
    if (!comport.Init(comName.c_str(), 115200)) {
        errorExit("Cannot open serial port");
    }

    enableFluidEcho();

    // In the main thread, read the console and send to the serial port
    while (true) {
        char c = get_character();

        if (c == '\r') {
            c = '\n';
        }
#define CTRL(N) ((N)&0x1f)
        switch (c) {
            case CTRL('R'): {
                resetFluidNC();
            } break;
            case CTRL('U'): {  // ^U
                const char* path = getFileName();
                if (*path == '\0') {
                    std::cout << "No file selected" << std::endl;
                } else {
                    const char* remoteName = getSaveName(fileTail(path));
                    std::cout << "XModem Upload " << path << " " << remoteName << std::endl;
                    std::string msg = "$Xmodem/Receive=";
                    msg += remoteName;
                    msg += '\n';
                    comport.write(msg);
                    Sleep(1000);
                    std::ifstream infile(path, std::ifstream::in | std::ifstream::binary);
                    int           ret = xmodemTransmit(comport, infile);
                    if (ret < 0) {
                        std::cout << "Returned " << ret << std::endl;
                    }
                }
            } break;

            case CTRL(']'):
                okayExit("Exited by ^]");
                break;
            case CTRL('O'):
                sendOverride();
                break;
            default:
                expectEcho();
                comport.write(&c, 1);
                break;
        }
    }
    return 0;
}
