/*
  stm32flash - Open Source ST STM32 flash program for *nix
  Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "serial.h"
#include "port.h"
#include <../windows/SerialPort.h>

// static uint32_t saved_baudrate;
static unsigned long saved_baudrate;
static uint8_t       saved_data_bits;
static uint8_t       saved_parity;
static uint8_t       saved_stop_bits;

// cppcheck-suppress unusedFunction
static port_err_t serial_open(SerialPort* serial) {
    return PORT_ERR_OK;
}

// cppcheck-suppress unusedFunction
static void serial_flush(const serial_t* h) {}

// cppcheck-suppress unusedFunction
static port_err_t serial_close(struct port_interface* port) {
    auto h = (SerialPort*)port->priv;
    h->setIndirect();
    h->setMode(saved_baudrate, saved_data_bits, saved_parity, saved_stop_bits);
    port->priv = NULL;
    return PORT_ERR_OK;
}

static port_err_t serial_read(struct port_interface* port, void* buf, size_t nbyte) {
    auto     h   = (SerialPort*)port->priv;
    uint8_t* pos = (uint8_t*)buf;

    if (h == NULL) {
        return PORT_ERR_UNKNOWN;
    }

#if 0
    while (nbyte) {
        int byte = h->timedRead(2000);
        if (byte == -1) {
            byte = h->timedRead(2000);
            if (byte == -1) {
                fprintf(stderr, "XXXX\n");
                return PORT_ERR_TIMEDOUT;
            }
        }
        fprintf(stderr, "%02x ", (uint8_t)byte);
        *pos++ = byte;
        --nbyte;
    }
    fprintf(stderr, "\n");
#else
    if (h->timedRead(pos, nbyte, 2000) != nbyte) {
        return PORT_ERR_TIMEDOUT;
    }
#endif
    return PORT_ERR_OK;
}

static port_err_t serial_write(struct port_interface* port, void* buf, size_t nbyte) {
    auto h = (SerialPort*)port->priv;
    if (h == NULL) {
        return PORT_ERR_UNKNOWN;
    }

    h->write((const char*)buf, nbyte);
    return PORT_ERR_OK;
}

static port_err_t serial_gpio(struct port_interface* port, serial_gpio_t n, int level) {
    auto h = (SerialPort*)port->priv;

    if (h == NULL)
        return PORT_ERR_UNKNOWN;

    switch (n) {
        case GPIO_RTS:
            h->setRts(level);
            break;

        case GPIO_DTR:
            h->setDtr(level);
            break;

        case GPIO_BRK:
            if (level == 0)
                return PORT_ERR_OK;
            return PORT_ERR_OK;

        default:
            return PORT_ERR_UNKNOWN;
    }

    return PORT_ERR_OK;
}

static const char* serial_get_cfg_str(struct port_interface* port) {
    auto h = (SerialPort*)port->priv;
    return h ? "FluidNC" : "INVALID";
}

static port_err_t serial_flush(struct port_interface* port) {
    auto h = (SerialPort*)port->priv;
    if (h == NULL) {
        return PORT_ERR_UNKNOWN;
    }

    return PORT_ERR_OK;
}

static port_err_t serial_open(struct port_interface* port, struct port_options* ops) {
    auto h = (SerialPort*)ops->device;
    h->setDirect();
    port->priv = (void*)h;

    h->getMode(saved_baudrate, saved_data_bits, saved_parity, saved_stop_bits);
    const char* mode      = ops->serial_mode;
    uint8_t     data_bits = mode[0] - '0';
    uint8_t     parity    = mode[1] == 'e' ? 2 : (mode[1] == 'o' ? 1 : 9);
    uint8_t     stop_bits = mode[2] - '0';

    h->setMode(ops->baudRate, data_bits, parity, stop_bits);

    return PORT_ERR_OK;
}

struct port_interface port_serial = {
    .name        = "serial_fluidnc",
    .flags       = PORT_BYTE | PORT_GVR_ETX | PORT_CMD_INIT | PORT_RETRY,
    .open        = serial_open,
    .close       = serial_close,
    .flush       = serial_flush,
    .read        = serial_read,
    .write       = serial_write,
    .gpio        = serial_gpio,
    .get_cfg_str = serial_get_cfg_str,
};
