#include "session.h"
#include "../common/net_utils.h"

#include "register.pb.h"
#include "message_general.pb.h"
#include "message_dm.pb.h"
#include "change_status.pb.h"
#include "list_users.pb.h"
#include "get_user_info.pb.h"
#include "quit.pb.h"

#include "server_response.pb.h"
#include "all_users.pb.h"
#include "for_dm.pb.h"
#include "broadcast_messages.pb.h"
#include "get_user_info_response.pb.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <unistd.h>

static constexpr int SC_OK       = 200;
static constexpr int SC_ERROR    = 400;
static constexpr int SC_NOT_FOUND= 404;
static constexpr int SC_CONFLICT = 409;

static constexpr int INACTIVITY_SECONDS = 60;

static chat::StatusEnum to_proto_status(UserStatus s)
{
    switch (s) {
    case UserStatus::ACTIVE:         return chat::ACTIVE;
    case UserStatus::DO_NOT_DISTURB: return chat::DO_NOT_DISTURB;
    case UserStatus::INVISIBLE:      return chat::INVISIBLE;
    }
    return chat::ACTIVE;
}

static UserStatus from_proto_status(chat::StatusEnum s)
{
    switch (s) {
    case chat::ACTIVE:         return UserStatus::ACTIVE;
    case chat::DO_NOT_DISTURB: return UserStatus::DO_NOT_DISTURB;
    case chat::INVISIBLE:      return UserStatus::INVISIBLE;
    default:                   return UserStatus::ACTIVE;
    }
}

static void send_response(int sockfd, int code, const std::string &msg, bool ok)
{
    chat::ServerResponse resp;
    resp.set_status_code(code);
    resp.set_message(msg);
    resp.set_is_successful(ok);
    std::string payload;
    resp.SerializeToString(&payload);
    send_message(sockfd, MSG_SERVER_RESPONSE, payload);
}

static void broadcast(UserRegistry &registry, int sender_fd,
                      const std::string &origin, const std::string &text)
{
    chat::BroadcastDelivery msg;
    msg.set_message(text);
    msg.set_username_origin(origin);
    std::string payload;
    msg.SerializeToString(&payload);
    for (const auto &[name, info] : registry.all_users())
        if (info.sockfd != sender_fd)
            send_message(info.sockfd, MSG_BROADCAST_DELIVERY, payload);
}

