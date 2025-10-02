
#pragma once
#include "config.hpp"
#include <chrono>
#include <unordered_map>
#include <mutex>

enum class Role {
    UNKNOWN,
    GUEST,
    OWNER
};

struct UserSession {
    int64_t tg_user_id;
    Role role;
    std::chrono::system_clock::time_point expires_at;
    
    bool is_expired() const;
};

class AuthManager {
public:
    explicit AuthManager(const Config& config);
    
    Role get_user_role(int64_t tg_user_id) const;
    bool is_command_allowed(const std::string& cmd, Role role) const;
    void set_guest_session(int64_t tg_user_id, int minutes);
    void cleanup_expired_sessions();
    
private:
    const Config& config_;
    std::unordered_map<int64_t, UserSession> guest_sessions_;
    mutable std::mutex sessions_mutex_;
};
