#include <gtest/gtest.h>
#include "../../../lib/cu_up/adapters/plain_ip_adapter.h"
#include "srsran/support/executors/task_executor.h"
#include "srsran/srslog/srslog.h"
#include "cu_up_test_helpers.h"

using namespace srsran;
using namespace srs_cu_up;

// Simple mock executor for testing
class mock_task_executor : public task_executor {
public:
  bool execute(unique_task task) override {
    // Just execute the task immediately
    task();
    return true;
  }
  
  bool defer(unique_task task) override {
    // Just execute the task immediately
    task();
    return true;
  }
};

class PlainIPAdapterTest : public ::testing::Test {
protected:
  void SetUp() override {
    srslog::init();
    
    // Create a mock task executor
    ul_executor = std::make_unique<mock_task_executor>();
    dl_executor = std::make_unique<mock_task_executor>();
    
    // Create plain IP configuration
    config.interface_name = "test_tun0";
    config.ip_address = "192.168.1.1";
    config.netmask = "255.255.255.0";
    config.enable_routing = false; // Disable routing for tests
    
    // Create the adapter with proper arguments
    adapter = std::make_unique<plain_ip_adapter>(config, *ul_executor, *dl_executor);
  }
  
  void TearDown() override {
    adapter.reset();
    ul_executor.reset();
    dl_executor.reset();
    srslog::flush();
  }
  
  std::unique_ptr<mock_task_executor> ul_executor;
  std::unique_ptr<mock_task_executor> dl_executor;
  plain_ip_config config;
  std::unique_ptr<plain_ip_adapter> adapter;
};

TEST_F(PlainIPAdapterTest, ConstructorTest) {
  EXPECT_NE(adapter, nullptr);
}

TEST_F(PlainIPAdapterTest, InitTest) {
  // Note: This test may fail if run without proper privileges
  // In a real test environment, you might want to mock the system calls
  // or run with CAP_NET_ADMIN capability
  
  // For now, just test that init doesn't crash
  bool result = adapter->init();
  // We don't assert on the result because it depends on system privileges
  (void)result; // Suppress unused variable warning
  
  adapter->stop();
}

TEST_F(PlainIPAdapterTest, SendReceiveTest) {
  // Test that send/receive don't crash when adapter is not initialized
  byte_buffer test_buffer;
  bool success = test_buffer.append(0x45); // IP version 4, header length 5
  EXPECT_TRUE(success);
  
  for (int i = 0; i < 19; ++i) {
    success = test_buffer.append(0x00);
    EXPECT_TRUE(success);
  }
  
  bool result = adapter->send_pdu(test_buffer.copy());
  EXPECT_FALSE(result); // Should fail when not initialized
  
  byte_buffer received = adapter->receive_pdu();
  EXPECT_TRUE(received.empty()); // Should be empty when not initialized
}


// Mock DN receiver
class MockDNReceiver : public plain_ip_rx_data_notifier {
public:
    byte_buffer last_pkt;
    void on_new_ip_packet(byte_buffer pkt) override {
        last_pkt = std::move(pkt);
    }
};

TEST(PlainIPAdapterTest, UEtoDNPacketFlow) {
    srslog::init();
    plain_ip_config config;
    config.interface_name = "test_tun0";
    config.ip_address = "192.168.1.1";
    config.netmask = "255.255.255.0";
    config.enable_routing = false;
    
    mock_task_executor ul_executor;
    mock_task_executor dl_executor;
    plain_ip_adapter adapter(config, ul_executor, dl_executor);


    ASSERT_TRUE(adapter.init());

    MockDNReceiver dn_receiver;
    adapter.connect_rx_notifier(dn_receiver);

    // Simulate UE packet
    byte_buffer pkt;
  ASSERT_TRUE(pkt.append(0x45)); // IPv4 header
  for (int i = 0; i < 19; ++i) ASSERT_TRUE(pkt.append(0x00));

    ASSERT_TRUE(adapter.send_pdu(pkt.copy()));

byte_buffer rx_pkt = adapter.receive_pdu();
if (!rx_pkt.empty()) {
    dn_receiver.on_new_ip_packet(std::move(rx_pkt));
}

    // Check DN received packet
    EXPECT_FALSE(dn_receiver.last_pkt.empty());
    EXPECT_EQ(dn_receiver.last_pkt[0], 0x45);

    adapter.stop();
    srslog::flush();
}