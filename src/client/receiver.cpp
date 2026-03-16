#include "receiver.h"
#include "../common/net_utils.h"

// Encabezados protobuf generados
#include "server_response.pb.h"
#include "all_users.pb.h"
#include "for_dm.pb.h"
#include "broadcast_messages.pb.h"
#include "get_user_info_response.pb.h"

#include <iostream>
#include <thread>
#include <string>

// Convierte StatusEnum a texto legible
static std::string status_str(int s)
{
    switch (s)
    {
    case 0:
        return "ACTIVO";
    case 1:
        return "OCUPADO";
    case 2:
        return "INACTIVO";
    default:
        return "DESCONOCIDO";
    }
}

static void receiver_loop(int sockfd)
{
    MessageType type;
    std::string payload;

    while (recv_message(sockfd, type, payload))
    {
        switch (type)
        {

        case MSG_BROADCAST_DELIVERY:
        {
            chat::BroadcastDelivery msg;
            if (msg.ParseFromString(payload))
            {
                std::cout << "\n[" << msg.username_origin() << "]: "
                          << msg.message() << std::endl;
            }
            break;
        }

        case MSG_FOR_DM:
        {
            chat::ForDm msg;
            if (msg.ParseFromString(payload))
            {
                std::cout << "\n[DM de " << msg.username_des() << "]: "
                          << msg.message() << std::endl;
            }
            break;
        }

        case MSG_SERVER_RESPONSE:
        {
            chat::ServerResponse msg;
            if (msg.ParseFromString(payload))
            {
                if (!msg.is_successful())
                {
                    std::cout << "\n[ERROR " << msg.status_code() << "]: "
                              << msg.message() << std::endl;
                }
                else
                {
                    std::cout << "\n[OK]: " << msg.message() << std::endl;
                }
            }
            break;
        }

        case MSG_ALL_USERS:
        {
            chat::AllUsers msg;
            if (msg.ParseFromString(payload))
            {
                std::cout << "\n--- Usuarios conectados ---" << std::endl;
                for (int i = 0; i < msg.usernames_size(); i++)
                {
                    std::cout << "  " << msg.usernames(i)
                              << " [" << status_str(msg.status(i)) << "]"
                              << std::endl;
                }
                std::cout << "---------------------------" << std::endl;
            }
            break;
        }

        case MSG_GET_USER_INFO_RESP:
        {
            chat::GetUserInfoResponse msg;
            if (msg.ParseFromString(payload))
            {
                std::cout << "\n--- Info de usuario ---" << std::endl;
                std::cout << "  Usuario: " << msg.username() << std::endl;
                std::cout << "  IP:      " << msg.ip_address() << std::endl;
                std::cout << "  Status:  " << status_str(msg.status()) << std::endl;
                std::cout << "-----------------------" << std::endl;
            }
            break;
        }

        default:
            std::cerr << "\n[WARN]: Tipo de mensaje desconocido: "
                      << static_cast<int>(type) << std::endl;
            break;
        }
    }

    std::cout << "\n[Sistema]: Conexión con el servidor cerrada." << std::endl;
}

void start_receiver(int sockfd)
{
    std::thread t(receiver_loop, sockfd);
    t.detach(); // corre independientemente en segundo plano
}