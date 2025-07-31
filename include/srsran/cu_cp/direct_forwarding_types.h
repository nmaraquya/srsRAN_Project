#pragma once

#include <cstdint>
#include <string>

namespace srsran {
namespace srs_cu_up {

enum class forwarding_mode {
  STANDARD_GTPU,
  DIRECT_FORWARDING
};

struct direct_forwarding_session_info {
  uint32_t session_id;
  std::string ue_ip;
  bool active;
  uint32_t qos_priority;
  uint64_t bytes_forwarded;
};

struct direct_forwarding_stats {
  uint64_t packets_forwarded;
  uint64_t bytes_forwarded;
  uint32_t active_sessions;
  uint32_t dropped_packets;
};

} // namespace srs_cu_up
} // namespace srsran