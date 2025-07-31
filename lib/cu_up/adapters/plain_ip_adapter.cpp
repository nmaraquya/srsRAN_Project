#include "plain_ip_adapter.h"
#include <net/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>

namespace srsran {
namespace srs_cu_up {

plain_ip_adapter::plain_ip_adapter(const plain_ip_config& config, task_executor& executor) :
    config_(config),
    executor_(executor),
    logger_(srslog::fetch_basic_logger("CU-UP"))
{
}

plain_ip_adapter::~plain_ip_adapter()
{
    stop();
}

bool plain_ip_adapter::init()
{
    // Create TUN interface
    struct ifreq ifr;
    int flags = IFF_TUN | IFF_NO_PI;

    if ((fd_ = open("/dev/net/tun", O_RDWR)) < 0) {
        logger_.error("Failed to open /dev/net/tun");
        return false;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;
    strncpy(ifr.ifr_name, config_.interface_name.c_str(), IFNAMSIZ);

    if (ioctl(fd_, TUNSETIFF, &ifr) < 0) {
        logger_.error("Failed to create TUN interface");
        close(fd_);
        return false;
    }

    if (config_.enable_direct_forwarding) {
        if (!setup_direct_forwarding()) {
            logger_.error("Failed to setup direct forwarding");
            close(fd_);
            return false;
        }
    }

    if (!configure_interface()) {
        logger_.error("Failed to configure interface");
        close(fd_);
        return false;
    }

    running_ = true;
    return true;
}

bool plain_ip_adapter::setup_direct_forwarding()
{
    struct ifreq ifr;
    int flags = IFF_TUN | IFF_NO_PI;

    if ((dn_fd_ = open("/dev/net/tun", O_RDWR)) < 0) {
        logger_.error("Failed to open DN interface");
        return false;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;
    strncpy(ifr.ifr_name, config_.dn_interface.c_str(), IFNAMSIZ);

    if (ioctl(dn_fd_, TUNSETIFF, &ifr) < 0) {
        logger_.error("Failed to create DN interface");
        close(dn_fd_);
        return false;
    }

    return true;
}

bool plain_ip_adapter::add_ue_route(const std::string& ue_ip, const qos_params& qos)
{
    std::lock_guard<std::mutex> lock(ue_mutex_);
    
    if (ue_contexts_.find(ue_ip) != ue_contexts_.end()) {
        logger_.warning("UE route already exists for IP: {}", ue_ip);
        return false;
    }

    ue_context context;
    context.active = true;
    context.qos = qos;

    if (!configure_ue_routing(ue_ip)) {
        logger_.error("Failed to configure routing for UE IP: {}", ue_ip);
        return false;
    }

    if (!apply_qos_rules(ue_ip, qos)) {
        logger_.error("Failed to apply QoS rules for UE IP: {}", ue_ip);
        return false;
    }

    ue_contexts_[ue_ip] = context;
    logger_.info("Added UE route for IP: {}", ue_ip);
    return true;
}

bool plain_ip_adapter::remove_ue_route(const std::string& ue_ip)
{
    std::lock_guard<std::mutex> lock(ue_mutex_);
    
    auto it = ue_contexts_.find(ue_ip);
    if (it == ue_contexts_.end()) {
        logger_.warning("UE route not found for IP: {}", ue_ip);
        return false;
    }

    // Remove routing rules
    std::string cmd = "ip route del " + ue_ip;
    if (system(cmd.c_str()) != 0) {
        logger_.error("Failed to remove route for UE IP: {}", ue_ip);
        return false;
    }

    // Remove QoS rules if any
    if (!apply_qos_rules(ue_ip, {})) {
        logger_.warning("Failed to remove QoS rules for UE IP: {}", ue_ip);
    }

    ue_contexts_.erase(it);
    logger_.info("Removed UE route for IP: {}", ue_ip);
    return true;
}

bool plain_ip_adapter::update_ue_qos(const std::string& ue_ip, const qos_params& qos)
{
    std::lock_guard<std::mutex> lock(ue_mutex_);

    auto it = ue_contexts_.find(ue_ip);
    if (it == ue_contexts_.end()) {
        logger_.warning("UE not found for QoS update: {}", ue_ip);
        return false;
    }

    if (!apply_qos_rules(ue_ip, qos)) {
        logger_.error("Failed to update QoS rules for UE IP: {}", ue_ip);
        return false;
    }

    it->second.qos = qos;
    logger_.info("Updated QoS for UE IP: {}", ue_ip);
    return true;
}

bool plain_ip_adapter::configure_ue_routing(const std::string& ue_ip)
{
    // Add routing rule for UE
    std::string cmd = "ip route add " + ue_ip + " dev " + config_.interface_name;
    if (system(cmd.c_str()) != 0) {
        logger_.error("Failed to add route for UE IP: {}", ue_ip);
        return false;
    }
    return true;
}

bool plain_ip_adapter::apply_qos_rules(const std::string& ue_ip, const qos_params& qos)
{
    // Apply tc rules for QoS
    // Note: This is a simplified implementation
    std::string cmd = "tc qdisc add dev " + config_.interface_name + " root handle 1: htb default 10";
    system(cmd.c_str());

    if (qos.max_bitrate > 0) {
        cmd = "tc class add dev " + config_.interface_name + " parent 1: classid 1:1 htb rate " +
              std::to_string(qos.max_bitrate) + "kbit";
        if (system(cmd.c_str()) != 0) {
            logger_.error("Failed to apply QoS rate limiting for UE IP: {}", ue_ip);
            return false;
        }
    }

    return true;
}

// ... (continue with existing method implementations)

} // namespace srs_cu_up
} // namespace srsran