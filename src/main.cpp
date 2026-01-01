#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>
#include "Main.h"
#include "Colorize.h"
#include "SerialPort.h"
#include "FileDialog.h"
#include "Xmodem.h"
#include "stm32loader/stm32action.h"
#include "Console.h"
#include "SendGCode.h"
#include "RxThread.h"
#include <unistd.h>

static void errorExit(const char* msg) {
    std::cerr << msg << std::endl;
    // Restore input mode on exit.
    restoreConsoleModes();
    exit(1);
}

static SerialPort comport;

static void okayExit(const char* msg) {
    comport.write("\x0c");  // Send CTRL-L to exit FluidNC echo mode
    comport.lock();
    std::cerr << msg << std::endl;
    sleep_ms(200);

    comport.setRts(false);
    comport.setDtr(false);

    // Restore input mode on exit.
    restoreConsoleModes();
    exit(0);
}

static void enableFluidEcho() {
    comport.write("\x1b[C");  // Send right-arrow to enter FluidNC echo mode
}

void resetToDownload() {
    comport.lock();

    // Initially RTS and DTR are true, so their hardware lines are 0, and EN and BOOT0 are 1
    comport.setRtsDtr(true, false);  // RTS true (hw 0)  and DTR false (hw 1) sets EN to hw 0
    sleep_ms(10);
    comport.setRtsDtr(false, true);  // RTS false (hw 1) and DTR true (hw 0)  sets BOOT0 to hw 0
                                     // and EN goes to 1 after an RC delay, sampling BOOT0 as 0
    sleep_ms(100);                   // Give the EN time to go to 1 before releasing BOOT0
                                     //    comport.setRtsDtr(true, true);   // Back to normal

    comport.unlock();
}

void resetFluidNC() {
    std::cout << "Resetting MCU" << std::endl;
    comport.lock();

    // Idle state is DTR true, RTS true, so both low at the hardware level,
    // but since their values are the same, EN and IO0 are both high

    comport.setDtr(false);  // RTS true (hw 0)  and DTR false (hw 1) makes EN go low
    sleep_ms(10);
    comport.setDtr(true);  // Return to normal idle state

#if CDC_ACM_SEQUENCE
    // Sequence recommended for CDC-ACM engine in ESP32-S3
    comport.setDtr(false);
    sleep_ms(2);
    comport.setRts(false);
    sleep_ms(2);
    comport.setRts(true);
    sleep_ms(2);
    comport.setDtr(true);
#endif

#if ESPTOOL_CLASSIC_SEQUENCE
    // Sequence cribbed from esptool using separate setRts and setDtr
    comport.setRts(false);
    comport.setDtr(false);  // Idle
    sleep_ms(100);
    comport.setDtr(true);  // Set IO0
    comport.setRts(false);
    sleep_ms(100);
    comport.setRts(true);  // Reset. Calls inverted to go through (1,1) instead of (0,0)
    comport.setDtr(false);
    comport.setRts(true);  // RTS set as Windows only propagates DTR on RTS setting
    sleep_ms(100);
    comport.setDtr(false);
    comport.setRts(false);  // Chip out of reset
    sleep_ms(100);
    // Addition to return to Rts and Dtr active as expected by USB CDC
    comport.setDtr(true);
    comport.setRts(true);  // Normal
#endif
#if ESPTOOL_TIGHT_SEQUENCE
    // Sequence cribbed from esptool using combined setRtsDtr
    comport.setRtsDtr(false, false);  // Idle
    std::cout << "idle" << std::endl;
    sleep_ms(100);
    comport.setRtsDtr(false, true);  // Set IO0
    std::cout << "IO0" << std::endl;
    sleep_ms(20);
    std::cout << "reset" << std::endl;
    comport.setRtsDtr(true, false);  // Reset. Calls inverted to go through (1,1) instead of (0,0)
    sleep_ms(30);
    std::cout << "unreset" << std::endl;
    comport.setRtsDtr(false, false);  // Chip out of reset
    comport.setDtr(false);            // Needed in some environments to ensure IO0=HIGH
    // Addition to return to Rts and Dtr active as expected by USB CDC
    sleep_ms(40);
    comport.setRtsDtr(true, true);
#endif
    comport.unlock();
    //    enableFluidEcho();
    std::cout << "Resetting Done" << std::endl;
}

static const std::string getSaveName(const std::string proposal) {
    editModeOn();

    static std::string saveName;

    std::cout << "FluidNC filename [" << proposal << "]: ";
    std::getline(std::cin, saveName);

    if (saveName.length() && saveName[saveName.length() - 1] == '\r') {
        saveName.pop_back();
    }

    if (saveName.length() == 0) {
        saveName = proposal;
    }

    editModeOff();

    return saveName;
}

