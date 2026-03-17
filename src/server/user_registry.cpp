#include "user_registry.h"

bool UserRegistry::register_user(const std::string &username,
                                  const std::string &ip,
                                  int                sockfd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Reject duplicate username
    if (users_.count(username))
        return false;

    // Reject duplicate IP — comentar para pruebas locales,
    // descomentar para el dia de la entrega (maquinas distintas)
    // for (const auto &[name, info] : users_)
    //     if (info.ip == ip)
    //         return false;

    users_[username]    = {ip, sockfd, UserStatus::ACTIVE};
    fd_to_name_[sockfd] = username;
    return true;
}

void UserRegistry::remove_user(int sockfd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = fd_to_name_.find(sockfd);
    if (it == fd_to_name_.end())
        return;

    users_.erase(it->second);
    fd_to_name_.erase(it);
}

bool UserRegistry::set_status(const std::string &username, UserStatus status)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end())
        return false;

    it->second.status = status;
    return true;
}

void UserRegistry::set_inactive(const std::string &username)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it != users_.end())
        it->second.status = UserStatus::INVISIBLE; // maps to INVISIBLE (2) == INACTIVO
}

bool UserRegistry::get_user(const std::string &username, UserInfo &out) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end())
        return false;

    out = it->second;
    return true;
}

std::unordered_map<std::string, UserInfo> UserRegistry::all_users() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return users_; // snapshot copy
}

std::string UserRegistry::username_by_fd(int sockfd) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = fd_to_name_.find(sockfd);
    if (it == fd_to_name_.end())
        return "";
    return it->second;
}