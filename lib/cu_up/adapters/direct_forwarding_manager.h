#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/support/async/async_task.h"
#include "plain_ip_adapter.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace srsran {
namespace srs_cu_up {

struct direct_forwarding_config {
  std::string dn_interface;
  bool enable_qos = true;
  std::vector<std::string> allowed_ue_subnets;
};

class direct_forwarding_manager {
public:
  direct_forwarding_manager(const direct_forwarding_config& config);
  ~direct_forwarding_manager();

  bool init();
  void stop();

  bool add_ue_session(uint32_t session_id, const std::string& ue_ip);
  bool remove_ue_session(uint32_t session_id);
  bool update_ue_qos(uint32_t session_id, const qos_params& qos);

  bool forward_packet(byte_buffer pkt, uint32_t session_id);

private:
  bool validate_ue_subnet(const std::string& ue_ip);
  bool setup_forwarding_rules(const std::string& ue_ip);

  direct_forwarding_config config_;
  std::unique_ptr<plain_ip_adapter> ip_adapter_;
  std::unordered_map<uint32_t, std::string> session_ip_map_;
  srslog::basic_logger& logger_;
};

} // namespace srs_cu_up
} // namespace srsran