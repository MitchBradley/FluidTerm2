#include <string>
#include <iostream>
#include <assert.h>
#include "SerialPort.h"
#include <Process.h>

#include "Colorize.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void SerialPort::setTimeout(DWORD ms) {
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout         = 0;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = ms;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;

    if (!SetCommTimeouts(m_hCommPort, &timeouts)) {
        assert(0);
    }
}

void SerialPort::InvalidateHandle(HANDLE& hHandle) {
    hHandle = INVALID_HANDLE_VALUE;
}

void SerialPort::CloseAndCleanHandle(HANDLE& hHandle) {
    BOOL abRet = CloseHandle(hHandle);
    if (!abRet) {
        assert(0);
    }
    InvalidateHandle(hHandle);
}
SerialPort::SerialPort() {
    InvalidateHandle(m_hThreadTerm);
    InvalidateHandle(m_hThread);
    InvalidateHandle(m_hThreadStarted);
    InvalidateHandle(m_hCommPort);
}

SerialPort::~SerialPort() {}

void SerialPort::setDirect() {
    m_direct = true;
}
void SerialPort::setIndirect() {
    setTimeout(10);
    m_direct = false;
}
int SerialPort::timedRead(uint32_t ms) {
    setTimeout(ms);
    char  c;
    DWORD dwBytesRead;
    ::ReadFile(m_hCommPort, &c, 1, &dwBytesRead, NULL);
    if (dwBytesRead != 1) {
        // std::cout << '.';
    }

    return dwBytesRead == 1 ? c : -1;
}
void SerialPort::flushInput() {
    while (timedRead(500) >= 0) {}
}

bool SerialPort::Init(std::string portName, DWORD dwBaudRate, BYTE byParity, BYTE byStopBits, BYTE byByteSize) {
    bool hr = false;
    try {
        //open the COM Port
        std::string portDev = "\\\\.\\";  // Prefix to convert to NT device name
        portDev += portName;
        m_hCommPort = CreateFile(portDev.c_str(),               // e.g. "\\\\.\\COM16"
                                 GENERIC_READ | GENERIC_WRITE,  //access ( read and write)
                                 0,                             //(share) 0:cannot share the COM port
                                 0,                             //security  (None)
                                 OPEN_EXISTING,                 // creation : open_existing
                                 0,                             // Not overlapped
                                 0                              // no templates file for COM port...
        );
        if (m_hCommPort == INVALID_HANDLE_VALUE) {
            std::cout << "Can't open " << portName << std::endl;
            return false;
        }

        if (!::SetCommMask(m_hCommPort, EV_RXCHAR | EV_TXEMPTY)) {
            assert(0);
            return false;
        }

        DCB dcb       = { 0 };
        dcb.DCBlength = sizeof(DCB);
        if (!::GetCommState(m_hCommPort, &dcb)) {
            return false;
        }

        dcb.BaudRate = dwBaudRate;
        dcb.ByteSize = byByteSize;
        dcb.Parity   = byParity;
        if (byStopBits == 1)
            dcb.StopBits = ONESTOPBIT;
        else if (byStopBits == 2)
            dcb.StopBits = TWOSTOPBITS;
        else
            dcb.StopBits = ONE5STOPBITS;

        dcb.fDsrSensitivity = 0;
        dcb.fDtrControl     = DTR_CONTROL_DISABLE;
        dcb.fRtsControl     = RTS_CONTROL_DISABLE;
        dcb.fOutxDsrFlow    = 0;

        if (!::SetCommState(m_hCommPort, &dcb)) {
            assert(0);
            return false;
        }

        setTimeout(10);

        //create thread terminator event...
        m_hThreadTerm    = CreateEvent(0, 0, 0, 0);
        m_hThreadStarted = CreateEvent(0, 0, 0, 0);

        m_hThread = (HANDLE)_beginthreadex(0, 0, SerialPort::ThreadFn, (void*)this, 0, 0);

        DWORD dwWait = WaitForSingleObject(m_hThreadStarted, INFINITE);

        assert(dwWait == WAIT_OBJECT_0);

        CloseHandle(m_hThreadStarted);

        InvalidateHandle(m_hThreadStarted);
    } catch (...) {
        assert(0);
        hr = false;
    }
    return true;
}

