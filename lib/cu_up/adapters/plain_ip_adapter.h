#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/sdap/sdap.h"
#include "srsran/support/async/async_task.h"
#include "srsran/support/executors/task_executor.h"
#include "srsran/srslog/srslog.h"

#include "plain_ip_sdap_adapter.h"

#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace srsran {
namespace srs_cu_up {

struct qos_params {
    uint32_t priority = 0;
    uint32_t min_bitrate = 0;
    uint32_t max_bitrate = 0;
};

struct plain_ip_config {
    std::string interface_name = "srs_tun0";
    std::string ip_address = "192.168.1.1";
    std::string netmask = "255.255.255.0";
    bool enable_routing = true;
    bool enable_direct_forwarding = false;
    std::string dn_interface = "srs_dn0";
    qos_params default_qos;
};

class plain_ip_adapter {
public:
    plain_ip_adapter(const plain_ip_config& config, task_executor& executor);
    ~plain_ip_adapter();

    bool init();
    void stop();
    bool send_pdu(byte_buffer pdu);
    byte_buffer receive_pdu();

    // New methods for direct forwarding
    bool add_ue_route(const std::string& ue_ip, const qos_params& qos = {});
    bool remove_ue_route(const std::string& ue_ip);
    bool update_ue_qos(const std::string& ue_ip, const qos_params& qos);
    bool enable_direct_forwarding(bool enable);

    // Existing methods...
    void connect_rx_notifier(plain_ip_rx_data_notifier& notifier);
    void disconnect_rx_notifier();
    void start_rx_loop();
    void stop_rx_loop();

private:
    bool setup_direct_forwarding();
    bool configure_ue_routing(const std::string& ue_ip);
    bool apply_qos_rules(const std::string& ue_ip, const qos_params& qos);

    struct ue_context {
        bool active = false;
        qos_params qos;
        std::string route_table;
    };

    plain_ip_config config_;
    int fd_{-1};
    int dn_fd_{-1};  // Direct forwarding interface
    std::atomic<bool> running_{false};
    std::atomic<bool> rx_loop_running_{false};
    task_executor& executor_;
    plain_ip_rx_data_notifier* rx_notifier_ = nullptr;

    std::mutex ue_mutex_;
    std::unordered_map<std::string, ue_context> ue_contexts_;

    srslog::basic_logger& logger_;
};

} // namespace srs_cu_up
} // namespace srsran
