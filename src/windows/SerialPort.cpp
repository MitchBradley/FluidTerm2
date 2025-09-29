#include <string>
#include <iostream>
#include <assert.h>
#include "SerialPort.h"
#include <Process.h>
#include "Console.h"
#include <stdio.h>
#include <iostream>

#include "Colorize.h"

void SerialPort::setTimeout(DWORD ms) {
    COMMTIMEOUTS timeouts;
    if (ms != 1) {
        timeouts.ReadIntervalTimeout        = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant   = ms;
    } else {
        timeouts.ReadIntervalTimeout        = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        // Block in the serial driver until at least one character is received -
        // but we can still timeout at the GetOverlappedResultEx level
        // timeouts.ReadTotalTimeoutConstant = 10;
        timeouts.ReadTotalTimeoutConstant = 10;
    }
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 2;

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
    m_hCommPort = INVALID_HANDLE_VALUE;
}

SerialPort::~SerialPort() {}

size_t SerialPort::copy_out(char* buf, size_t len) {
    auto have   = m_end - m_bufp;
    auto toCopy = std::min(have, len);
    if (toCopy) {
        memcpy(buf, &m_buf[m_bufp], toCopy);
        m_bufp += toCopy;
        if (m_bufp == m_end) {
            m_bufp = m_end = 0;
        }
    }

    return toCopy;
}

void SerialPort::setDirect() {
    m_direct = true;
}
void SerialPort::setIndirect() {
    setTimeout(0);
    m_direct = false;
}
void SerialPort::flushInput() {
    //    while (timedRead(500) >= 0) {}
}
#include <strsafe.h>

void ShowError(const char* lpszFunction, DWORD err) {
    // Retrieve the system error message for the last-error code
    LPVOID lpMsgBuf;
    DWORD  dw = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0,
                  NULL);

    // Display the error message and exit the process

    printf("%s failed with error %d: %s", lpszFunction, dw, lpMsgBuf);
}

void ShowError(const char* lpszFunction) {
    ShowError(lpszFunction, GetLastError());
}

bool SerialPort::reOpenPort() {
    //open the COM Port
    m_hCommPort = CreateFile(m_commName.c_str(),            // e.g. "\\\\.\\COM16"
                             GENERIC_READ | GENERIC_WRITE,  //access ( read and write)
                             0,                             //(share) 0:cannot share the COM port
                             0,                             //security  (None)
                             OPEN_EXISTING,                 // creation : open_existing
                             0,                             // Not overlapped
                             0                              // no templates file for COM port...
    );
    if (m_hCommPort == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!m_read_overlapped.hEvent) {
        m_read_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  // Manual-reset event
    }
    if (!m_write_overlapped.hEvent) {
        m_write_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  // Manual-reset event
    }

    if (!::SetCommMask(m_hCommPort, EV_RXCHAR | EV_TXEMPTY)) {
        assert(0);
        return false;
    }

    _dcb           = { 0 };
    _dcb.DCBlength = sizeof(DCB);
    if (!::GetCommState(m_hCommPort, &_dcb)) {
        return false;
    }

    _dcb.BaudRate = m_baud;
    _dcb.ByteSize = m_dataBits;
    _dcb.Parity   = m_parity;
    switch (m_stopBits) {
        case 1:
            _dcb.StopBits = ONESTOPBIT;
            break;
        case 2:
            _dcb.StopBits = TWOSTOPBITS;
            break;
        default:
            _dcb.StopBits = ONE5STOPBITS;
            break;
    }

    _dcb.fDsrSensitivity = 0;
    // RTS and DTR must be at the same level to avoid driving low either
    // EN or IO0.  DTR must be enabled other USB CDC serial ports will
    // not be in the connected state.  Hence both DTR and RTS must be
    // enabled.

    //    _dcb.fDtrControl  = DTR_CONTROL_ENABLE;
    //    _dcb.fRtsControl  = RTS_CONTROL_ENABLE;
    _dcb.fOutxDsrFlow = 0;

    if (!::SetCommState(m_hCommPort, &_dcb)) {
        ShowError("SetCommState");
        assert(0);
        return false;
    }
    setTimeout(1);
    ResetEvent(m_read_overlapped.hEvent);
    ResetEvent(m_write_overlapped.hEvent);
#if 0
    auto rts = _dcb.fRtsControl == RTS_CONTROL_ENABLE;
    auto dtr = _dcb.fDtrControl == DTR_CONTROL_ENABLE;

    if (dtr && rts) {
        // already in desired state; do nothing
    } else if (dtr && !rts) {
        // IO0 is driven low; assert RTS to release IO0
        // and get to the desired state
        setRtsDtr(true, true);
    } else if (!dtr && rts) {
        // EN is driven low; assert DTR to release EN
        // and get to the desired state
        setRtsDtr(true, true);
    } else {
        // With !dtr && !rts, EN and IO0 are both released,
        // but we want dtr asserted so USB CDC devices will
        // be in connected state, and rts asserted so neither
        // EN nor IO0 is driven low.  If we go directly to
        // rts and dtr true, that often results in a reset
        // (EN low) pulse because Windows drivers sometimes
        // set RTS a few hundred microseconds before setting,
        // DTR, resuiting in a spurious reset pulse.  Explicitly
        // setting DTR first changes it to a pulse on IO0
        // which, though not ideal, is better than a reset.
        setRtsDtr(false, true);
        setRtsDtr(true, true);
    }
#else
    setDtr(true);
    setRts(true);
#endif
    return true;
}