struct cmd {
    const char* code;
    uint8_t     value;
    const char* help;
} realtime_commands[] = {
    { "sd", 0x84, "Safety Door" },
    { "jc", 0x85, "JogCancel" },
    { "dr", 0x86, "DebugReport" },
    { "m0", 0x87, "Macro0" },
    { "m1", 0x88, "Macro1" },
    { "m2", 0x89, "Macro2" },
    { "m3", 0x8a, "Macro3" },
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

void uploadFile(const std::string& path, const std::string& remoteName) {
    std::ifstream infile(path, std::ifstream::in | std::ifstream::binary);
    if (infile.fail()) {
        std::cout << "Can't open " << path << std::endl;
        return;
    }
    std::cout << "XModem Upload " << path << " " << remoteName << std::endl;

    std::string msg = "$Xmodem/Receive=";
    msg += remoteName;
    msg += '\n';
    comport.write(msg);
    xmodemTransmit(comport, infile);
}

const char* uploadpath = nullptr;

int main(int argc, char** argv) {
    std::string comName;
    std::string uploadName;
    std::string remoteName;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "p:u:r:")) != -1) {
        switch (c) {
            case 'p':
                comName = optarg;
                break;
            case 'u':
                uploadName = optarg;
                break;
            case 'r':
                remoteName = optarg;
                break;
            case '?':
                if (optopt == 'p' || optopt == 'u' || optopt == 'd')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort();
        }
    }
    for (int index = optind; index < argc; index++)
        printf("Non-option argument %s\n", argv[index]);

    if (comName.length() == 0 && !selectComPort(comName)) {
        editModeOff();
        errorExit("No COM port found");
    }
    editModeOff();

    // Start a thread to read the serial port and send to the console
    if (!comport.Init(comName.c_str(), 115200)) {
        std::string errorstr("Cannot open ");
        errorstr += comName;
        errorExit(errorstr.c_str());
    }

    startRxThread(&comport);

    if (uploadName.length()) {
        if (remoteName.length()) {
            if (remoteName.back() == '/') {
                remoteName += fileTail(uploadName.c_str());
            }
        } else {
            remoteName = fileTail(uploadName.c_str());
        }
        uploadFile(uploadName, remoteName);
        okayExit("Done");
    }

    if (!setConsoleColor()) {
        errorExit("setConsoleColor failed");
    }

    if (!setConsoleModes()) {
        errorExit("setConsoleModes failed");
    }

    std::cout << "FluidTerm " << VERSION << " using " << comName << std::endl;
    std::cout << "Exit: Ctrl-C, Ctrl-Q or Ctrl-], Clear screen: CTRL-W" << std::endl;
    std::cout << "Upload: Ctrl-U, Reset ESP32: Ctrl-R, Send Override: Ctrl-O, STM32 Loader: Ctrl-S" << std::endl;

    enableFluidEcho();

    // In the main thread, read the console and send to the serial port
    while (true) {
        char c = get_character();

        if (c == '\r') {
            c = '\n';
        }
#define CTRL(N) ((N) & 0x1f)
        switch (c) {
            case CTRL('N'):
                std::cout << "Download" << std::endl;
                resetToDownload();
                break;
            case CTRL('D'):
                comport.setDtr(true);
                break;
            case CTRL('E'):
                comport.setDtr(false);
                break;
            case CTRL('F'):
                comport.setRts(true);
                break;
            case CTRL('L'):
                comport.setRts(false);
                break;
            case CTRL('R'): {
                resetFluidNC();
            } break;
            case CTRL('S'): {  // ^S  STMLoader Actions
                editModeOn();
                static std::string command;
                std::cout << "STM32 Loader Command: ";
                std::getline(std::cin, command);
                editModeOff();
                stm32action(comport, command);
            } break;
            case CTRL('U'): {  // ^U
                const std::string path = getFileName("FluidNC\0*.yaml;*.flnc;*.gz\0All\0*.*\0");
                if (path.length() == 0) {
                    std::cout << "No file selected" << std::endl;
                } else {
                    auto remoteName = getSaveName(fileTail(path));
                    uploadFile(path.c_str(), remoteName.c_str());
                }
            } break;
            case CTRL('G'): {  // ^G
                const std::string path = getFileName("GCode\0*.gc;*.gcode;*.nc\0All\0*.*\0");
                if (path.length() == 0) {
                    std::cout << "No file selected" << std::endl;
                } else {
                    infoColor();
                    std::cout << "Sending " << path << std::endl;
                    normalColor();
                    std::ifstream infile(path.c_str(), std::ifstream::in | std::ifstream::binary);
                    sendGCode(comport, infile);
                    infoColor();
                    std::cout << "Sent" << std::endl;
                    normalColor();
                }
            } break;

            case CTRL(']'):
                okayExit("Exited by ^]");
                break;
            case CTRL('W'):
                clearScreen();
                break;
            case CTRL('Q'):
                okayExit("Exited by ^Q");
                break;
            case CTRL('C'):
                okayExit("Exited by ^C");
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
