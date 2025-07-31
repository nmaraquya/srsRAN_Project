#include "ue_ip_manager.h"
#include <arpa/inet.h>

namespace srsran {
namespace srs_cu_up {

ue_ip_manager::ue_ip_manager() :
    logger_(srslog::fetch_basic_logger("CU-UP-IP"))
{
}

bool ue_ip_manager::allocate_ip(uint32_t session_id, std::string& ip)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if session already has an IP
    if (session_ip_map_.find(session_id) != session_ip_map_.end()) {
        logger_.warning("Session {} already has IP {}", session_id, session_ip_map_[session_id]);
        return false;
    }

    // Generate new IP
    ip = generate_ip();
    if (ip.empty()) {
        logger_.error("Failed to generate IP for session {}", session_id);
        return false;
    }

    session_ip_map_[session_id] = ip;
    ip_pool_[ip] = true;
    
    logger_.info("Allocated IP {} for session {}", ip, session_id);
    return true;
}

bool ue_ip_manager::release_ip(uint32_t session_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = session_ip_map_.find(session_id);
    if (it == session_ip_map_.end()) {
        logger_.warning("No IP found for session {}", session_id);
        return false;
    }

    ip_pool_[it->second] = false;
    session_ip_map_.erase(it);
    
    logger_.info("Released IP for session {}", session_id);
    return true;
}

bool ue_ip_manager::get_session_ip(uint32_t session_id, std::string& ip)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = session_ip_map_.find(session_id);
    if (it == session_ip_map_.end()) {
        return false;
    }

    ip = it->second;
    return true;
}

bool ue_ip_manager::is_ip_allocated(const std::string& ip)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ip_pool_.find(ip);
    return (it != ip_pool_.end() && it->second);
}

std::string ue_ip_manager::generate_ip()
{
    // Simple IP generation from base IP
    for (uint32_t i = 2; i < 254; i++) {
        std::string ip = ip_base_ + std::to_string(i);
        if (ip_pool_.find(ip) == ip_pool_.end() || !ip_pool_[ip]) {
            return ip;
        }
    }
    return "";
}

bool ue_ip_manager::is_ip_in_pool(const std::string& ip)
{
    return ip.find(ip_base_) == 0;
}

} // namespace srs_cu_up
} // namespace srsran