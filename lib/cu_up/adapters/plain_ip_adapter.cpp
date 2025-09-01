
#include "plain_ip_adapter.h"
#include "plain_ip_sdap_adapter.h"  // Include here, not in header
#include "srsran/support/error_handling.h"
#include <fcntl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>


using namespace srsran;
using namespace srs_cu_up;

plain_ip_adapter::plain_ip_adapter(const plain_ip_config& config, task_executor& ul_executor, task_executor& dl_executor) :
  config_(config), ul_executor_(ul_executor), dl_executor_(dl_executor), logger_(srslog::fetch_basic_logger("PLAIN-IP"))
{
}

plain_ip_adapter::~plain_ip_adapter()
{
  stop();
}

bool plain_ip_adapter::init()
{
  if (fd_ >= 0) {
    logger_.warning("Plain IP adapter already initialized");
    return false;
  }

  // Open TUN device
  fd_ = open("/dev/net/tun", O_RDWR);
  if (fd_ < 0) {
    logger_.error("Failed to open TUN device: {}", strerror(errno));
    return false;
  }

  // Configure TUN interface
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  strncpy(ifr.ifr_name, config_.interface_name.c_str(), IFNAMSIZ - 1);

  if (ioctl(fd_, TUNSETIFF, &ifr) < 0) {
    logger_.error("Failed to configure TUN interface: {}", strerror(errno));
    close(fd_);
    fd_ = -1;
    return false;
  }

  // Set non-blocking mode
  int flags = fcntl(fd_, F_GETFL);
  if (flags < 0 || fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
    logger_.error("Failed to set non-blocking mode: {}", strerror(errno));
    close(fd_);
    fd_ = -1;
    return false;
  }

  // Configure network interface
  if (!configure_interface()) {
    logger_.error("Failed to configure network interface");
    close(fd_);
    fd_ = -1;
    return false;
  }

  // Setup routing if enabled
  if (config_.enable_routing && !setup_routing()) {
    logger_.warning("Failed to setup routing, continuing without routing");
  }

  running_ = true;
  logger_.info("Plain IP adapter initialized successfully on interface {}", config_.interface_name);
  return true;
}

  // Method called when IP packets arrive from network interface
  void plain_ip_adapter::on_new_ip_packet(byte_buffer pkt) {
    if (ul_handler_ == nullptr) {
      logger_.warning("Dropping UL IP packet: UL handler not connected");
      return;
    }
    
    if (pkt.empty()) {
      logger_.debug("Dropping empty UL IP packet");
      return;
    }
    
    // Forward to UL adapter
    logger_.debug("Forwarding UL IP packet of {} bytes to UL adapter", pkt.length());
    ul_handler_->on_new_ip_packet(std::move(pkt));
  }
void plain_ip_adapter::stop()
{
  stop_rx_loop();

  running_ = false;
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }

  logger_.info("Plain IP adapter stopped");
}
void plain_ip_adapter::send_pdu_async(byte_buffer pdu)
{
  dl_executor_.execute([this, pdu = std::move(pdu)]() mutable {
    send_pdu(std::move(pdu));
  });
}

bool plain_ip_adapter::send_pdu(byte_buffer pdu)
{
  if (!running_ || fd_ < 0 || pdu.empty()) {
    return false;
  }

  // Check minimum IP header size
  if (pdu.length() < 20) {
    logger_.debug("Dropping PDU: too small for IP header (size={})", pdu.length());
    return false;
  }

  // Copy data to a contiguous buffer for writing
  std::vector<uint8_t> buffer(pdu.length());
  std::copy(pdu.begin(), pdu.end(), buffer.begin());

  ssize_t n = write(fd_, buffer.data(), buffer.size());
  if (n == static_cast<ssize_t>(buffer.size())) {
    logger_.debug("Sent IP packet of {} bytes", n);
    return true;
  } else {
    logger_.debug("Failed to send IP packet: written={}, expected={}", n, buffer.size());
    return false;
  }
}