void handle_session(int sockfd, UserRegistry &registry)
{
    // ── Step 1: registration ───────────────────────────────────────────────
    MessageType type;
    std::string payload;

    if (!recv_message(sockfd, type, payload) || type != MSG_REGISTER) {
        send_response(sockfd, SC_ERROR, "Se esperaba un mensaje de registro.", false);
        close(sockfd);
        return;
    }

    chat::Register reg;
    if (!reg.ParseFromString(payload)) {
        send_response(sockfd, SC_ERROR, "Mensaje de registro malformado.", false);
        close(sockfd);
        return;
    }

    std::string username = reg.username();
    std::string ip       = reg.ip();

    if (!registry.register_user(username, ip, sockfd)) {
        send_response(sockfd, SC_CONFLICT,
                      "El nombre de usuario o IP ya está registrado.", false);
        close(sockfd);
        return;
    }

    send_response(sockfd, SC_OK, "Bienvenid@ al chat, " + username + "!", true);
    std::cout << "[Server] Usuario conectado: " << username
              << " (" << ip << ")" << std::endl;

    // ── Step 2: inactivity watcher using shared_ptr so it outlives session ─
    auto last_activity = std::make_shared<std::atomic<time_t>>(std::time(nullptr));
    auto running       = std::make_shared<std::atomic<bool>>(true);

    std::thread watcher([last_activity, running, username, sockfd, &registry]() {
        while (running->load()) {
            // Sleep in 1s increments so we can react quickly to running=false
            for (int i = 0; i < 5 && running->load(); i++)
                std::this_thread::sleep_for(std::chrono::seconds(1));

            if (!running->load()) break;

            time_t elapsed = std::time(nullptr) - last_activity->load();
            if (elapsed >= INACTIVITY_SECONDS && running->load()) {
                UserInfo info;
                if (registry.get_user(username, info) &&
                    info.status == UserStatus::ACTIVE &&
                    running->load())
                {
                    registry.set_status(username, UserStatus::INVISIBLE);
                    send_response(sockfd, SC_OK,
                                  "Tu status fue cambiado a INACTIVO por inactividad.", true);
                }
            }
        }
    });
    watcher.detach();

    // ── Step 3: message loop ───────────────────────────────────────────────
    while (recv_message(sockfd, type, payload)) {
        last_activity->store(std::time(nullptr));

        // Restore ACTIVE if was INACTIVE
        {
            UserInfo info;
            if (registry.get_user(username, info) &&
                info.status == UserStatus::INVISIBLE)
            {
                registry.set_status(username, UserStatus::ACTIVE);
                send_response(sockfd, SC_OK, "Tu status fue restaurado a ACTIVO.", true);
            }
        }

        switch (type) {

        case MSG_GENERAL: {
            chat::MessageGeneral msg;
            if (!msg.ParseFromString(payload)) break;
            broadcast(registry, sockfd, username, msg.message());
            break;
        }

        case MSG_DM: {
            chat::MessageDM msg;
            if (!msg.ParseFromString(payload)) break;

            UserInfo dest_info;
            if (!registry.get_user(msg.username_des(), dest_info)) {
                send_response(sockfd, SC_NOT_FOUND,
                              "Usuario '" + msg.username_des() + "' no encontrado.", false);
                break;
            }

            chat::ForDm fwd;
            fwd.set_username_des(username);
            fwd.set_message(msg.message());
            std::string fwd_payload;
            fwd.SerializeToString(&fwd_payload);
            send_message(dest_info.sockfd, MSG_FOR_DM, fwd_payload);
            send_response(sockfd, SC_OK, "Mensaje enviado.", true);
            break;
        }

        case MSG_CHANGE_STATUS: {
            chat::ChangeStatus msg;
            if (!msg.ParseFromString(payload)) break;
            registry.set_status(username, from_proto_status(msg.status()));
            send_response(sockfd, SC_OK, "Status actualizado.", true);
            break;
        }

        case MSG_LIST_USERS: {
            chat::AllUsers resp;
            for (const auto &[name, info] : registry.all_users()) {
                resp.add_usernames(name);
                resp.add_status(to_proto_status(info.status));
            }
            std::string resp_payload;
            resp.SerializeToString(&resp_payload);
            send_message(sockfd, MSG_ALL_USERS, resp_payload);
            break;
        }

        case MSG_GET_USER_INFO: {
            chat::GetUserInfo msg;
            if (!msg.ParseFromString(payload)) break;

            UserInfo info;
            if (!registry.get_user(msg.username_des(), info)) {
                send_response(sockfd, SC_NOT_FOUND,
                              "Usuario '" + msg.username_des() + "' no encontrado.", false);
                break;
            }

            chat::GetUserInfoResponse resp;
            resp.set_ip_address(info.ip);
            resp.set_username(msg.username_des());
            resp.set_status(to_proto_status(info.status));
            std::string resp_payload;
            resp.SerializeToString(&resp_payload);
            send_message(sockfd, MSG_GET_USER_INFO_RESP, resp_payload);
            break;
        }

        case MSG_QUIT:
            std::cout << "[Server] Usuario desconectado: " << username << std::endl;
            goto cleanup;

        default:
            std::cerr << "[Server] Tipo desconocido: "
                      << static_cast<int>(type) << std::endl;
            break;
        }
    }

cleanup:
    running->store(false);
    registry.remove_user(sockfd);
    close(sockfd);
    std::cout << "[Server] Sesión cerrada: " << username << std::endl;
}