#include "Colorize.h"
#include "Console.h"
#include <assert.h>
#include "SerialPort.h"
#include "RxThread.h"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>

const int queue_timeout_ms = 100;

static std::mutex              queuedMutex;
static std::condition_variable queuedCond;
static std::string             queuedData;

bool redirected = false;

int timedRead(char* buf, size_t len, int timeout_ms) {
    if (queuedData.empty()) {
        std::unique_lock<std::mutex> lock(queuedMutex);
        queuedCond.wait_for(lock, std::chrono::milliseconds(timeout_ms), [] { return !queuedData.empty(); });
    }

    if (queuedData.empty()) {
        return 0;
    }

    //    std::cout << 'Q' << queuedData.length() << std::endl;
    // Copy out data up to the requested length
    auto toCopy = std::min(queuedData.length(), len);
    memcpy(buf, queuedData.data(), toCopy);
    queuedData.erase(0, toCopy);

    return toCopy;
}
int timedRead(int timeout_ms) {
    char c;
    return timedRead(&c, 1, timeout_ms) ? c : -1;
}

static SerialPort* serial;

#if 0
static XModemTransmitter* xModemTransmitter = nullptr;

void startXModem(std::ifstream& infile) {
    xModemTransmitter = new XModemTransmitter(*serial, infile);
}
bool inXModem() {
    return xModemTransmitter != nullptr;
}
#endif

HANDLE hThreadTerm;
HANDLE hThread;
HANDLE hThreadStarted;

static DWORD saved_baudrate;
static BYTE  saved_data_bits;
static BYTE  saved_parity;
static BYTE  saved_stop_bits;

void changeMode(int baudRate, const char* mode) {
    // XXX need to cancel pending IO

    // mode is like "8n1"
    serial->getMode(saved_baudrate, saved_data_bits, saved_parity, saved_stop_bits);

    uint8_t data_bits = mode[0] - '0';
    uint8_t parity    = mode[1] == 'e' ? 2 : (mode[1] == 'o' ? 1 : 0);
    uint8_t stop_bits = mode[2] - '0';

    serial->setMode(baudRate, data_bits, parity, stop_bits);
}

void restoreMode() {
    if (saved_baudrate) {
        // XXX need to cancel pending IO
        serial->setMode(saved_baudrate, saved_data_bits, saved_parity, saved_stop_bits);
    }
}

void queuedReception() {
    redirected = true;
}
void normalReception() {
    redirected = false;
}

unsigned __stdcall ThreadFn(void* pvParam) {
    bool abContinue = true;

    SetEvent(hThreadStarted);
    while (1) {
        static int timeouts    = 0;
        DWORD      dwBytesRead = 0;
        char       szTmp[128];

        if (redirected) {
            if (queuedData.empty()) {
                dwBytesRead = serial->read(szTmp, sizeof(szTmp));
                if (dwBytesRead > 0) {
                    // A zero length string indicates a timeout
                    std::unique_lock<std::mutex> lock(queuedMutex);
                    queuedData.append(szTmp, dwBytesRead);  // Might append nothing
                    queuedCond.notify_one();                // Notify the waiting consumer
                }
            } else {
                Sleep(1);
            }
#if 0
        } else if (gCodeTransmitter) {
            if (gCodeTransmitter->process_bytes((uint8_t*)szTmp, dwBytesRead)) {
                delete gCodeTransmitter;
                gCodeTransmitter = nullptr;
            }
        } else if (xModemTransmitter) {
            bool done = false;
            if (dwBytesRead < 0) {
                done = true;
            } else if (dwBytesRead > 0) {
                for (int i = 0; !done && i < dwBytesRead; i++) {
                    auto ch = szTmp[i];
                    done    = xModemTransmitter->process_byte(ch);
                }
            } else {
                // timeout
                if (++timeouts == 10) {
                    timeouts = 0;
                    done     = xModemTransmitter->handle_timeout();
                }
            }
            if (done) {
                delete xModemTransmitter;
                xModemTransmitter = nullptr;
            }
#endif
        } else {
            dwBytesRead = serial->read(szTmp, sizeof(szTmp));
            // Errors are reported by read()
            if (dwBytesRead > 0) {
                colorizeOutput(szTmp, dwBytesRead);
            } else {
                // Get someone else a chance
                Sleep(1);
            }
        }
    }
    return 0;
}
void InvalidateHandle(HANDLE& hHandle) {
    hHandle = INVALID_HANDLE_VALUE;
}

void startRxThread(SerialPort* serialp) {
    serial = serialp;
    //create thread terminator event...
    hThreadTerm    = CreateEvent(0, 0, 0, 0);
    hThreadStarted = CreateEvent(0, 0, 0, 0);

    hThread = (HANDLE)_beginthreadex(0, 0, ThreadFn, nullptr, 0, 0);

    DWORD dwWait = WaitForSingleObject(hThreadStarted, INFINITE);

    assert(dwWait == WAIT_OBJECT_0);

    CloseHandle(hThreadStarted);

    InvalidateHandle(hThreadStarted);
}

void endRxThread() {
    InvalidateHandle(hThreadTerm);
    InvalidateHandle(hThread);
    InvalidateHandle(hThreadStarted);
}
