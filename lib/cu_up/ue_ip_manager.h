#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include "srsran/srslog/srslog.h"

namespace srsran {
namespace srs_cu_up {

class ue_ip_manager {
public:
  ue_ip_manager();
  ~ue_ip_manager() = default;

  bool allocate_ip(uint32_t session_id, std::string& ip);
  bool release_ip(uint32_t session_id);
  bool get_session_ip(uint32_t session_id, std::string& ip);
  bool is_ip_allocated(const std::string& ip);

private:
  bool is_ip_in_pool(const std::string& ip);
  std::string generate_ip();

  std::mutex mutex_;
  std::unordered_map<uint32_t, std::string> session_ip_map_;
  std::unordered_map<std::string, bool> ip_pool_;
  uint32_t next_ip_index_{0};
  const std::string ip_base_{"192.168.2."};
  srslog::basic_logger& logger_;
};

} // namespace srs_cu_up
} // namespace srsran