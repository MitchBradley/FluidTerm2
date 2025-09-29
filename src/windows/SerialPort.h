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

    bool m_direct = false;

    static unsigned __stdcall ThreadFn(void* pvParam);

    DWORD m_baud;
    BYTE  m_parity;
    BYTE  m_stopBits;
    BYTE  m_dataBits;

    std::string m_commName;
    HANDLE      m_hCommPort;

    OVERLAPPED m_read_overlapped;
    OVERLAPPED m_write_overlapped;

    bool _locked = false;

    char   m_buf[1024];
    size_t m_bufp = 0;
    size_t m_end  = 0;
    size_t copy_out(char* buf, size_t len);

    bool m_readWait = false;

    DCB _dcb;

public:
    SerialPort();
    virtual ~SerialPort();

    std::string m_portName;

    bool reOpenPort();

    void setDirect();
    void setIndirect();
#if 0
    int  timedRead(uint32_t ms);
    int  timedRead(uint8_t* buf, size_t len, uint32_t ms);
    int  timedRead(char* buf, size_t len, uint32_t ms);
#endif
    int read(char* buf, size_t len);

    bool isOpen() {
        return m_hCommPort != INVALID_HANDLE_VALUE;
    }

    void flushInput();

    void setTimeout(DWORD ms);

    HRESULT write(const char* data, DWORD dwSize);
    HRESULT write(std::string s) {
        return write(s.c_str(), s.length());
    }
    void write(char data) {
        write(&data, 1);
    }
    bool Init(std::string szPortName = "COM1", DWORD dwBaudRate = 115200, BYTE byParity = 0, BYTE byStopBits = 1, BYTE byByteSize = 8);
    void getMode(DWORD& dwBaudRate, BYTE& byByteSize, BYTE& byParity, BYTE& byStopBits);
    bool setMode(DWORD dwBaudRate, BYTE byByteSize, BYTE byParity, BYTE byStopBits);

    void setRts(bool on);
    void setDtr(bool on);
    void setRtsDtr(bool rts, bool dtr);

    bool locked() {
        return _locked;
    }
    void lock() {
        _locked = true;
    }
    void unlock() {
        _locked = false;
    }

    void restartPort();
};

bool selectComPort(std::string& comName);
