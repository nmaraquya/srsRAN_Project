#include "direct_forwarding_manager.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

namespace srsran {
namespace srs_cu_up {

direct_forwarding_manager::direct_forwarding_manager(const direct_forwarding_config& config) :
    config_(config),
    logger_(srslog::fetch_basic_logger("CU-UP-DF"))
{
}

direct_forwarding_manager::~direct_forwarding_manager()
{
    stop();
}

bool direct_forwarding_manager::init()
{
    // Initialize plain IP adapter with direct forwarding configuration
    plain_ip_config ip_config;
    ip_config.interface_name = config_.dn_interface;
    ip_config.enable_direct_forwarding = true;

    ip_adapter_ = std::make_unique<plain_ip_adapter>(ip_config, executor_);
    if (!ip_adapter_->init()) {
        logger_.error("Failed to initialize plain IP adapter");
        return false;
    }

    logger_.info("Direct forwarding manager initialized");
    return true;
}

void direct_forwarding_manager::stop()
{
    if (ip_adapter_) {
        ip_adapter_->stop();
    }

    // Clean up all sessions
    for (const auto& session : session_ip_map_) {
        remove_ue_session(session.first);
    }
    session_ip_map_.clear();
}

bool direct_forwarding_manager::add_ue_session(uint32_t session_id, const std::string& ue_ip)
{
    if (!validate_ue_subnet(ue_ip)) {
        logger_.error("UE IP {} not in allowed subnets", ue_ip);
        return false;
    }

    if (session_ip_map_.find(session_id) != session_ip_map_.end()) {
        logger_.warning("Session {} already exists", session_id);
        return false;
    }

    // Setup forwarding rules
    if (!setup_forwarding_rules(ue_ip)) {
        logger_.error("Failed to setup forwarding rules for UE IP {}", ue_ip);
        return false;
    }

    // Add route in plain IP adapter
    qos_params default_qos; // Use default QoS initially
    if (!ip_adapter_->add_ue_route(ue_ip, default_qos)) {
        logger_.error("Failed to add UE route for IP {}", ue_ip);
        return false;
    }

    session_ip_map_[session_id] = ue_ip;
    logger_.info("Added UE session {} with IP {}", session_id, ue_ip);
    return true;
}

bool direct_forwarding_manager::remove_ue_session(uint32_t session_id)
{
    auto it = session_ip_map_.find(session_id);
    if (it == session_ip_map_.end()) {
        logger_.warning("Session {} not found", session_id);
        return false;
    }

    // Remove route from plain IP adapter
    if (!ip_adapter_->remove_ue_route(it->second)) {
        logger_.error("Failed to remove UE route for IP {}", it->second);
    }

    // Remove forwarding rules
    std::string cmd = "iptables -D FORWARD -s " + it->second + " -j ACCEPT";
    system(cmd.c_str());
    cmd = "iptables -D FORWARD -d " + it->second + " -j ACCEPT";
    system(cmd.c_str());

    session_ip_map_.erase(it);
    logger_.info("Removed UE session {}", session_id);
    return true;
}

bool direct_forwarding_manager::update_ue_qos(uint32_t session_id, const qos_params& qos)
{
    auto it = session_ip_map_.find(session_id);
    if (it == session_ip_map_.end()) {
        logger_.warning("Session {} not found for QoS update", session_id);
        return false;
    }

    if (!ip_adapter_->update_ue_qos(it->second, qos)) {
        logger_.error("Failed to update QoS for UE IP {}", it->second);
        return false;
    }

    logger_.info("Updated QoS for session {} (UE IP {})", session_id, it->second);
    return true;
}

bool direct_forwarding_manager::forward_packet(byte_buffer pkt, uint32_t session_id)
{
    auto it = session_ip_map_.find(session_id);
    if (it == session_ip_map_.end()) {
        logger_.warning("Session {} not found for packet forwarding", session_id);
        return false;
    }

    // Forward packet through plain IP adapter
    if (!ip_adapter_->send_pdu(std::move(pkt))) {
        logger_.error("Failed to forward packet for session {}", session_id);
        return false;
    }

    return true;
}

bool direct_forwarding_manager::validate_ue_subnet(const std::string& ue_ip)
{
    struct in_addr addr;
    if (inet_pton(AF_INET, ue_ip.c_str(), &addr) != 1) {
        return false;
    }

    for (const auto& subnet : config_.allowed_ue_subnets) {
        // Simple string prefix matching (in production, use proper CIDR checking)
        if (ue_ip.find(subnet) == 0) {
            return true;
        }
    }

    return false;
}

bool direct_forwarding_manager::setup_forwarding_rules(const std::string& ue_ip)
{
    // Enable IP forwarding
    std::string cmd = "echo 1 > /proc/sys/net/ipv4/ip_forward";
    if (system(cmd.c_str()) != 0) {
        logger_.error("Failed to enable IP forwarding");
        return false;
    }

    // Add forwarding rules
    cmd = "iptables -A FORWARD -s " + ue_ip + " -j ACCEPT";
    if (system(cmd.c_str()) != 0) {
        logger_.error("Failed to add source forwarding rule for {}", ue_ip);
        return false;
    }

    cmd = "iptables -A FORWARD -d " + ue_ip + " -j ACCEPT";
    if (system(cmd.c_str()) != 0) {
        logger_.error("Failed to add destination forwarding rule for {}", ue_ip);
        return false;
    }

    return true;
}

} // namespace srs_cu_up
} // namespace srsran