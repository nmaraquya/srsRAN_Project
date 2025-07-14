// plain_ip_adapter.h
#pragma once

#include "srsran/adt/byte_buffer.h"
#include <atomic>

namespace srsran {
namespace srs_cu_up {

class plain_ip_adapter {
public:
  plain_ip_adapter();
  ~plain_ip_adapter();

  bool init();
  void stop();
  bool send_pdu(byte_buffer pdu);
  byte_buffer receive_pdu();

private:
  int fd{-1};
  std::atomic<bool> running{false};
};

} // namespace srs_cu_up
} // namespace srsran