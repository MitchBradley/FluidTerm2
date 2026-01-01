#pragma once

#include <windows.h>
#include <string>
#include <cstdint>
#include "Main.h"

class SerialPort {
private:
    HANDLE m_hThreadTerm;
    HANDLE m_hThread;
    HANDLE m_hThreadStarted;

    void InvalidateHandle(HANDLE& hHandle);
    void CloseAndCleanHandle(HANDLE& hHandle);

    static unsigned __stdcall ThreadFn(void* pvParam);

    DWORD m_baud;
    BYTE  m_parity;
    BYTE  m_stopBits;
    BYTE  m_dataBits;

    std::string m_commName;
    HANDLE      m_hCommPort;

    bool _locked = false;

    DCB _dcb;

public:
    SerialPort();
    virtual ~SerialPort();

    std::string m_portName;

    bool reOpenPort();

    int read(char* buf, size_t len);

    bool isOpen() { return m_hCommPort != INVALID_HANDLE_VALUE; }

    void flushInput();

    void setTimeout(DWORD interval, DWORD multiplier, DWORD constant);

    size_t  write(const char* data, DWORD dwSize);
    size_   write(std::string s) { return write(s.c_str(), s.length()); }
    void    write(char data) { write(&data, 1); }
    bool    Init(std::string szPortName = "COM1", DWORD dwBaudRate = 115200, BYTE byParity = 0, BYTE byStopBits = 1, BYTE byByteSize = 8);
    void    getMode(DWORD& dwBaudRate, BYTE& byByteSize, BYTE& byParity, BYTE& byStopBits);
    bool    setMode(DWORD dwBaudRate, BYTE byByteSize, BYTE byParity, BYTE byStopBits);

    void setRts(bool on);
    void setDtr(bool on);
    void setRtsDtr(bool rts, bool dtr);

    bool locked() { return _locked; }
    void lock() { _locked = true; }
    void unlock() { _locked = false; }

    void restartPort();
};

bool selectComPort(std::string& comName);
