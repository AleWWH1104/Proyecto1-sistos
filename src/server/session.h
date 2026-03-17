#pragma once

#include "user_registry.h"

// Runs the full lifecycle of one connected client.
// Called in its own std::thread by server.cpp for every accepted connection.
//
// sockfd    : the accepted socket file descriptor
// registry  : shared user registry (thread-safe)
void handle_session(int sockfd, UserRegistry &registry);