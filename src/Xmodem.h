#pragma once

#include "SerialPort.h"
#include <fstream>

int xmodemReceive(SerialPort& serial, std::ostream& out);
int xmodemTransmit(SerialPort& serial, std::ifstream& in);
