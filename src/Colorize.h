#pragma once
#include <cstdint>
#include <string_view>

void expectEcho();
void colorizeOutput(const char* buf, size_t len);
void colorizeOutput(std::string_view str);

void goodColor();
void errorColor();
void normalColor();
void infoColor();
