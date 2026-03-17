#include <iostream>
#include <thread>
#include <string>
#include <csignal>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "session.h"
#include "user_registry.h"

// Global registry shared across all session threads
static UserRegistry g_registry;

// Graceful shutdown on Ctrl+C
static int g_server_fd = -1;
static void handle_signal(int)
{
    std::cout << "\n[Server] Apagando..." << std::endl;
    if (g_server_fd >= 0)
        close(g_server_fd);
}

// Uso: ./server <puerto>
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Uso: " << argv[0] << " <puerto>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    // ── Create TCP socket ─────────────────────────────────────────────────
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }
    g_server_fd = server_fd;

    // Allow reuse of port immediately after restart
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ── Bind ──────────────────────────────────────────────────────────────
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // ── Listen ────────────────────────────────────────────────────────────
    if (listen(server_fd, 16) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::cout << "[Server] Escuchando en puerto " << port << std::endl;

    // ── Accept loop ───────────────────────────────────────────────────────
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,
                               reinterpret_cast<sockaddr *>(&client_addr),
                               &client_len);
        if (client_fd < 0)
        {
            // accept() fails when the server socket is closed (shutdown)
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "[Server] Nueva conexión entrante desde " << client_ip << std::endl;

        // Spawn a detached thread for this client session
        std::thread t(handle_session, client_fd, std::ref(g_registry));
        t.detach();
    }

    close(server_fd);
    return 0;
}