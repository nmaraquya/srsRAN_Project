#pragma once

#include "srsran/adt/byte_buffer.h"
#include "srsran/sdap/sdap.h"
#include "srsran/support/async/async_task.h"
#include "direct_forwarding_manager.h"

namespace srsran {
namespace srs_cu_up {

class plain_ip_sdap_adapter : public sdap_interface
{
public:
  plain_ip_sdap_adapter(task_executor& executor);
  ~plain_ip_sdap_adapter() = default;

  void set_direct_forwarding(bool enable);
  void set_forwarding_manager(direct_forwarding_manager* manager);

  // SDAP interface implementation
  void handle_pdu(byte_buffer pdu) override;
  void notify_qos_flow_mapping(uint32_t pdu_session_id, uint32_t qos_flow_id) override;

private:
  bool handle_direct_forwarding(byte_buffer& pdu);
  bool handle_standard_path(byte_buffer& pdu);

  task_executor& executor_;
  bool direct_forwarding_enabled_{false};
  direct_forwarding_manager* forwarding_manager_{nullptr};
  srslog::basic_logger& logger_;
};

} // namespace srs_cu_up
} // namespace srsran