#pragma once

#include <windows.h>
#include <string>

class SerialPort {
private:
    HANDLE m_hThreadTerm;
    HANDLE m_hThread;
    HANDLE m_hThreadStarted;

    void InvalidateHandle(HANDLE& hHandle);
    void CloseAndCleanHandle(HANDLE& hHandle);

    bool m_direct = false;

    static unsigned __stdcall ThreadFn(void* pvParam);

    HANDLE m_hCommPort;

    void setTimeout(DWORD ms);

public:
    SerialPort();
    virtual ~SerialPort();

    void setDirect();
    void setIndirect();
    int  timedRead(uint32_t ms);

    void flushInput();

    HRESULT write(const char* data, DWORD dwSize);
    HRESULT write(std::string s) { return write(s.c_str(), s.length()); }
    void    write(char data) { write(&data, 1); }
    bool    Init(std::string szPortName = "COM1", DWORD dwBaudRate = 115200, BYTE byParity = 0, BYTE byStopBits = 1, BYTE byByteSize = 8);
};

bool selectComPort(std::string& comName);