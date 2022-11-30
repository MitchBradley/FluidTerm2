#include "SendGCode.h"
#include <iostream>
#include <fstream>
#include <thread>

int sendGCode(SerialPort& serial, std::ifstream& infile) {
    int retval = 0;
    serial.setDirect();
    serial.write('\f');  // Turn off echoing
    for (std::string line; std::getline(infile, line);) {
        std::cout << "> " << line << std::endl;
        serial.write(line);
        serial.write('\n');
        char  sline[128];
        char* sp = sline;
        do {
            int c = serial.timedRead(4000);
            if (c == -1) {
                continue;
            }
            *sp++ = c;
            std::cout << char(c);
            if (char(c) == '\n') {
                if (!strncmp(sline, "error", strlen("error"))) {
                    retval = -1;
                    goto out;
                }
                break;
            }
        } while (true);
    }
out:
    serial.setIndirect();
    serial.write('\t');  // Echo mode on
    return retval;
}
