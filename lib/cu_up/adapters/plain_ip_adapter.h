// plain_ip_adapter.h
#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/sdap/sdap.h"
#include "srsran/support/async/async_task.h"
#include "srsran/support/executors/task_executor.h"
#include "srsran/srslog/srslog.h"  // Add this include

#include <atomic>
#include <memory>

namespace srsran {
namespace srs_cu_up {
// Forward declaration to avoid circular dependency
class plain_ip_sdap_ul_adapter;

/// Configuration for plain IP adapter
struct plain_ip_config {
  std::string interface_name = "srs_tun0";
  std::string dev_name       = "eth0";
  std::string ip_address     = "192.168.100.1";
  std::string subnet         = "192.168.100.0";
  std::string netmask        = "255.255.255.0";
  std::string netmask_       = "24";
  bool        enable_routing = true;
};

/// Interface for plain IP data path notifications
class plain_ip_rx_data_notifier {
public:
  virtual ~plain_ip_rx_data_notifier() = default;
  virtual void on_new_ip_packet(byte_buffer pkt) = 0;
};

/// Plain IP adapter that handles TUN interface and IP packet processing
class plain_ip_adapter {
  std::unordered_map<std::string, plain_ip_rx_data_notifier*> rx_notifiers_;
public:
  plain_ip_adapter(const plain_ip_config& config, task_executor& ul_executor, task_executor& dl_executor);
  ~plain_ip_adapter();


    void set_ul_handler(plain_ip_sdap_ul_adapter& ul_handler) {
    ul_handler_ = &ul_handler;
    logger_.info("Plain IP adapter connected to UL handler");
  }
  
  void on_new_ip_packet(byte_buffer pkt);


  bool init();
  void stop();
  bool send_pdu(byte_buffer pdu);
  void send_pdu_async(byte_buffer pdu);
  byte_buffer receive_pdu();
  void log_all_notifiers() const;
  // Data path integration
  void connect_rx_notifier(plain_ip_rx_data_notifier& notifier);
  void disconnect_rx_notifier();

  // Async operations
  void start_rx_loop();
  void stop_rx_loop();

  void register_rx_notifier(const std::string& ue_ip, plain_ip_rx_data_notifier& notifier);
  void unregister_rx_notifier(const std::string& ue_ip);

private:
  void handle_rx_packets();
  bool configure_interface();
  bool setup_routing();
  plain_ip_sdap_ul_adapter* ul_handler_ = nullptr;

  plain_ip_config config_;
  int fd_{-1};
  std::atomic<bool> running_{false};
  std::atomic<bool> rx_loop_running_{false};
  task_executor& ul_executor_;
  task_executor& dl_executor_;
  plain_ip_rx_data_notifier* rx_notifier_ = nullptr;
  std::string extract_dest_ip(const byte_buffer& pkt);
  std::string extract_src_ip(const byte_buffer& pkt);
  srslog::basic_logger& logger_;
};

} // namespace srs_cu_up
} // namespace srsran