
#include "auth.hpp"
#include <spdlog/spdlog.h>

bool UserSession::is_expired() const {
    return std::chrono::system_clock::now() > expires_at;
}

AuthManager::AuthManager(const Config& config) : config_(config) {}

Role AuthManager::get_user_role(int64_t tg_user_id) const {
    // Check if it's the owner
    if (tg_user_id == config_.owner_telegram_id) {
        return Role::OWNER;
    }
    
    // Check guest sessions
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = guest_sessions_.find(tg_user_id);
    if (it != guest_sessions_.end() && !it->second.is_expired()) {
        return Role::GUEST;
    }
    
    return Role::UNKNOWN;
}

bool AuthManager::is_command_allowed(const std::string& cmd, Role role) const {
    // Owner can use all commands
    if (role == Role::OWNER) {
        return true;
    }
    
    // Guest commands
    if (role == Role::GUEST) {
        return cmd == "start" || cmd == "help" || cmd == "balance" || 
               cmd == "holdings" || cmd == "signals" || cmd == "health";
    }
    
    return false;
}

void AuthManager::set_guest_session(int64_t tg_user_id, int minutes) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    UserSession session;
    session.tg_user_id = tg_user_id;
    session.role = Role::GUEST;
    session.expires_at = std::chrono::system_clock::now() + 
                        std::chrono::minutes(minutes);
    
    guest_sessions_[tg_user_id] = session;
    spdlog::info("Guest session created for user {} (expires in {} minutes)", 
                 tg_user_id, minutes);
}

void AuthManager::cleanup_expired_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = guest_sessions_.begin();
    while (it != guest_sessions_.end()) {
        if (it->second.is_expired()) {
            spdlog::debug("Removing expired session for user {}", it->first);
            it = guest_sessions_.erase(it);
        } else {
            ++it;
        }
    }
}
