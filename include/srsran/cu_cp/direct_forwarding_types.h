
/*
 *
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

 #include <cstdint>
 #include <string>
 
 namespace srsran {
 namespace srs_cu_up {
 
 /// Enum class defining the forwarding mode for PDU sessions
 enum class forwarding_mode {
   STANDARD_GTPU,    ///< Standard GTP-U forwarding
   DIRECT_FORWARDING ///< Direct forwarding mode
 };
 
 /// Structure containing information about a direct forwarding session
 struct direct_forwarding_session_info {
   uint32_t session_id;      ///< Unique session identifier
   std::string ue_ip;        ///< IP address of the UE
   bool active;              ///< Session activity status
   uint32_t qos_priority;    ///< QoS priority level
   uint64_t bytes_forwarded; ///< Number of bytes forwarded in this session
 };
 
 /// Structure containing statistics for direct forwarding
 struct direct_forwarding_stats {
   uint64_t packets_forwarded; ///< Total number of packets forwarded
   uint64_t bytes_forwarded;   ///< Total number of bytes forwarded
   uint32_t active_sessions;   ///< Number of currently active sessions
   uint32_t dropped_packets;   ///< Number of dropped packets
 };
 
 } // namespace srs_cu_up
 } // namespace srsran