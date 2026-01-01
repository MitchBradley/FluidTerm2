#include <iostream>

#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>

struct termios orig_termios;

void editModeOn() {
    // stdout changes?
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void editModeOff() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;

    // input modes: no break, no CR to NL, no parity check, no strip char,
    // no start/stop output control.
    raw.c_iflag &= ~(BRKINT | /* ICRNL | */ INPCK | ISTRIP | IXON);

    // output modes - disable post processing */
    //    raw.c_oflag &= ~(OPOST);
    raw.c_oflag |= ONLCR;

    // control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);

    // local modes - choing off, canonical off, no extended functions,
    // no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_lflag |= ISIG;

    // control chars - set return condition: min number of bytes and timer.
    // We want read to return every single byte, without timeout. */
    raw.c_cc[VINTR] = 3; /* Ctrl-C interrupts process */
    // raw.c_cc[VMIN]  = 1;
    // raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    // // The cfmakeraw function is a standard way to achieve raw mode
    // cfmakeraw(&raw);

    // // Additional configuration for read() behavior (VMIN and VTIME)
    // // VMIN=1, VTIME=0 means read() returns as soon as 1 character is available, blocking indefinitely until then
    // raw.c_cc[VMIN]  = 1;
    // raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    atexit(editModeOn);  // Ensure terminal mode is restored on exit
}

bool setConsoleModes() {
    return true;
}

void clearScreen() {
    //    std::cout << "\x1b[2J";
}

bool setConsoleColor() {
    clearScreen();  // Apply the new colors
    return true;
}

void restoreConsoleModes() {
    editModeOn();
}

int getConsoleChar() {
    char c;
    return read(0, &c, 1) == 1 ? c : -1;
}

bool availConsoleChar() {
    struct pollfd pfd;
    pfd.fd     = STDIN_FILENO;  // File descriptor for standard input
    pfd.events = POLLIN;        // Check for input events

    return poll(&pfd, 1, 0) > 0;
}