bool SerialPort::Init(std::string portName, DWORD dwBaudRate, BYTE byParity, BYTE byStopBits, BYTE byByteSize) {
    m_portName = portName;
    m_baud     = dwBaudRate;
    m_parity   = byParity;
    m_stopBits = byStopBits;
    m_dataBits = byByteSize;
    m_commName = "\\\\.\\";  // Prefix to convert to NT device name
    m_commName += portName;

    bool hr = false;
    try {
        if (!reOpenPort()) {
            return false;
        }
    } catch (...) {
        assert(0);
        hr = false;
    }
    return true;
}

void SerialPort::getMode(DWORD& dwBaudRate, BYTE& byByteSize, BYTE& byParity, BYTE& byStopBits) {
    dwBaudRate = m_baud;
    byByteSize = m_dataBits;
    byParity   = m_parity;
    byStopBits = m_stopBits;
}

bool SerialPort::setMode(DWORD dwBaudRate, BYTE byByteSize, BYTE byParity, BYTE byStopBits) {
    m_baud     = dwBaudRate;
    m_dataBits = byByteSize;
    m_parity   = byParity;
    m_stopBits = byStopBits;

    _dcb.BaudRate = m_baud;
    _dcb.ByteSize = m_dataBits;
    _dcb.Parity   = m_parity;
    switch (m_stopBits) {
        case 1:
            _dcb.StopBits = ONESTOPBIT;
            break;
        case 2:
            _dcb.StopBits = TWOSTOPBITS;
            break;
        default:
            _dcb.StopBits = ONE5STOPBITS;
            break;
    }

    if (!::SetCommState(m_hCommPort, &_dcb)) {
        ShowError("SetCommState");
        assert(0);
        return false;
    }
    return true;
}

void SerialPort::setRts(bool on) {
    _dcb.fRtsControl = on ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
    ::SetCommState(m_hCommPort, &_dcb);
}

void SerialPort::setDtr(bool on) {
    _dcb.fDtrControl = on ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    ::SetCommState(m_hCommPort, &_dcb);
}

void SerialPort::setRtsDtr(bool rts, bool dtr) {
    _dcb.fRtsControl = rts ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;
    _dcb.fDtrControl = dtr ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    ::SetCommState(m_hCommPort, &_dcb);
}

