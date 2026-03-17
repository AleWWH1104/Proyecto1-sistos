#include "session.h"
#include "../common/net_utils.h"

// protobuf – client → server
#include "register.pb.h"
#include "message_general.pb.h"
#include "message_dm.pb.h"
#include "change_status.pb.h"
#include "list_users.pb.h"
#include "get_user_info.pb.h"
#include "quit.pb.h"

// protobuf – server → client
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
#include <unistd.h>

// ─── Status codes used in ServerResponse ───────────────────────────────────
static constexpr int SC_OK               = 200;
static constexpr int SC_ERROR            = 400;
static constexpr int SC_NOT_FOUND        = 404;
static constexpr int SC_CONFLICT         = 409;

// ─── Inactivity timeout ────────────────────────────────────────────────────
// Seconds of silence before the server marks a user INACTIVE.
// Keep short for easier grading; change here only.
static constexpr int INACTIVITY_SECONDS = 60;

// ─── Helpers ──────────────────────────────────────────────────────────────

static chat::StatusEnum to_proto_status(UserStatus s)
{
    switch (s)
    {
    case UserStatus::ACTIVE:         return chat::ACTIVE;
    case UserStatus::DO_NOT_DISTURB: return chat::DO_NOT_DISTURB;
    case UserStatus::INVISIBLE:      return chat::INVISIBLE;
    }
    return chat::ACTIVE;
}

static UserStatus from_proto_status(chat::StatusEnum s)
{
    switch (s)
    {
    case chat::ACTIVE:         return UserStatus::ACTIVE;
    case chat::DO_NOT_DISTURB: return UserStatus::DO_NOT_DISTURB;
    case chat::INVISIBLE:      return UserStatus::INVISIBLE;
    default:                   return UserStatus::ACTIVE;
    }
}

// Send a generic ServerResponse to one socket.
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

// Broadcast a BroadcastDelivery to every connected user except the sender.
static void broadcast(UserRegistry &registry,
                      int           sender_fd,
                      const std::string &origin,
                      const std::string &text)
{
    chat::BroadcastDelivery msg;
    msg.set_message(text);
    msg.set_username_origin(origin);

    std::string payload;
    msg.SerializeToString(&payload);

    for (const auto &[name, info] : registry.all_users())
    {
        if (info.sockfd != sender_fd)
            send_message(info.sockfd, MSG_BROADCAST_DELIVERY, payload);
    }
}

// ─── Inactivity timer thread ───────────────────────────────────────────────
// Runs alongside the session loop.  Resets every time `last_activity` is
// updated.  When timeout fires, sets status to INVISIBLE (INACTIVO) and
// notifies the client with a ServerResponse so its UI updates.
static void inactivity_watcher(const std::string           &username,
                                int                          sockfd,
                                UserRegistry                &registry,
                                std::atomic<time_t>         &last_activity,
                                std::atomic<bool>           &running)
{
    while (running.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!running.load())
            break;

        time_t now     = std::time(nullptr);
        time_t elapsed = now - last_activity.load();

        if (elapsed >= INACTIVITY_SECONDS)
        {
            // Only change status if currently ACTIVE
            UserInfo info;
            if (registry.get_user(username, info) &&
                info.status == UserStatus::ACTIVE)
            {
                registry.set_status(username, UserStatus::INVISIBLE);
                send_response(sockfd, SC_OK,
                              "Tu status fue cambiado a INACTIVO por inactividad.", true);
            }
        }
    }
}

