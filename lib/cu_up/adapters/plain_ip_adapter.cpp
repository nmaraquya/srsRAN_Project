// plain_ip_adapter.cpp
#include "plain_ip_adapter.h"
#include "srsran/support/error_handling.h"
#include <fcntl.h>
#include <net/if.h>       // for IFNAMSIZ and struct ifreq
#include <linux/if_tun.h> // for IFF_TUN and IFF_NO_PI
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

using namespace srsran;
using namespace srs_cu_up;

plain_ip_adapter::plain_ip_adapter() = default;

plain_ip_adapter::~plain_ip_adapter()
{
  stop();
}

bool plain_ip_adapter::init()
{
  if (fd >= 0) {
    return false; // Already initialized
  }

  // Open TUN device
  fd = open("/dev/net/tun", O_RDWR);
  if (fd < 0) {
    return false;
  }

  // Configure TUN interface
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  strncpy(ifr.ifr_name, "srs_tun0", IFNAMSIZ - 1);

  if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
    close(fd);
    fd = -1;
    return false;
  }

  // Set non-blocking mode
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    close(fd);
    fd = -1;
    return false;
  }

  running = true;
  return true;
}

void plain_ip_adapter::stop()
{
  running = false;
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

bool plain_ip_adapter::send_pdu(byte_buffer pdu)
{
  if (!running || fd < 0 || pdu.empty()) {
    return false;
  }

  // Check minimum IP header size
  if (pdu.length() < 20) { // Minimum IPv4 header size
    return false;
  }

  // Copy data to a contiguous buffer for writing
  std::vector<uint8_t> buffer(pdu.length());
  std::copy(pdu.begin(), pdu.end(), buffer.begin());

  ssize_t n = write(fd, buffer.data(), buffer.size());
  return n == static_cast<ssize_t>(buffer.size());
}

byte_buffer plain_ip_adapter::receive_pdu()
{
  byte_buffer pdu;
  if (!running || fd < 0) {
    return pdu;
  }

  // Allocate buffer for maximum IP packet size
  std::vector<uint8_t> buffer(65536);
  ssize_t n = read(fd, buffer.data(), buffer.size());

  if (n > 0) {
    if (!pdu.resize(n)) {
      return byte_buffer{};
    }
    std::copy(buffer.begin(), buffer.begin() + n, pdu.begin());
  }

  return pdu;
}