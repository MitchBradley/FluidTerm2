#pragma once
#include "SerialPort.h"
#include "RxThread.h"
#include <iostream>

void sendGCode(SerialPort* serial, std::ifstream& infile);