void SerialPort::setRts(bool on) {
    DCB dcb       = { 0 };
    dcb.DCBlength = sizeof(DCB);
    if (!::GetCommState(m_hCommPort, &dcb)) {
        return;
    }
    dcb.fRtsControl = on ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
    ::SetCommState(m_hCommPort, &dcb);
}

void SerialPort::setDtr(bool on) {
    DCB dcb       = { 0 };
    dcb.DCBlength = sizeof(DCB);
    if (!::GetCommState(m_hCommPort, &dcb)) {
        return;
    }
    dcb.fDtrControl = on ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    ::SetCommState(m_hCommPort, &dcb);
}

unsigned __stdcall SerialPort::ThreadFn(void* pvParam) {
    SerialPort* apThis     = (SerialPort*)pvParam;
    bool        abContinue = true;

    SetEvent(apThis->m_hThreadStarted);
    while (1) {
        // When Xmodem is using the serial port directly we stop polling in the thread
        while (apThis->m_direct) {
            Sleep(100);
        }
        DWORD dwBytesRead = 0;
        char  szTmp[128];

        ::ReadFile(apThis->m_hCommPort, szTmp, sizeof(szTmp), &dwBytesRead, NULL);
        if (dwBytesRead > 0) {
            colorizeOutput(szTmp, dwBytesRead);
        } else {
            // Timeout
            // std::cout << "T" << std::endl;
        }
    }
    return 0;
}

HRESULT SerialPort::write(const char* data, DWORD dwSize) {
    int        iRet = 0;
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    ov.hEvent            = CreateEvent(0, true, 0, 0);
    DWORD dwBytesWritten = 0;

    iRet = WriteFile(m_hCommPort, data, dwSize, &dwBytesWritten, &ov);
    if (iRet == 0) {
        WaitForSingleObject(ov.hEvent, INFINITE);
    }

    CloseHandle(ov.hEvent);

    return S_OK;
}

bool selectComPort(std::string& comName)  //added function to find the present serial
{
    char lpTargetPath[5000];  // buffer to store the path of the COMPORTS
    int  numPorts  = 0;
    int  firstPort = -1;

    unsigned int choices = 0;
    uint8_t      ports[256];
    for (int i = 0; i < 255; i++) {                    // checking ports from COM0 to COM255
        std::string str  = "COM" + std::to_string(i);  // converting to COM0, COM1, COM2
        DWORD       test = QueryDosDevice(str.c_str(), lpTargetPath, 5000);

        if (test != 0) {  //QueryDosDevice returns zero if it didn't find an object
            ports[choices++] = i;
        }

        if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            ExitProcess(1);
        }
    }
    if (choices == 0) {
        comName = "";
        return false;
    }
    if (choices == 1) {
        comName = "COM" + std::to_string(ports[0]);
        return true;
    }
    std::cout << "Select a COM port" << std::endl;

    for (int i = 0; i < choices; i++) {
        std::string str  = "COM" + std::to_string(ports[i]);  // converting to COM0, COM1, COM2
        DWORD       test = QueryDosDevice(str.c_str(), lpTargetPath, 5000);
        if (test != 0) {  //QueryDosDevice returns zero if it didn't find an object
            std::cout << std::to_string(i) << ": " << str << " (" << lpTargetPath << ")" << std::endl;
        }
    }
    while (true) {
        std::cout << "Choice: ";
        unsigned int choice;
        std::cin >> choice;
        if (choice < choices) {
            comName = "COM" + std::to_string(ports[choice]);
            return true;
        }
    }

    return false;
}
