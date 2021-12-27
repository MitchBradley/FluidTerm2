#pragma once

#include <windows.h>
#include <string>

typedef enum tagSERIAL_STATE {
    SS_Unknown,
    SS_UnInit,
    SS_Init,
    SS_Started,
    SS_Stopped,

} SERIAL_STATE;

class SerialPort {
private:
    SERIAL_STATE m_eState;
    HANDLE       m_hThreadTerm;
    HANDLE       m_hThread;
    HANDLE       m_hThreadStarted;
    HANDLE       m_hDataRx;

    void InvalidateHandle(HANDLE& hHandle);
    void CloseAndCleanHandle(HANDLE& hHandle);

    bool m_tap = false;

    static unsigned __stdcall ThreadFn(void* pvParam);

    HANDLE m_hCommPort;

public:
    SerialPort();
    virtual ~SerialPort();

    inline void SetDataReadEvent() { SetEvent(m_hDataRx); }

    void setDirect();
    void setIndirect();
    int  timedRead(uint32_t ms);

    void setTimeout(DWORD ms);
    void flushInput();

    HRESULT write(const char* data, DWORD dwSize);
    HRESULT write(std::string s) { return write(s.c_str(), s.length()); }
    void    write(char data) { write(&data, 1); }
    bool    Init(std::string szPortName = "COM1", DWORD dwBaudRate = 115200, BYTE byParity = 0, BYTE byStopBits = 1, BYTE byByteSize = 8);
};

bool selectComPort(std::string& comName);
