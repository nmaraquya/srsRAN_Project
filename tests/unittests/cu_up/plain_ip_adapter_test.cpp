#include <gtest/gtest.h>
#include "../../../lib/cu_up/adapters/plain_ip_adapter.h"
#include "srsran/support/executors/task_executor.h"
#include "srsran/srslog/srslog.h"

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
    executor = std::make_unique<mock_task_executor>();
    
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
  
  std::unique_ptr<mock_task_executor> executor;
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