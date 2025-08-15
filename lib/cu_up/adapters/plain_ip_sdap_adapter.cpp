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

#include "plain_ip_sdap_adapter.h"

using namespace srsran;
using namespace srs_cu_up;

plain_ip_sdap_ul_adapter::plain_ip_sdap_ul_adapter() :
  logger_(srslog::fetch_basic_logger("PLAIN-IP-UL"))
{
}

void plain_ip_sdap_ul_adapter::connect_sdap(sdap_tx_sdu_handler& sdap_handler)
{
  logger_.info(" !! Plain IP UL adapter connected to SDAP");
  sdap_handler_ = &sdap_handler;
}

void plain_ip_sdap_ul_adapter::disconnect_sdap()
{
  sdap_handler_ = nullptr;
  logger_.info("Plain IP UL adapter disconnected from SDAP");
}

void plain_ip_sdap_ul_adapter::on_new_ip_packet(byte_buffer pkt)
{
  if (sdap_handler_ == nullptr) {
    logger_.warning("Dropping UL IP packet: SDAP handler not connected");
    return;
  }

  if (pkt.empty()) {
    logger_.debug("Dropping empty UL IP packet");
    return;
  }

  // Forward IP packet to SDAP as SDU with default QFI
  logger_.debug("Forwarding UL IP packet of {} bytes to SDAP with QFI {}",
                pkt.length(), default_qfi_);
  sdap_handler_->handle_sdu(std::move(pkt), default_qfi_);
}

plain_ip_sdap_dl_adapter::plain_ip_sdap_dl_adapter() :
  logger_(srslog::fetch_basic_logger("PLAIN-IP-DL"))
{
}

void plain_ip_sdap_dl_adapter::connect_plain_ip(plain_ip_adapter& adapter)
{
  plain_ip_adapter_ = &adapter;
  logger_.info("Plain IP DL adapter connected to Plain IP adapter");
}

void plain_ip_sdap_dl_adapter::disconnect_plain_ip()
{
  plain_ip_adapter_ = nullptr;
  logger_.info("Plain IP DL adapter disconnected from Plain IP adapter");
}

void plain_ip_sdap_dl_adapter::on_new_sdu(byte_buffer sdu, qos_flow_id_t qfi)
{
  if (plain_ip_adapter_ == nullptr) {
    logger_.warning("Dropping DL SDU: Plain IP adapter not connected");
    return;
  }

  if (sdu.empty()) {
    logger_.debug("Dropping empty DL SDU");
    return;
  }

  // Forward SDU to Plain IP adapter (strip QFI information)
  logger_.debug("Forwarding DL SDU of {} bytes from QFI {} to Plain IP",
                sdu.length(), qfi);
  bool success = plain_ip_adapter_->send_pdu(std::move(sdu));

  if (!success) {
    logger_.warning("Failed to send DL packet to Plain IP adapter");
  }
}