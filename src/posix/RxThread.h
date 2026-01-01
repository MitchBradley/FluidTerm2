#pragma once
#include <fstream>

class SerialPort;

void sendGCode(SerialPort& serial, std::ifstream& infile);

void startRxThread(SerialPort* serial);
void endRxThread();

void changeMode(uint32_t baudRate, const char* mode);
void restoreMode();

void queuedReception();
void normalReception();

int timedRead(char* buf, size_t len, int timeout_ms);
int timedRead(int timeout_ms);
