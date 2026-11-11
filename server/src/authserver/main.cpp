#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <mysql.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libmysql.lib")
#pragma comment(lib, "libcrypto.lib")

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <ctime>
#include <algorithm>

#include "AuthShared.h"
#include "Network.h"
#include "Opcodes.h"
#include "SRP6.h"