#include <iostream>
#include <string>
#include <cstring>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#include "../common/net_utils.h"
#include "receiver.h"
#include "input_handler.h"

// Encabezados protobuf generados
#include "register.pb.h"
#include "server_response.pb.h"

// Uso: ./client <username> <server_ip> <server_port>
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Uso: " << argv[0] << " <username> <server_ip> <server_port>" << std::endl;
        return 1;
    }

    std::string username = argv[1];
    std::string server_ip = argv[2];
    int server_port = std::stoi(argv[3]);

    // 1. Crear socket TCP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    // 2. Conectar al servidor
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0)
    {
        std::cerr << "IP inválida: " << server_ip << std::endl;
        return 1;
    }

    if (connect(sockfd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
    {
        perror("connect");
        return 1;
    }

    std::cout << "Conectado al servidor " << server_ip << ":" << server_port << std::endl;

    // 3. Obtener IP local
    sockaddr_in local_addr{};
    socklen_t local_len = sizeof(local_addr);
    getsockname(sockfd, reinterpret_cast<sockaddr *>(&local_addr), &local_len);
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));
    std::string my_ip(local_ip);

    // 4. Registrarse con el servidor
    {
        chat::Register reg;
        reg.set_username(username);
        reg.set_ip(my_ip);

        std::string payload;
        reg.SerializeToString(&payload);
        if (!send_message(sockfd, MSG_REGISTER, payload))
        {
            std::cerr << "Error enviando registro." << std::endl;
            close(sockfd);
            return 1;
        }
    }

    // 5. Esperar respuesta del servidor para confirmar registro
    {
        MessageType type;
        std::string payload;
        if (!recv_message(sockfd, type, payload))
        {
            std::cerr << "Sin respuesta del servidor." << std::endl;
            close(sockfd);
            return 1;
        }

        if (type == MSG_SERVER_RESPONSE)
        {
            chat::ServerResponse resp;
            resp.ParseFromString(payload);
            if (!resp.is_successful())
            {
                std::cerr << "[Error de registro]: " << resp.message() << std::endl;
                close(sockfd);
                return 1;
            }
            std::cout << "[Sistema]: " << resp.message() << std::endl;
        }
    }

    // 6. Iniciar hilo receptor en segundo plano
    start_receiver(sockfd);

    // 7. Hilo principal: manejar entrada del usuario
    input_loop(sockfd, username, my_ip);

    close(sockfd);
    return 0;
}