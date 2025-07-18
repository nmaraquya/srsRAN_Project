/*
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#pragma once

#include "plain_ip_adapter.h"
#include "srsran/sdap/sdap.h"
#include "srsran/srslog/srslog.h"

namespace srsran {
namespace srs_cu_up {

/// Adapter between Plain IP and SDAP for uplink traffic
class plain_ip_sdap_ul_adapter : public plain_ip_rx_data_notifier
{
public:
  plain_ip_sdap_ul_adapter();
  ~plain_ip_sdap_ul_adapter() override = default;

  void connect_sdap(sdap_tx_sdu_handler& sdap_handler);
  void disconnect_sdap();

  // plain_ip_rx_data_notifier interface
  void on_new_ip_packet(byte_buffer pkt) override;

private:
  sdap_tx_sdu_handler* sdap_handler_ = nullptr;
  srslog::basic_logger& logger_;
  qos_flow_id_t default_qfi_{uint_to_qos_flow_id(1)}; // Default QFI for plain IP
};

/// Adapter between SDAP and Plain IP for downlink traffic
class plain_ip_sdap_dl_adapter : public sdap_rx_sdu_notifier
{
public:
  plain_ip_sdap_dl_adapter();
  ~plain_ip_sdap_dl_adapter() override = default;

  void connect_plain_ip(plain_ip_adapter& adapter);
  void disconnect_plain_ip();

  // sdap_rx_sdu_notifier interface
  void on_new_sdu(byte_buffer sdu, qos_flow_id_t qfi) override;

private:
  plain_ip_adapter* plain_ip_adapter_ = nullptr;
  srslog::basic_logger& logger_;
};

} // namespace srs_cu_up
} // namespace srsran