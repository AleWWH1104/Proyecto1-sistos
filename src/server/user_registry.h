#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <mutex>
#include <ctime>

#include "common.pb.h"
#include "common/net_utils.h"

struct UserInfo
{
    std::string username;
    std::string ip;
    chat::StatusEnum status;
    int socket_fd;
    std::time_t last_activity;
    bool inactive_auto; // true if INVISIBLE was set by inactivity (not user choice)
};

class UserRegistry
{
public:
    // Register a new user. Returns false if username OR IP already exists.
    // Sets status=ACTIVE, last_activity=now, inactive_auto=false.
    bool register_user(const std::string &username, const std::string &ip, int socket_fd);

    // Remove user from registry. Returns true if found, false otherwise.
    bool unregister_user(const std::string &username);

    // Return list of {username, status} pairs, EXCLUDING users with status INVISIBLE.
    std::vector<std::pair<std::string, chat::StatusEnum>> list_users();

    // Return info for a specific user (including INVISIBLE users).
    // Returns std::nullopt if not found.
    std::optional<UserInfo> get_user_info(const std::string &username);

    // Update user's status. If user sets INVISIBLE manually, inactive_auto=false.
    // Returns true if user found, false otherwise.
    bool change_status(const std::string &username, chat::StatusEnum status);

    // Find username by socket fd. Returns empty string if not found. O(1) via reverse map.
    std::string get_username_by_fd(int fd);

    // Send a framed message to all connected users except exclude_username.
    // On send failure: log but don't remove (session thread handles cleanup).
    void broadcast(const std::string &serialized_msg, MessageType type,
                   const std::string &exclude_username = "");

    // Send a framed message to a specific user by username.
    // Returns false if user not found or send_message() fails.
    bool send_to_user(const std::string &username, const std::string &serialized_msg,
                      MessageType type);

    // Scan all users. If (now - last_activity > timeout) AND status != INVISIBLE,
    // set status to INVISIBLE and inactive_auto=true.
    void check_inactivity(int timeout_seconds);

    // Update last_activity to now(). If inactive_auto is true and user was
    // auto-INVISIBLE, restore status to ACTIVE and set inactive_auto=false.
    void update_activity(const std::string &username);

private:
    std::unordered_map<std::string, UserInfo> users_;
    std::unordered_map<int, std::string> fd_to_name_; // O(1) fd→username reverse map
    mutable std::mutex mtx_;
};
