#include "input_handler.h"
#include "../common/net_utils.h"

// Encabezados protobuf generados
#include "message_general.pb.h"
#include "message_dm.pb.h"
#include "change_status.pb.h"
#include "list_users.pb.h"
#include "get_user_info.pb.h"
#include "quit.pb.h"

#include <iostream>
#include <sstream>
#include <string>

void print_help()
{
    std::cout << "\nComandos disponibles" << std::endl;
    std::cout << "/dm <usuario> <mensaje>   Mensaje directo a un usuario" << std::endl;
    std::cout << "/status <1|2|3>           Cambiar status (1=ACTIVO, 2=OCUPADO, 3=INACTIVO)" << std::endl;
    std::cout << "/list                     Listar usuarios conectados" << std::endl;
    std::cout << "/info <usuario>           Ver info de un usuario" << std::endl;
    std::cout << "/help                     Mostrar esta ayuda" << std::endl;
    std::cout << "/quit                     Desconectarse y salir" << std::endl;
    std::cout << "<mensaje>                 Enviar mensaje al chat general" << std::endl;
    std::cout << "\n"
              << std::endl;
}

void input_loop(int sockfd, const std::string &username, const std::string &ip)
{
    print_help();
    std::string line;

    while (true)
    {
        std::cout << "> ";
        std::cout.flush();

        if (!std::getline(std::cin, line))
            break; // EOF o Ctrl+D
        if (line.empty())
            continue;

        // Comando /quit
        if (line == "/quit")
        {
            chat::Quit msg;
            msg.set_quit(true);
            msg.set_ip(ip);

            std::string payload;
            msg.SerializeToString(&payload);
            send_message(sockfd, MSG_QUIT, payload);
            std::cout << "[Sistema]: Desconectado. Hasta luego." << std::endl;
            break;
        }

        // Comando /list
        if (line == "/list")
        {
            chat::ListUsers msg;
            msg.set_username(username);
            msg.set_ip(ip);

            std::string payload;
            msg.SerializeToString(&payload);
            send_message(sockfd, MSG_LIST_USERS, payload);
            continue;
        }

        // Comando /help
        if (line == "/help")
        {
            print_help();
            continue;
        }

        // Comando /dm
        if (line.rfind("/dm ", 0) == 0)
        {
            std::istringstream ss(line.substr(4));
            std::string target;
            std::string rest;
            ss >> target;
            std::getline(ss, rest);
            if (!rest.empty() && rest[0] == ' ')
                rest = rest.substr(1);

            if (target.empty() || rest.empty())
            {
                std::cout << "[Error]: Uso: /dm <usuario> <mensaje>" << std::endl;
                continue;
            }

            chat::MessageDM msg;
            msg.set_message(rest);
            msg.set_status(chat::ACTIVE); // el servidor mantiene el estado real
            msg.set_username_des(target);
            msg.set_ip(ip);

            std::string payload;
            msg.SerializeToString(&payload);
            send_message(sockfd, MSG_DM, payload);
            continue;
        }

        // Comando /status
        if (line.rfind("/status ", 0) == 0)
        {
            std::string arg = line.substr(8);
            chat::StatusEnum new_status;

            if (arg == "1")
                new_status = chat::ACTIVE;
            else if (arg == "2")
                new_status = chat::DO_NOT_DISTURB;
            else if (arg == "3")
                new_status = chat::INVISIBLE;
            else
            {
                std::cout << "[Error]: Status inválido. Usa 1 (ACTIVO), 2 (OCUPADO) o 3 (INACTIVO)" << std::endl;
                continue;
            }

            chat::ChangeStatus msg;
            msg.set_status(new_status);
            msg.set_username(username);
            msg.set_ip(ip);

            std::string payload;
            msg.SerializeToString(&payload);
            send_message(sockfd, MSG_CHANGE_STATUS, payload);
            continue;
        }

        // Comando /info
        if (line.rfind("/info ", 0) == 0)
        {
            std::string target = line.substr(6);
            if (target.empty())
            {
                std::cout << "[Error]: Uso: /info <usuario>" << std::endl;
                continue;
            }

            chat::GetUserInfo msg;
            msg.set_username_des(target);
            msg.set_username(username);
            msg.set_ip(ip);

            std::string payload;
            msg.SerializeToString(&payload);
            send_message(sockfd, MSG_GET_USER_INFO, payload);
            continue;
        }

        // Mensaje broadcast
        {
            chat::MessageGeneral msg;
            msg.set_message(line);
            msg.set_status(chat::ACTIVE);
            msg.set_username_origin(username);
            msg.set_ip(ip);

            std::string payload;
            msg.SerializeToString(&payload);
            send_message(sockfd, MSG_GENERAL, payload);
        }
    }
}