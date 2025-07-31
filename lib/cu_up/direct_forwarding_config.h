#pragma once

#include <string>
#include <vector>

namespace srsran {
namespace srs_cu_up {

struct direct_forwarding_qos_config {
  uint32_t default_priority = 0;
  uint32_t min_guaranteed_bitrate = 0;
  uint32_t max_bitrate = 0;
};

struct direct_forwarding_security_config {
  bool enable_encryption = true;
  bool enable_integrity = true;
  std::string security_policy;
};

struct direct_forwarding_config {
  bool enabled = false;
  std::string dn_interface = "srs_dn0";
  std::vector<std::string> allowed_ue_subnets;
  direct_forwarding_qos_config qos;
  direct_forwarding_security_config security;
  bool enable_monitoring = true;
  uint32_t max_sessions_per_ue = 8;
};

} // namespace srs_cu_up
} // namespace srsran