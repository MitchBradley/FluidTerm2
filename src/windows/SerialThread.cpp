#include <string>
#include <iostream>
#include <assert.h>
#include "SerialThread.h"
#include <Process.h>

#include "Filter.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void SerialThread::setTimeout(DWORD ms) {
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

void SerialThread::InvalidateHandle(HANDLE& hHandle) {
    hHandle = INVALID_HANDLE_VALUE;
}

void SerialThread::CloseAndCleanHandle(HANDLE& hHandle) {
    BOOL abRet = CloseHandle(hHandle);
    if (!abRet) {
        assert(0);
    }
    InvalidateHandle(hHandle);
}
SerialThread::SerialThread() {
    InvalidateHandle(m_hThreadTerm);
    InvalidateHandle(m_hThread);
    InvalidateHandle(m_hThreadStarted);
    InvalidateHandle(m_hCommPort);
    InvalidateHandle(m_hDataRx);

    m_eState = SS_UnInit;
}

SerialThread::~SerialThread() {
    m_eState = SS_Unknown;
}

void SerialThread::setDirect() {
    m_tap = true;
}
void SerialThread::setIndirect() {
    setTimeout(10);
    m_tap = false;
}
int SerialThread::timedRead(uint32_t ms) {
    setTimeout(ms);
    char  c;
    DWORD dwBytesRead;
    ::ReadFile(m_hCommPort, &c, 1, &dwBytesRead, NULL);
    if (dwBytesRead != 1) {
        // std::cout << '.';
    }

    return dwBytesRead == 1 ? c : -1;
}
void SerialThread::flushInput() {
    while (timedRead(500) >= 0) {}
}

bool SerialThread::Init(std::string szPortName, DWORD dwBaudRate, BYTE byParity, BYTE byStopBits, BYTE byByteSize) {
    bool hr = false;
    try {
        m_hDataRx = CreateEvent(0, 0, 0, 0);
        //open the COM Port
        //        m_hCommPort = CreateFile(szPortName.c_str(),
        m_hCommPort = CreateFile("\\\\.\\COM16",
                                 GENERIC_READ | GENERIC_WRITE,  //access ( read and write)
                                 0,                             //(share) 0:cannot share the COM port
                                 0,                             //security  (None)
                                 OPEN_EXISTING,                 // creation : open_existing
                                 //                                 FILE_FLAG_OVERLAPPED,
                                 0,  // Not overlapped
                                 0   // no templates file for COM port...
        );
        if (m_hCommPort == INVALID_HANDLE_VALUE) {
            std::cout << "Can't open " << szPortName.c_str() << std::endl;

            //           assert(0);
            return false;
        }

        if (!::SetCommMask(m_hCommPort, EV_RXCHAR | EV_TXEMPTY)) {
            assert(0);
            return false;
        }

        //now we need to set baud rate etc,
        DCB dcb = { 0 };

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
        dcb.fDtrControl     = DTR_CONTROL_ENABLE;
        dcb.fOutxDsrFlow    = 0;

        if (!::SetCommState(m_hCommPort, &dcb)) {
            assert(0);
            return false;
        }

        setTimeout(10);

        //create thread terminator event...
        m_hThreadTerm    = CreateEvent(0, 0, 0, 0);
        m_hThreadStarted = CreateEvent(0, 0, 0, 0);

        m_hThread = (HANDLE)_beginthreadex(0, 0, SerialThread::ThreadFn, (void*)this, 0, 0);

        DWORD dwWait = WaitForSingleObject(m_hThreadStarted, INFINITE);

        assert(dwWait == WAIT_OBJECT_0);

        CloseHandle(m_hThreadStarted);

        InvalidateHandle(m_hThreadStarted);
    } catch (...) {
        assert(0);
        hr = false;
    }
    if (SUCCEEDED(hr)) {
        m_eState = SS_Init;
    }
    return true;
}

unsigned __stdcall SerialThread::ThreadFn(void* pvParam) {
    SerialThread* apThis     = (SerialThread*)pvParam;
    bool          abContinue = true;

    SetEvent(apThis->m_hThreadStarted);
    while (1) {
        while (apThis->m_tap) {
            Sleep(1000);
        }
        DWORD dwBytesRead = 0;
        char  szTmp[128];

        ::ReadFile(apThis->m_hCommPort, szTmp, sizeof(szTmp), &dwBytesRead, NULL);
        if (dwBytesRead > 0) {
            filterOutput(szTmp, dwBytesRead);
        } else {
            //            std::cout << "T" << std::endl;
        }
    }
    return 0;
}

HRESULT SerialThread::write(const char* data, DWORD dwSize) {
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
        std::cerr << "No COM ports";
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
