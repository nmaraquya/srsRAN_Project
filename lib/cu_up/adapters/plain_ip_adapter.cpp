
#include "plain_ip_adapter.h"
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

plain_ip_adapter::plain_ip_adapter(const plain_ip_config& config, task_executor& executor) :
  config_(config), executor_(executor), logger_(srslog::fetch_basic_logger("PLAIN-IP"))
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
    rx_notifiers_[ue_ip] = &notifier;
}
void plain_ip_adapter::unregister_rx_notifier(const std::string& ue_ip) {
    rx_notifiers_.erase(ue_ip);
}
/*
void plain_ip_adapter::handle_rx_packets()
{
  while (rx_loop_running_ && running_) {
    byte_buffer pdu = receive_pdu();
    if (!pdu.empty() && rx_notifier_) {
      rx_notifier_->on_new_ip_packet(std::move(pdu));
    }

    // Small delay to prevent busy waiting
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}
*/
// Helper to extract destination IP as string from a byte_buffer
std::string plain_ip_adapter::extract_dest_ip(const byte_buffer& pkt) {
    if (pkt.length() < 20) {
        return "";
    }
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
    std::string dest_ip = extract_dest_ip(pkt); // implement this helper
    logger_.warning("handle_rx_packets.....", dest_ip);
    auto it = rx_notifiers_.find(dest_ip);
    if (it != rx_notifiers_.end()) {
        it->second->on_new_ip_packet(std::move(pkt));
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
  executor_.execute([this]() {
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