// config.cpp
#include "config.h"

const char* ssid = "Tenda_C2E100";        // Define network ssid
const char* password = "12345678"; // Define password

char studentAppIP[16] = "192.168.137.200";  // IP address of the student app
const char* endpoint = "/receiveUID";  // endpoint for sending detected RFID IDs