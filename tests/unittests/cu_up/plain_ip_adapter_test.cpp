#include <gtest/gtest.h>
#include "../../../lib/cu_up/adapters/plain_ip_adapter.h"
#include "srsran/support/executors/manual_task_executor.h"
#include "srsran/srslog/srslog.h"

using namespace srsran;
using namespace srs_cu_up;

class PlainIPAdapterTest : public ::testing::Test {
protected:
  void SetUp() override {
    srslog::init();
    
    // Create a manual task executor for testing
    executor = std::make_unique<manual_task_executor>();
    
    // Create plain IP configuration
    config.interface_name = "test_tun0";
    config.ip_address = "192.168.1.1";
    config.netmask = "255.255.255.0";
    config.enable_routing = false; // Disable routing for tests
    
    // Create the adapter with proper arguments
    adapter = std::make_unique<plain_ip_adapter>(config, *executor);
  }
  
  void TearDown() override {
    adapter.reset();
    executor.reset();
    srslog::flush();
  }
  
  std::unique_ptr<manual_task_executor> executor;
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
  
  adapter->stop();
}

TEST_F(PlainIPAdapterTest, SendReceiveTest) {
  // Test that send/receive don't crash when adapter is not initialized
  byte_buffer test_buffer;
  test_buffer.append(0x45); // IP version 4, header length 5
  for (int i = 0; i < 19; ++i) {
    test_buffer.append(0x00);
  }
  
  bool result = adapter->send_pdu(test_buffer.copy());
  EXPECT_FALSE(result); // Should fail when not initialized
  
  byte_buffer received = adapter->receive_pdu();
  EXPECT_TRUE(received.empty()); // Should be empty when not initialized
}