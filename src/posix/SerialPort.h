#pragma once

#include <string>
#include <unistd.h>
#include <cstdint>
#include "Main.h"
#include "serial/serial.h"
#include <thread>
#include <chrono>

inline void sleep_ms(const int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

class SerialPort {
private:
    serial::Serial* _port;

    std::string _portName;
    uint32_t    _baudRate;
    uint8_t     _parity;
    uint8_t     _stopBits;
    uint8_t     _dataBits;

    bool _locked = false;

public:
    SerialPort();
    virtual ~SerialPort();

    bool reOpenPort();

    int read(char* buf, size_t len);

    bool isOpen() { return _port->isOpen(); }

    void flushInput();

    void setTimeout(uint32_t interval, uint32_t multiplier, uint32_t constant);

    size_t write(const char* data, size_t len);
    size_t write(std::string s) { return write(s.c_str(), s.length()); }
    void   write(char data) { write(&data, 1); }
    bool   Init(std::string portName, uint32_t baudRate = 115200, uint8_t parity = 0, uint8_t stopBits = 1, uint8_t byteSize = 8);
    void   getMode(uint32_t& baudRate, uint8_t& byteSize, uint8_t& parity, uint8_t& stopBits);
    bool   setMode(uint32_t baudRate, uint8_t byteSize, uint8_t parity, uint8_t stopBits);

    void setRts(bool on);
    void setDtr(bool on);
    void setRtsDtr(bool rts, bool dtr);

    bool locked() { return _locked; }
    void lock() { _locked = true; }
    void unlock() { _locked = false; }

    void restartPort();
};

bool selectComPort(std::string& portName);