// ─── Main session handler ──────────────────────────────────────────────────
void handle_session(int sockfd, UserRegistry &registry)
{
    // ── Step 1: Expect a Register message ──────────────────────────────────
    MessageType type;
    std::string payload;

    if (!recv_message(sockfd, type, payload) || type != MSG_REGISTER)
    {
        send_response(sockfd, SC_ERROR, "Se esperaba un mensaje de registro.", false);
        close(sockfd);
        return;
    }

    chat::Register reg;
    if (!reg.ParseFromString(payload))
    {
        send_response(sockfd, SC_ERROR, "Mensaje de registro malformado.", false);
        close(sockfd);
        return;
    }

    std::string username = reg.username();
    std::string ip       = reg.ip();

    if (!registry.register_user(username, ip, sockfd))
    {
        send_response(sockfd, SC_CONFLICT,
                      "El nombre de usuario o IP ya está registrado.", false);
        close(sockfd);
        return;
    }

    send_response(sockfd, SC_OK,
                  "Bienvenid@ al chat, " + username + "!", true);
    std::cout << "[Server] Usuario conectado: " << username
              << " (" << ip << ")" << std::endl;

    // ── Step 2: Start inactivity watcher ───────────────────────────────────
    std::atomic<time_t> last_activity{std::time(nullptr)};
    std::atomic<bool>   running{true};

    std::thread watcher(inactivity_watcher,
                        std::ref(username),
                        sockfd,
                        std::ref(registry),
                        std::ref(last_activity),
                        std::ref(running));
    watcher.detach();

    // ── Step 3: Message dispatch loop ──────────────────────────────────────
    while (recv_message(sockfd, type, payload))
    {
        last_activity.store(std::time(nullptr));

        // If user was INACTIVE, restore to ACTIVE on any message
        {
            UserInfo info;
            if (registry.get_user(username, info) &&
                info.status == UserStatus::INVISIBLE)
            {
                registry.set_status(username, UserStatus::ACTIVE);
                send_response(sockfd, SC_OK,
                              "Tu status fue restaurado a ACTIVO.", true);
            }
        }

        switch (type)
        {

        // ── Broadcast message ────────────────────────────────────────────
        case MSG_GENERAL:
        {
            chat::MessageGeneral msg;
            if (!msg.ParseFromString(payload)) break;

            broadcast(registry, sockfd, username, msg.message());
            break;
        }

        // ── Direct message ───────────────────────────────────────────────
        case MSG_DM:
        {
            chat::MessageDM msg;
            if (!msg.ParseFromString(payload)) break;

            std::string dest = msg.username_des();
            UserInfo    dest_info;

            if (!registry.get_user(dest, dest_info))
            {
                send_response(sockfd, SC_NOT_FOUND,
                              "Usuario '" + dest + "' no encontrado.", false);
                break;
            }

            // Forward to destination
            chat::ForDm fwd;
            fwd.set_username_des(username); // who sent it
            fwd.set_message(msg.message());

            std::string fwd_payload;
            fwd.SerializeToString(&fwd_payload);
            send_message(dest_info.sockfd, MSG_FOR_DM, fwd_payload);

            send_response(sockfd, SC_OK, "Mensaje enviado.", true);
            break;
        }

        // ── Change status ────────────────────────────────────────────────
        case MSG_CHANGE_STATUS:
        {
            chat::ChangeStatus msg;
            if (!msg.ParseFromString(payload)) break;

            UserStatus new_status = from_proto_status(msg.status());
            registry.set_status(username, new_status);

            send_response(sockfd, SC_OK, "Status actualizado.", true);
            break;
        }

        // ── List users ───────────────────────────────────────────────────
        case MSG_LIST_USERS:
        {
            chat::AllUsers resp;
            for (const auto &[name, info] : registry.all_users())
            {
                resp.add_usernames(name);
                resp.add_status(to_proto_status(info.status));
            }

            std::string resp_payload;
            resp.SerializeToString(&resp_payload);
            send_message(sockfd, MSG_ALL_USERS, resp_payload);
            break;
        }

        // ── Get user info ────────────────────────────────────────────────
        case MSG_GET_USER_INFO:
        {
            chat::GetUserInfo msg;
            if (!msg.ParseFromString(payload)) break;

            UserInfo info;
            if (!registry.get_user(msg.username_des(), info))
            {
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

        // ── Quit ─────────────────────────────────────────────────────────
        case MSG_QUIT:
        {
            std::cout << "[Server] Usuario desconectado: " << username << std::endl;
            goto cleanup; // break out of the while loop
        }

        default:
            std::cerr << "[Server] Tipo de mensaje desconocido: "
                      << static_cast<int>(type) << std::endl;
            break;
        }
    }

cleanup:
    running.store(false);
    registry.remove_user(sockfd);
    close(sockfd);
    std::cout << "[Server] Sesión cerrada: " << username << std::endl;
}