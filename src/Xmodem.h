#pragma once

#include "SerialThread.h"
#include <fstream>

int xmodemReceive(SerialThread& serial, std::ostream& out);
int xmodemTransmit(SerialThread& serial, std::ifstream& in);