void SerialPort::restartPort() {
    lock();
    int count = 0;

    errorColor();
    std::cout << "Serial port disconnected - waiting for reconnect" << std::endl;
    normalColor();

    m_end  = 0;
    m_bufp = 0;
    if (m_read_overlapped.hEvent) {
        CloseHandle(m_read_overlapped.hEvent);
        m_read_overlapped.hEvent = nullptr;
    }
    if (m_write_overlapped.hEvent) {
        CloseHandle(m_write_overlapped.hEvent);
        m_write_overlapped.hEvent = nullptr;
    }

    if (m_hCommPort != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hCommPort);
        m_hCommPort = INVALID_HANDLE_VALUE;
    }
    //    Sleep(100);
    while (true) {
        Sleep(10);
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
    if (m_hCommPort == INVALID_HANDLE_VALUE) {
        Sleep(10);
        return 0;
    }

    if (m_direct) {
        return 0;
    }
    if (locked()) {
        Sleep(10);
        return 0;
    }
    size_t copied = copy_out(buf, len);
    if (copied) {
        return copied;
    }
    // We need to use overlapped I/O instead of just depending on read timeouts, because
    // if the COM port is a USB CDC, a USB disconnect is not easy to detect in the ReadFile
    // result.  Sometimes the call just hangs.
    // Initiate an overlapped read
    DWORD dwBytesRead = 0;
    if (!m_readWait) {
        //        if (::ReadFile(m_hCommPort, m_buf, 1024, &dwBytesRead, &m_read_overlapped)) {
        if (::ReadFile(m_hCommPort, m_buf, 1024, &dwBytesRead, NULL)) {
            m_bufp      = 0;
            m_end       = dwBytesRead;
            auto actual = copy_out(buf, len);
            if (!actual) {
                //                std::cout << '>';
            } else {
                // std::cout << 'r';
                // std::cout << (char)m_buf[1] << "," << buf[0];
                //                std::cout << '{' << buf[0] << "}";
            }
            return actual;
        }
        switch (GetLastError()) {
            case ERROR_IO_PENDING:
                m_readWait = true;
                break;
            case ERROR_OPERATION_ABORTED:
                //                restartPort();
                std::cout << "Operation aborted" << std::endl;
                return 0;
            case WAIT_TIMEOUT:
            case ERROR_TIMEOUT:
                std::cout << ':';
                // Do not reissue the read on a timeout from,GetOverlappedResult
                return 0;
            case ERROR_BAD_COMMAND:
            case ERROR_ACCESS_DENIED:
                //            case ERROR_OPERATION_ABORTED:
                //                CancelIo(m_hCommPort);
                std::cout << "First one" << std::endl;
                restartPort();
                m_readWait = false;
                return 0;
            default:
                ShowError("ReadFile");
                m_readWait = false;
                return -1;
        }
    }
    if (m_readWait) {
        std::cout << 'o';
        if (GetOverlappedResultEx(m_hCommPort, &m_read_overlapped, &dwBytesRead, 1000, FALSE)) {
            m_bufp      = 0;
            m_end       = dwBytesRead;
            auto actual = copy_out(buf, len);
            if (!actual) {
                std::cout << 'T';
                // This rarely happen because setTimeout has
                // ReadTotalTimeoutConstant = MAXDWORD, so the overlapped read
                // should not time out by itself; the timeout happens in
                // GetOverlappedResultEx, which then returns false.
                // I have seen cases where it happens with USB-CDC devices
                m_readWait = false;
            } else {
                std::cout << "R " << actual << " " << std::string_view((char*)m_buf, dwBytesRead) << std::endl << std::endl;
            }
            return actual;
        }
        switch (GetLastError()) {
            case ERROR_OPERATION_ABORTED:
                //                restartPort();
                std::cout << "aborted 2" << std::endl;
                m_readWait = false;
                return 0;
            case WAIT_TIMEOUT:
            case ERROR_TIMEOUT:
                // std::cout << '+';
                // Do not reissue the read on a timeout from,GetOverlappedResult
                return 0;
            case ERROR_BAD_COMMAND:
            case ERROR_ACCESS_DENIED:
                //            case ERROR_OPERATION_ABORTED:
                m_readWait = false;
                std::cout << "Second one" << std::endl;
                restartPort();
                setTimeout(1);
                m_readWait = false;
                return 0;
            default:
                ShowError("GetOverLappedResult error");
                m_readWait = false;
                return -1;
        }
    }
    return 0;
}

HRESULT SerialPort::write(const char* data, DWORD dwSize) {
    if (m_hCommPort == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (locked()) {
        // discard data while port is being reopened
        //        std::cout << "<";
        return S_OK;
    }
    int   iRet           = 0;
    DWORD dwBytesWritten = 0;

    iRet = WriteFile(m_hCommPort, data, dwSize, &dwBytesWritten, NULL);
    if (iRet == 0) {
        if (m_hCommPort != INVALID_HANDLE_VALUE && GetLastError() == ERROR_BAD_COMMAND) {
            //            fprintf(stderr, "Write to serial port failed; reopening\n");
            //            restartPort();
            //            restoreConsoleModes();
            //            exit(0);
        }
    }

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
        std::cin.ignore();  // Consume the rest of the line including the newline
        if (choice < choices) {
            comName = "COM" + std::to_string(ports[choice]);
            return true;
        }
    }

    return false;
}
