#pragma once

#include "SerialPort.h"
#include <fstream>

int sendGCode(SerialPort& serial, std::ifstream& in);
