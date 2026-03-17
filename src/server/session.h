#pragma once

#include <string>

class UserRegistry; // forward declaration

// Constants
constexpr int POLL_TIMEOUT_MS = 10000; // 10 seconds poll timeout

// Main session handler — runs in its own thread.
// Owns the client_fd lifecycle (will close it on exit).
// client_ip is the authoritative IP from accept().
void handle_session(int client_fd, const std::string &client_ip, UserRegistry &registry);
