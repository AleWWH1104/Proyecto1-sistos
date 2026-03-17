#include "server/user_registry.h"
#include "common/net_utils.h"

#include <iostream>
#include <algorithm>

// ---------------------------------------------------------------------------
// Task 2.2: Registration, removal, and query methods
// ---------------------------------------------------------------------------

bool UserRegistry::register_user(const std::string &username,
                                 const std::string &ip, int socket_fd)
{
    std::lock_guard<std::mutex> lock(mtx_);

    // Check duplicate username
    if (users_.count(username) > 0)
        return false;

    // Check duplicate IP
    for (const auto &[name, info] : users_)
    {
        if (info.ip == ip)
            return false;
    }

    // Insert new user
    UserInfo ui;
    ui.username = username;
    ui.ip = ip;
    ui.status = chat::ACTIVE;
    ui.socket_fd = socket_fd;
    ui.last_activity = std::time(nullptr);
    ui.inactive_auto = false;

    users_[username] = std::move(ui);
    return true;
}

bool UserRegistry::unregister_user(const std::string &username)
{
    std::lock_guard<std::mutex> lock(mtx_);
    return users_.erase(username) > 0;
}

std::vector<std::pair<std::string, chat::StatusEnum>> UserRegistry::list_users()
{
    std::lock_guard<std::mutex> lock(mtx_);

    std::vector<std::pair<std::string, chat::StatusEnum>> result;
    for (const auto &[name, info] : users_)
    {
        if (info.status == chat::INVISIBLE)
            continue;
        result.emplace_back(name, info.status);
    }
    return result;
}

std::optional<UserInfo> UserRegistry::get_user_info(const std::string &username)
{
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = users_.find(username);
    if (it == users_.end())
        return std::nullopt;
    return it->second;
}

bool UserRegistry::change_status(const std::string &username,
                                 chat::StatusEnum status)
{
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = users_.find(username);
    if (it == users_.end())
        return false;

    it->second.status = status;
    // Manual status change — clear inactive_auto regardless
    it->second.inactive_auto = false;
    return true;
}

std::string UserRegistry::get_username_by_fd(int fd)
{
    std::lock_guard<std::mutex> lock(mtx_);

    for (const auto &[name, info] : users_)
    {
        if (info.socket_fd == fd)
            return name;
    }
    return "";
}

// ---------------------------------------------------------------------------
// Task 2.3: Activity tracking, inactivity checker, send/broadcast
// ---------------------------------------------------------------------------

void UserRegistry::update_activity(const std::string &username)
{
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = users_.find(username);
    if (it == users_.end())
        return;

    it->second.last_activity = std::time(nullptr);

    // Auto-restore: if user was auto-INVISIBLE, restore to ACTIVE
    if (it->second.inactive_auto && it->second.status == chat::INVISIBLE)
    {
        it->second.status = chat::ACTIVE;
        it->second.inactive_auto = false;
        std::cout << "Auto-restaurado: " << username << " → ACTIVE" << std::endl;
    }
}

void UserRegistry::check_inactivity(int timeout_seconds)
{
    std::lock_guard<std::mutex> lock(mtx_);

    std::time_t now = std::time(nullptr);
    for (auto &[name, info] : users_)
    {
        if (info.status != chat::INVISIBLE)
        {
            double elapsed = std::difftime(now, info.last_activity);
            if (elapsed >= static_cast<double>(timeout_seconds))
            {
                std::cout << "Inactividad: " << name << " → INVISIBLE" << std::endl;
                info.status = chat::INVISIBLE;
                info.inactive_auto = true;
            }
        }
    }
}

void UserRegistry::broadcast(const std::string &serialized_msg, MessageType type,
                             const std::string &exclude_username)
{
    std::lock_guard<std::mutex> lock(mtx_);

    for (const auto &[name, info] : users_)
    {
        if (name == exclude_username)
            continue;

        if (!send_message(info.socket_fd, type, serialized_msg))
        {
            std::cerr << "broadcast: send failed for " << name << std::endl;
            // Don't remove — their session thread will detect the disconnect
        }
    }
}

bool UserRegistry::send_to_user(const std::string &username,
                                const std::string &serialized_msg,
                                MessageType type)
{
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = users_.find(username);
    if (it == users_.end())
        return false;

    bool ok = send_message(it->second.socket_fd, type, serialized_msg);
    if (!ok)
    {
        std::cerr << "send_to_user failed for " << username << std::endl;
        // Don't remove — their session thread will handle cleanup
    }
    return ok;
}
