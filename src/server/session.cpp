#include "server/session.h"
#include "common/net_utils.h"
#include "server/user_registry.h"

#include "common/platform.h"

#include <iostream>
#include <string>

// Protobuf headers (all generated flat in gen/)
#include "all_users.pb.h"
#include "broadcast_messages.pb.h"
#include "change_status.pb.h"
#include "for_dm.pb.h"
#include "get_user_info.pb.h"
#include "get_user_info_response.pb.h"
#include "list_users.pb.h"
#include "message_dm.pb.h"
#include "message_general.pb.h"
#include "quit.pb.h"
#include "register.pb.h"
#include "server_response.pb.h"

// ---------------------------------------------------------------------------
// Helper: send a ServerResponse back to a client
// ---------------------------------------------------------------------------
static void send_server_response(int fd, int status_code,
                                 const std::string &message,
                                 bool is_successful) {
  chat::ServerResponse resp;
  resp.set_status_code(status_code);
  resp.set_message(message);
  resp.set_is_successful(is_successful);

  std::string payload;
  resp.SerializeToString(&payload);
  send_message(fd, MSG_SERVER_RESPONSE, payload);
}

// ---------------------------------------------------------------------------
// Main session handler
// ---------------------------------------------------------------------------
void handle_session(int client_fd, const std::string &client_ip,
                    UserRegistry &registry) {
  // =======================================================================
  // Phase 1: Registration — first message MUST be MSG_REGISTER (type 1)
  // =======================================================================
  MessageType type;
  std::string payload;

  if (!recv_message(client_fd, type, payload)) {
    platform_close_socket(client_fd);
    return;
  }

  if (type != MSG_REGISTER) {
    send_server_response(client_fd, 400, "Debe registrarse primero", false);
    platform_close_socket(client_fd);
    return;
  }

  chat::Register reg_msg;
  if (!reg_msg.ParseFromString(payload)) {
    send_server_response(client_fd, 400, "Mensaje malformado", false);
    platform_close_socket(client_fd);
    return;
  }

  std::string username = reg_msg.username();

  if (username.empty()) {
    send_server_response(client_fd, 400, "Nombre de usuario vacío", false);
    platform_close_socket(client_fd);
    return;
  }

  // Use client_ip from accept() (NOT Register.ip) for security
  if (!registry.register_user(username, client_ip, client_fd)) {
    send_server_response(client_fd, 409, "Username or IP already registered",
                         false);
    platform_close_socket(client_fd);
    return;
  }

  // Registration succeeded
  send_server_response(client_fd, 200, "Registration successful", true);
  std::cout << "[Server] User '" << username << "' connected from " << client_ip
            << std::endl;

  // =======================================================================
  // Phase 2: Message dispatch loop (poll-based)
  // =======================================================================
  bool running = true;

  while (running) {
    struct pollfd pfd{};
    pfd.fd = client_fd;
    pfd.events = POLLIN;

    int poll_ret = poll(&pfd, 1, POLL_TIMEOUT_MS);

    if (poll_ret == 0) {
      // Timeout — no data in 10 seconds. Inactivity is handled by the
      // separate checker thread; we just continue polling.
      continue;
    }

    if (poll_ret < 0) {
      if (errno == EINTR)
        continue; // signal interrupted — retry
      break;      // real error → disconnect
    }

    // Data available (POLLIN) — read the message
    MessageType msg_type;
    std::string msg_payload;
    if (!recv_message(client_fd, msg_type, msg_payload)) {
      break; // connection closed or error
    }

    // Update activity timestamp (may auto-restore from INVISIBLE)
    registry.update_activity(username);

    // Dispatch on message type
    switch (msg_type) {

    // ----- Type 2: MSG_GENERAL — Broadcast -----
    case MSG_GENERAL: {
      chat::MessageGeneral gen_msg;
      if (!gen_msg.ParseFromString(msg_payload)) {
        send_server_response(client_fd, 400, "Mensaje malformado", false);
        break;
      }

      // Build BroadcastDelivery
      chat::BroadcastDelivery bcast;
      bcast.set_message(gen_msg.message());
      bcast.set_username_origin(username); // from session, NOT from message

      std::string bcast_payload;
      bcast.SerializeToString(&bcast_payload);

      // Send to all users except sender
      registry.broadcast(bcast_payload, MSG_BROADCAST_DELIVERY, username);
      break;
    }

    // ----- Type 3: MSG_DM — Direct Message -----
    case MSG_DM: {
      chat::MessageDM dm_msg;
      if (!dm_msg.ParseFromString(msg_payload)) {
        send_server_response(client_fd, 400, "Mensaje malformado", false);
        break;
      }

      std::string target = dm_msg.username_des();

      // Build ForDm — username_des is the SENDER's name (not the target!)
      // because the client receiver displays "[DM de {username_des}]"
      chat::ForDm fwd;
      fwd.set_username_des(username); // SENDER's name
      fwd.set_message(dm_msg.message());

      std::string dm_payload;
      fwd.SerializeToString(&dm_payload);

      if (!registry.send_to_user(target, dm_payload, MSG_FOR_DM)) {
        send_server_response(client_fd, 404, "User not found or disconnected",
                             false);
      } else {
        // Confirm DM sent to sender
        send_server_response(client_fd, 200, "Mensaje enviado a " + target,
                             true);
      }
      break;
    }

    // ----- Type 4: MSG_CHANGE_STATUS — Status Change -----
    case MSG_CHANGE_STATUS: {
      chat::ChangeStatus cs_msg;
      if (!cs_msg.ParseFromString(msg_payload)) {
        send_server_response(client_fd, 400, "Mensaje malformado", false);
        break;
      }

      registry.change_status(username, cs_msg.status());
      send_server_response(client_fd, 200, "Status changed successfully", true);
      break;
    }

    // ----- Type 5: MSG_LIST_USERS — List Users -----
    case MSG_LIST_USERS: {
      // list_users() returns vector<pair<string, StatusEnum>>
      // already excludes INVISIBLE users
      auto users = registry.list_users();

      chat::AllUsers all;
      for (const auto &[uname, ustatus] : users) {
        all.add_usernames(uname);
        all.add_status(ustatus);
      }

      std::string list_payload;
      all.SerializeToString(&list_payload);
      send_message(client_fd, MSG_ALL_USERS, list_payload);
      break;
    }

    // ----- Type 6: MSG_GET_USER_INFO — Get User Info -----
    case MSG_GET_USER_INFO: {
      chat::GetUserInfo gui_msg;
      if (!gui_msg.ParseFromString(msg_payload)) {
        send_server_response(client_fd, 400, "Mensaje malformado", false);
        break;
      }

      std::string target = gui_msg.username_des();
      auto info_opt = registry.get_user_info(target);

      if (!info_opt.has_value()) {
        send_server_response(client_fd, 404, "User not found", false);
        break;
      }

      const auto &info = info_opt.value();

      chat::GetUserInfoResponse resp;
      resp.set_ip_address(info.ip);
      resp.set_username(info.username);
      resp.set_status(info.status);

      std::string info_payload;
      resp.SerializeToString(&info_payload);
      send_message(client_fd, MSG_GET_USER_INFO_RESP, info_payload);
      break;
    }

    // ----- Type 7: MSG_QUIT — Quit -----
    case MSG_QUIT: {
      running = false;
      break;
    }

    // ----- Unknown message type -----
    default: {
      send_server_response(client_fd, 400, "Unknown message type", false);
      break;
    }

    } // end switch
  } // end while

  // =======================================================================
  // Phase 3: Cleanup — MUST unregister BEFORE close
  // =======================================================================
  registry.unregister_user(username);        // FIRST: remove from registry
  platform_close_socket(client_fd);          // THEN: close socket
  std::cout << "[Server] User '" << username << "' disconnected" << std::endl;
}
