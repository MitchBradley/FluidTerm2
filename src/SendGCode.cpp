#include "SendGCode.h"
#include "Console.h"
#include "Colorize.h"

#define CTRL(N) ((N)&0x1f)
void handle_user_input(SerialPort& serial) {
    if (availConsoleChar()) {
        char c = getConsoleChar();
        switch (c) {
                // Realtime characters
            case CTRL('x'):
            case '?':
                serial.write(c);
                break;
            case '~':
            case '!':
                serial.write(c);
                serial.write('?');
                break;
#if 0
                case CTRL('C'):
                case CTRL('Q'):
                case CTRL(']'):
                    okayExit("Exited");
                    break;
#endif
            default:
                // Ignore everything else
                break;
        }
    }
}
void sendGCode(SerialPort& serial, std::ifstream& infile) {
    char        buf[128];
    size_t      len;
    std::string inLine;

    serial.write('\f');  // Turn off echoing
    queuedReception();

    bool done = false;
    while (!done) {
        // Send the next GCode line from the file
        std::string outLine;
        if (!std::getline(infile, outLine)) {
            // Nothing more to send
            done = true;
            continue;
        }
        // Remove spurious CR
        if (outLine.length() && outLine[outLine.length() - 1] == '\r') {
            outLine.pop_back();
        }
        outLine += '\n';
        serial.write(outLine.c_str());
        std::cout << ">" << outLine;

        // Wait for a Grbl response
        // ok sends the next line
        // error and ALARM abort the sending
        // other messages are displayed to the user
        for (bool acked = false; !acked && !done;) {
            handle_user_input(serial);

            // Handle responses from FluidNC
            len = timedRead(buf, 128, 100);
            if (len < 0) {
                std::cout << "Read error" << std::endl;
                done = true;
                break;
            }
            for (size_t i = 0; i < len; i++) {
                char c = buf[i];
                if (c == '\r') {
                    // Ignore CR
                    continue;
                }
                if (c != '\n') {
                    // Add character to line
                    inLine += c;
                    continue;
                }
                if (inLine.length() >= 5 && (inLine.compare(0, 5, "error") == 0 || inLine.compare(0, 5, "ALARM") == 0)) {
                    colorizeOutput(inLine + '\n');
                    inLine = "";
                    done   = true;
                    // We cannot bail out immediately because FluidNC may have sent
                    // another message after the error: line, with more info.
                    // Part of that message might be in buf, so we should process
                    // the rest of buf.
                    continue;
                }
                if (inLine == "ok") {
                    inLine = "";
                    // Leave for loop to send next line
                    acked = true;
                    continue;
                }
                colorizeOutput(inLine + '\n');
                inLine = "";
            }
        }
    }

    // Handle any residual messages before switching back to normal
    while ((len = timedRead(buf, 128, 200)) > 0) {
        colorizeOutput(buf, len);
    }
    normalReception();
    serial.write('\t');  // Turn on echoing
}