byte_buffer plain_ip_adapter::receive_pdu()
{
  byte_buffer pdu;
  if (!running_ || fd_ < 0) {
    return pdu;
  }

  std::vector<uint8_t> buffer(65536);
  ssize_t n = read(fd_, buffer.data(), buffer.size());

  if (n > 0) {
    if (!pdu.resize(n)) {
      logger_.error("Failed to resize buffer for received packet");
      return byte_buffer{};
    }
    std::copy(buffer.begin(), buffer.begin() + n, pdu.begin());
    logger_.debug("Received IP packet of {} bytes", n);
  }

  return pdu;
}

void plain_ip_adapter::register_rx_notifier(const std::string& ue_ip, plain_ip_rx_data_notifier& notifier) {
  logger_.info("Registering RX notifier for UE IP: {}", ue_ip);

  std::string hex_dump;
  for (char c : ue_ip) {
      hex_dump += fmt::format("{:02x} ", static_cast<unsigned char>(c));
  }
  logger_.info("🔵 IP hex dump: {}", hex_dump);

  rx_notifiers_[ue_ip] = &notifier;

   logger_.info("🔵 Map size after registration: {}", rx_notifiers_.size());
   log_all_notifiers(); // Log immediately after registration

}
void plain_ip_adapter::unregister_rx_notifier(const std::string& ue_ip) {
    rx_notifiers_.erase(ue_ip);
}




std::string plain_ip_adapter::extract_dest_ip(const byte_buffer& pkt) {
      if (pkt.length() < 20) {
        logger_.warning("⚠️ Packet too short for IP header: {} bytes", pkt.length());
        return "";
    }

    // Try to access the data directly
    uint8_t ip_bytes[4];
    size_t copied = 0;
    
    // Copy bytes 16-19 (destination IP)
    for (auto it = pkt.begin(); it != pkt.end() && copied < 20; ++it, ++copied) {
        if (copied >= 16 && copied < 20) {
            ip_bytes[copied - 16] = *it;
        }
    }
    
    if (copied < 20) {
        logger_.warning("⚠️ Could not read full IP header");
        return "";
    }
    
    // Convert to string
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, ip_bytes, ip_str, INET_ADDRSTRLEN);
    
    std::string dest_ip(ip_str);
    logger_.info("🔍 EXTRACTED destination IP: '{}'", dest_ip);
    
    return dest_ip;
}

/*
std::string plain_ip_adapter::extract_dest_ip(const byte_buffer& pkt) {
    if (pkt.length() < 20) {
        logger_.warning("⚠️ Packet too short for IP header: {} bytes", pkt.length());
         return "";
    }

        // Get destination IP from IP header (bytes 16-19)
    auto slice = pkt.slice(16, 4);
    std::array<uint8_t, 4> ip_bytes_;
    std::copy(slice.begin(), slice.end(), ip_bytes_.begin());
    
    std::string dest_ip = fmt::format("{}.{}.{}.{}", 
                                     ip_bytes_[0], ip_bytes_[1], 
                                     ip_bytes_[2], ip_bytes_[3]);
    
    logger_.info("🔍 EXTRACTED destination IP: '{}' (length: {})", dest_ip, dest_ip.length());
    
    // Log hex dump
    std::string hex_dump;
    for (char c : dest_ip) {
        hex_dump += fmt::format("{:02x} ", static_cast<unsigned char>(c));
    }
    logger_.info("🔍 Extracted IP hex: {}", hex_dump);
    

    // IPv4: bytes 16-19 are destination IP
    uint8_t ip_bytes[4];
    auto it = pkt.begin();
    std::advance(it, 16);
    for (int i = 0; i < 4; ++i, ++it) {
        if (it == pkt.end()) return "";
        ip_bytes[i] = *it;
    }
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, ip_bytes, ip_str, INET_ADDRSTRLEN);
    return std::string(ip_str);
}
*/
void plain_ip_adapter::log_all_notifiers() const {

    logger_.info("📋 Current notifiers map (size: {}):", rx_notifiers_.size());
    if (rx_notifiers_.empty()) {
        logger_.warning("📋 No notifiers registered!");
        return;
    }
    
    for (const auto& [ip, notifier] : rx_notifiers_) {
        logger_.info("📋 IP: '{}' -> notifier: {}", ip, static_cast<const void*>(notifier));
        
        // Log hex dump for debugging
        std::string hex_dump;
        for (char c : ip) {
            hex_dump += fmt::format("{:02x} ", static_cast<unsigned char>(c));
        }
        logger_.info("📋 IP hex: {}", hex_dump);
    }

    std::string notifier_list;
    for (const auto& entry : rx_notifiers_) {
        notifier_list += entry.first + " ";
    }
    logger_.info("PIP !1 Registered plain IP notifiers: [{}]", notifier_list);
}

