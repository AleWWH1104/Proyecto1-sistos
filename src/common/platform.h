#pragma once
// ============================================================================
// platform.h — Cross-platform socket & signal compatibility layer
//
// Supports: Linux, macOS, Windows (MinGW / MSYS2)
//
// Usage: Replace all POSIX socket/signal includes with:
//     #include "common/platform.h"
// ============================================================================

#ifdef _WIN32
// ---- Windows (MinGW / MSYS2) ----
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include <cstdint>
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#ifdef _WIN64
typedef int64_t ssize_t;
#else
typedef int32_t ssize_t;
#endif
#endif

inline int platform_close_socket(int fd) {
    return closesocket(static_cast<SOCKET>(fd));
}

#define poll WSAPoll

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef SIGPIPE
#define SIGPIPE 13
#endif

#include <cerrno>
#ifndef EINTR
#define EINTR WSAEINTR
#endif

// RAII class for WSAStartup/WSACleanup
class WinsockInit {
public:
    WinsockInit() {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
    }
    ~WinsockInit() {
        WSACleanup();
    }
    WinsockInit(const WinsockInit&) = delete;
    WinsockInit& operator=(const WinsockInit&) = delete;
};

#else
// ---- POSIX (Linux / macOS) ----
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>

inline int platform_close_socket(int fd) {
    return close(fd);
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// No-op class for POSIX
class WinsockInit {
public:
    WinsockInit() = default;
};

#endif // _WIN32
