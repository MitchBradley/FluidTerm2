#include <string>
#include <iostream>
#include <assert.h>
#include "Console.h"
#include "SerialPort.h"
#include <stdio.h>
#include <iostream>

#include "Colorize.h"

using namespace serial;

void SerialPort::setTimeout(uint32_t interval, uint32_t multiplier, uint32_t constant) {
    _port->setTimeout(interval, constant, multiplier, 1, 10);
}

SerialPort::SerialPort() {
    _port = nullptr;
}

SerialPort::~SerialPort() {}

void SerialPort::flushInput() {
    //    while (timedRead(500) >= 0) {}
}

bool SerialPort::reOpenPort() {
    try {
        if (_port) {
            delete _port;
        }
        _port = new Serial(_portName, _baudRate, Timeout(), (bytesize_t)_dataBits, (parity_t)_parity, (stopbits_t)_stopBits);
    } catch (std::exception err) { return false; }

    setTimeout(Timeout::max(), 0, 10);

    setDtr(true);
    setRts(true);

    return true;
}

bool SerialPort::Init(std::string portName, uint32_t baudRate, uint8_t parity, uint8_t stopBits, uint8_t dataBits) {
    _portName = portName;
    _baudRate = baudRate;
    _parity   = parity;
    _stopBits = stopBits;
    _dataBits = dataBits;

    try {
        if (!reOpenPort()) {
            return false;
        }
    } catch (...) { assert(0); }
    return true;
}

void SerialPort::getMode(uint32_t& baudRate, uint8_t& dataBits, uint8_t& parity, uint8_t& stopBits) {
    baudRate = _baudRate;
    dataBits = _dataBits;
    parity   = _parity;
    stopBits = _stopBits;
}

bool SerialPort::setMode(uint32_t baudRate, uint8_t dataBits, uint8_t parity, uint8_t stopBits) {
    _baudRate = baudRate;
    _dataBits = dataBits;
    _parity   = parity;
    _stopBits = stopBits;

    _port->setBaudrate(baudRate);
    _port->setBytesize((bytesize_t)dataBits);
    _port->setParity((parity_t)parity);
    _port->setStopbits((stopbits_t)stopBits);

    return true;
}

void SerialPort::setRts(bool on) {
    _port->setRTS(on);
}

void SerialPort::setDtr(bool on) {
    _port->setDTR(on);
}

void SerialPort::setRtsDtr(bool rts, bool dtr) {
    setDtr(dtr);
    setRts(rts);
}

void SerialPort::restartPort() {
    lock();
    int count = 0;

    errorColor();
    std::cout << "Serial port disconnected - waiting for reconnect" << std::endl;
    normalColor();

    if (_port != nullptr) {
        _port->close();
        _port = nullptr;
    }
    //    sleep_ms(100);
    while (true) {
        sleep_ms(10);
        if (reOpenPort()) {
            break;
        }
        if (count == 50) {
            // Do not issue this message if the reconnect happens quickly
            std::cout << "Type Ctrl-C to quit" << std::endl;
        }
        ++count;
        if (availConsoleChar()) {
            restoreConsoleModes();
            exit(0);
        }
    }

    goodColor();
    std::cout << "Serial port reconnected" << std::endl;
    normalColor();
    unlock();
}

int SerialPort::read(char* buf, size_t len) {
    if (_port == nullptr || locked()) {
        sleep_ms(10);
        return 0;
    }
    int ret;
    try {
        ret = (int)_port->read((uint8_t*)buf, len);
    } catch (std::exception& ex) { ret = -1; }
    return ret;
}

size_t SerialPort::write(const char* data, size_t len) {
    if (_port == nullptr || locked()) {
        // discard data while port is being reopened
        return 0;
    }
    size_t bytesWritten = 0;

#if 0
    while (len) {
        size_t n;
        try {
            n = _port->write((const uint8_t*)data, len);

            if (n == 0) {
                break;
            }
            bytesWritten += n;
            len -= n;
        } catch (std::exception ex) {
            std::cerr << ex.what();
            return -1;
        }
    }
#else
    try {
        bytesWritten = _port->write((const uint8_t*)data, len);
    } catch (std::exception ex) {
        std::cerr << ex.what();
        return -1;
    }
#endif
    _port->flush();
    return bytesWritten;
}

bool selectComPort(std::string& portName)  //added function to find the present serial
{
    auto choices = list_ports();

    if (choices.size() == 0) {
        portName = "";
        return false;
    }
    if (choices.size() == 1) {
        portName = choices[0].port;
        return true;
    }
    std::cout << "Select a serial port" << std::endl;

    for (int i = 0; i < choices.size(); i++) {
        std::cout << std::to_string(i) << ": " << choices[i].port << std::endl;
    }
    while (true) {
        std::cout << "Choice: ";
        unsigned int choice;
        std::cin >> choice;
        std::cin.ignore();  // Consume the rest of the line including the newline
        if (choice < choices.size()) {
            portName = choices[choice].port;
            return true;
        }
    }

    return false;
}
