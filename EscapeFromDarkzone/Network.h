#pragma once
#include <WS2tcpip.h>

#pragma comment (lib, "WS2_32.LIB")

#define BUF_SIZE 1000

constexpr short SERVER_PORT = 3000;
constexpr char SERVER_ADDR[] = "127.0.0.1";
