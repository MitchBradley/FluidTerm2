#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include "Colorize.h"

static const char* gray          = "\x1b[0;37;40m";
static const char* red           = "\x1b[31m";
static const char* bright_green  = "\x1b[92m";
static const char* bright_blue   = "\x1b[94m";
static const char* bright_yellow = "\x1b[93m";
static const char* bright_red    = "\x1b[91m";
static const char* bold_yellow   = "\x1b[1;33;40m";
static const char* bold_cyan     = "\x1b[1;36;40m";
static const char* bold_magenta  = "\x1b[1;36;40m";
static const char* bold_white    = "\x1b[1;37;40m";

static const char* input_color   = gray;
static const char* echo_color    = bright_blue;
static const char* good_color    = bright_green;
static const char* warn_color    = bright_yellow;
static const char* error_color   = bright_red;
static const char* debug_color   = bright_yellow;
static const char* equals_color  = bold_white;
static const char* purple_color  = bold_magenta;
static const char* setting_color = bold_cyan;
static const char* value_color   = bold_yellow;

static void out(char c) {
    std::cout << c;
}
static void out(const char* s) {
    std::cout << s;
}
static void out(std::string s) {
    std::cout << s;
}

static bool colorizedSetting(std::string& line) {
    if (line.length() && line[0] == '$') {
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            return false;
        }
        out(line[0]);
        out(setting_color);
        out(line.substr(1, pos - 1));
        out(equals_color);
        out(line.substr(pos, 1));
        out(value_color);
        out(line.substr(pos + 1));
        out(input_color);
        out('\n');
        return true;
    } else {
        return false;
    }
}

static bool colorized(std::string& line, const char* tag, const char* color) {
    if (line.compare(0, strlen(tag), tag) != 0) {
        return false;
    }
    int offset = 0;

    if (*tag == '[' || *tag == '<') {
        offset = 1;
        out(*tag);
    }
    out(color);
    out(tag + offset);
    out(input_color);
    out(line.substr(strlen(tag)));
    out('\n');

    return true;
}

void errorColor() {
    out(error_color);
}
void normalColor() {
    out(input_color);
}
void goodColor() {
    out(good_color);
}

static void colorizeLine(std::string line) {
    // clang-format off
    bool matched = colorizedSetting(line) ||
            colorized(line, "[MSG:INFO", good_color)  ||
            colorized(line, "[MSG:ERR",  error_color) ||
            colorized(line, "[MSG:WARN", warn_color)  ||
            colorized(line, "[MSG:DBG",  debug_color) ||
            colorized(line, "<Alarm",    warn_color)  ||
            colorized(line, "<Idle",     good_color)  ||
            colorized(line, "<Run",      good_color)  ||
            colorized(line, "error",     error_color);
    // clang-format on
    if (!matched) {
        out(line);
        out('\n');
    }
}

static std::string residue = "";

static bool expectingEcho = false;

void expectEcho() {
    expectingEcho = true;
}

void colorizeOutput(const char* buf, size_t len) {
    //    out('{');    out(residue);     out('}');
    std::string next(buf, len);
    residue += next;

    std::istringstream lines(residue);
    residue.clear();
    int lineCnt = 0;
    while (lines.good()) {
        std::string line;
        std::getline(lines, line, '\n');
        if (lines.eof()) {
            // Partial line at end
            residue = line;
        } else {
            ++lineCnt;
            colorizeLine(line);
        }
    }
    if (residue.length() &&
        ((lineCnt == 0 && (expectingEcho || residue.length() == 1 || (residue[0] != '<' && residue[0] != '[' && residue[0] != '$'))))) {
        //   If there were no complete lines and there are extra
        // characters that do not form a complete lines, we send
        // the extra characters immediately, as they probably
        // result from user interaction.
        //   If there are extra characters after some number of
        // complete lines, we do not send them immediately,
        // as they probably result from a long transfer that
        // did not fit entirely in the serial input buffer.
        // However, if that residue cannot possibly be the
        // start of a colorized sequence, we send it anyway.
        //   This heuristic is probably imperfect; distinguishing
        // between echo of interaction and program output is
        // tricky.
        expectingEcho = false;
        out(residue);
        residue.clear();
    }
}
