#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

// Possible status values (mirror of StatusEnum in protobuf)
enum class UserStatus
{
    ACTIVE         = 0,
    DO_NOT_DISTURB = 1,
    INVISIBLE      = 2,
};

struct UserInfo
{
    std::string ip;
    int         sockfd;
    UserStatus  status;
};

class UserRegistry
{
public:
    // Register a new user. Returns false if username or IP already exists.
    bool register_user(const std::string &username, const std::string &ip, int sockfd);

    // Remove a user by socket fd (called on disconnect).
    void remove_user(int sockfd);

    // Change the status of a user identified by username.
    // Returns false if user not found.
    bool set_status(const std::string &username, UserStatus status);

    // Auto-set INACTIVE for a user (called by inactivity timer).
    void set_inactive(const std::string &username);

    // Look up a single user. Returns false if not found.
    bool get_user(const std::string &username, UserInfo &out) const;

    // Returns a snapshot of all connected users: {username -> UserInfo}
    std::unordered_map<std::string, UserInfo> all_users() const;

    // Return the username associated with a socket fd, or "" if not found.
    std::string username_by_fd(int sockfd) const;

private:
    mutable std::mutex                          mutex_;
    std::unordered_map<std::string, UserInfo>   users_;      // username -> info
    std::unordered_map<int, std::string>        fd_to_name_; // sockfd  -> username
};