#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "session.h"
#include "user_registry.h"

// Constants
constexpr int BACKLOG = 10;
constexpr int INACTIVITY_CHECK_INTERVAL = 30; // seconds between scans
constexpr int INACTIVITY_TIMEOUT = 60;        // seconds before auto-INVISIBLE

// Graceful shutdown support
static int g_server_fd = -1;

void shutdown_handler(int) {
  std::cout << "\n[Server] Shutting down..." << std::endl;
  if (g_server_fd >= 0)
    close(g_server_fd);
}

// Inactivity checker thread function — runs forever, scanning all users
// every INACTIVITY_CHECK_INTERVAL seconds.
void inactivity_checker(UserRegistry &registry) {
  while (true) {
    std::this_thread::sleep_for(
        std::chrono::seconds(INACTIVITY_CHECK_INTERVAL));
    registry.check_inactivity(INACTIVITY_TIMEOUT);
  }
}

int main(int argc, char *argv[]) {
  // 1. Parse command line: ./server <port>
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
    return 1;
  }
  int port = std::atoi(argv[1]);

  // Ignore SIGPIPE globally — belt-and-suspenders with MSG_NOSIGNAL in
  // send_exact()
  signal(SIGPIPE, SIG_IGN);

  // Register graceful shutdown handlers for SIGINT (Ctrl+C) and SIGTERM
  signal(SIGINT, shutdown_handler);
  signal(SIGTERM, shutdown_handler);

  // 2. Create TCP socket
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  // Store server fd for graceful shutdown handler
  g_server_fd = server_fd;

  // 3. Set SO_REUSEADDR for quick restart after crash/kill
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // 4. Bind to 0.0.0.0:port
  struct sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  // 5. Listen
  if (listen(server_fd, BACKLOG) < 0) {
    perror("listen");
    close(server_fd);
    return 1;
  }

  std::cout << "[Server] Listening on port " << port << std::endl;

  // 6. Create shared UserRegistry
  UserRegistry registry;

  // 7. Spawn inactivity checker thread (detached — runs forever)
  std::thread(inactivity_checker, std::ref(registry)).detach();

  // 8. Accept loop — runs forever, spawning a session thread per client
  while (true) {
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      break; // accept failed (likely due to signal closing server_fd) — exit
             // loop
    }

    // Get client IP from accepted connection (authoritative, not from Register
    // msg)
    std::string client_ip = inet_ntoa(client_addr.sin_addr);

    std::cout << "[Server] New connection from " << client_ip << std::endl;

    // Spawn session thread (detached — self-cleans on exit)
    // std::ref(registry) required because UserRegistry has a mutex
    // (non-copyable)
    std::thread(handle_session, client_fd, client_ip, std::ref(registry))
        .detach();
  }

  close(server_fd);
  return 0;
}
