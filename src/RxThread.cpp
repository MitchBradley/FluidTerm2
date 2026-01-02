#include "Colorize.h"
#include "Console.h"
#include <assert.h>
#include "SerialPort.h"
#include <thread>

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

static SerialPort* port;

static uint32_t saved_baudrate;
static uint8_t  saved_data_bits;
static uint8_t  saved_parity;
static uint8_t  saved_stop_bits;

void changeMode(uint32_t baudRate, const char* mode) {
    // XXX need to cancel pending IO

    // mode is like "8n1"
    port->getMode(saved_baudrate, saved_data_bits, saved_parity, saved_stop_bits);

    uint8_t data_bits = mode[0] - '0';
    uint8_t parity    = mode[1] == 'e' ? 2 : (mode[1] == 'o' ? 1 : 0);
    uint8_t stop_bits = mode[2] - '0';

    port->setMode(baudRate, data_bits, parity, stop_bits);
}

void restoreMode() {
    if (saved_baudrate) {
        // XXX need to cancel pending IO
        port->setMode(saved_baudrate, saved_data_bits, saved_parity, saved_stop_bits);
    }
}

void queuedReception() {
    redirected = true;
}
void normalReception() {
    redirected = false;
}

class Reader {
public:
    Reader(SerialPort* port) {
        _rxThread = std::jthread { [port](std::stop_token stopper) {
            while (!stopper.stop_requested()) {
                static int timeouts  = 0;
                size_t     bytesRead = 0;
                char       tmp[128];

#if 0
            if (redirected) {
                if (queuedData.empty()) {
                    bytesRead = port->read(tmp, sizeof(tmp));
                    if (bytesRead > 0) {
                        std::unique_lock<std::mutex> lock(queuedMutex);
                        queuedData.append(tmp, bytesRead);
                        queuedCond.notify_one();  // Notify the waiting consumer
                    }
                } else {
                    sleep_ms(50);
                }
            } else {
                bytesRead = port->read(tmp, sizeof(tmp));
                if (bytesRead > 0) {
                    colorizeOutput(tmp, bytesRead);
                } else {
                    // Get someone else a chance
                    sleep_ms(50);
                }
            }
#else
                try {
                    bytesRead = port->read(tmp, sizeof(tmp));
                    if (bytesRead > 0) {
                        if (redirected) {
                            std::unique_lock<std::mutex> lock(queuedMutex);
                            queuedData.append(tmp, bytesRead);
                            queuedCond.notify_one();  // Notify the waiting consumer
                        } else {
                            colorizeOutput(tmp, bytesRead);
                        }
                    } else {
                        sleep_ms(50);
                    }
                } catch (std::exception ex) {
                    std::cerr << "Serial port exception " << ex.what();
                    break;
                }
#endif
            }
        } };
    }
    ~Reader() {
        _rxThread.request_stop();
        _rxThread.join();
    }

private:
    std::jthread _rxThread;

    // ... shared variables and synchronization primitives (mutexes) ...
};

static Reader* reader = nullptr;

void startRxThread(SerialPort* serialp) {
    port   = serialp;
    reader = new Reader(serialp);
}

void endRxThread() {
    delete reader;
}
