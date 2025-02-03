#pragma once

#include <windows.h>
#include <string>
#include "Main.h"

class SerialPort {
private:
    HANDLE m_hThreadTerm;
    HANDLE m_hThread;
    HANDLE m_hThreadStarted;

    void InvalidateHandle(HANDLE& hHandle);
    void CloseAndCleanHandle(HANDLE& hHandle);

    bool m_direct = false;

    static unsigned __stdcall ThreadFn(void* pvParam);

    DWORD m_baud;
    BYTE  m_parity;
    BYTE  m_stopBits;
    BYTE  m_dataBits;

    std::string m_commName;
    HANDLE      m_hCommPort;

public:
    SerialPort();
    virtual ~SerialPort();

    std::string m_portName;

    bool reOpenPort();

    void setDirect();
    void setIndirect();
    int  timedRead(uint32_t ms);
    int  timedRead(uint8_t* buf, size_t len, uint32_t ms);
    int  timedRead(char* buf, size_t len, uint32_t ms);

    void flushInput();

    void setTimeout(DWORD ms);

    HRESULT write(const char* data, DWORD dwSize);
    HRESULT write(std::string s) { return write(s.c_str(), s.length()); }
    void    write(char data) { write(&data, 1); }
    bool    Init(std::string szPortName = "COM1", DWORD dwBaudRate = 115200, BYTE byParity = 0, BYTE byStopBits = 1, BYTE byByteSize = 8);
    void    getMode(DWORD& dwBaudRate, BYTE& byByteSize, BYTE& byParity, BYTE& byStopBits);
    bool    setMode(DWORD dwBaudRate, BYTE byByteSize, BYTE byParity, BYTE byStopBits);

    void setRts(bool on);
    void setDtr(bool on);
};

bool selectComPort(std::string& comName);