void plain_ip_adapter::handle_rx_packets() {
    
      logger_.warning("entering handle_rx_packets loop");
  while (rx_loop_running_ && running_) {
    byte_buffer pkt = receive_pdu();
    if (pkt.empty()) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }
    if (pkt.length() < 20) {
      logger_.warning("Received packet too short for IP header: {} bytes", pkt.length());
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }

    
    std::string dest_ip = extract_dest_ip(pkt);
    logger_.warning("handle_rx_packets.....", dest_ip);
    log_all_notifiers();
    auto it = rx_notifiers_.find(dest_ip);
    if (it != rx_notifiers_.end()) {
            ul_executor_.execute([notifier = it->second, pkt = std::move(pkt)]() mutable {
        notifier->on_new_ip_packet(std::move(pkt));
    });
    } else {
        logger_.warning("No notifier for IP {}", dest_ip);
    }
    // Small delay to prevent busy waiting
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}

void plain_ip_adapter::connect_rx_notifier(plain_ip_rx_data_notifier& notifier)
{
  rx_notifier_ = &notifier;
}

void plain_ip_adapter::disconnect_rx_notifier()
{
  rx_notifier_ = nullptr;
}

void plain_ip_adapter::start_rx_loop()
{
  if (rx_loop_running_) {
    return;
  }

  rx_loop_running_ = true;

  // Start async RX loop
  dl_executor_.execute([this]() {
    handle_rx_packets();
  });
}

void plain_ip_adapter::stop_rx_loop()
{
  rx_loop_running_ = false;
}


bool plain_ip_adapter::configure_interface()
{
  // Create socket for interface configuration
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    logger_.error("Failed to create socket for interface configuration: {}", strerror(errno));
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, config_.interface_name.c_str(), IFNAMSIZ - 1);

  // Set IP address
  struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
  addr->sin_family = AF_INET;
  inet_pton(AF_INET, config_.ip_address.c_str(), &addr->sin_addr);

  if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
    logger_.error("Failed to set IP address: {}", strerror(errno));
    close(sock);
    return false;
  }

  // Set netmask
  inet_pton(AF_INET, config_.netmask.c_str(), &addr->sin_addr);
  if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
    logger_.error("Failed to set netmask: {}", strerror(errno));
    close(sock);
    return false;
  }

  // Bring interface up
  if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
    logger_.error("Failed to get interface flags: {}", strerror(errno));
    close(sock);
    return false;
  }

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
    logger_.error("Failed to bring interface up: {}", strerror(errno));
    close(sock);
    return false;
  }

  close(sock);
  logger_.info("Interface {} configured with IP {} netmask {}",
               config_.interface_name, config_.ip_address, config_.netmask);
  return true;
}

bool plain_ip_adapter::setup_routing()
{
  // This is a simplified routing setup
  // In a real implementation, you might want to add specific routes
  logger_.info("Routing setup started for interface {}", config_.interface_name);
  std::string cmd = "ip route add 192.168.1.0/24 dev " + config_.interface_name;
  int result = system(cmd.c_str());

  if (result == 0) {
    logger_.info("Routing setup completed for interface {}", config_.interface_name);
    return true;
  } else {
    logger_.warning("Failed to setup routing for interface {}", config_.interface_name);
    return false;
  }
}