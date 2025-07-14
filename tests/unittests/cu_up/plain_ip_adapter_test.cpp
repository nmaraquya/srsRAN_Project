// plain_ip_adapter_test.cpp
#include "lib/cu_up/adapters/plain_ip_adapter.h"
#include <gtest/gtest.h>
#include <unistd.h>

using namespace srsran;
using namespace srs_cu_up;

class plain_ip_adapter_test : public ::testing::Test {
protected:
  void SetUp() override 
  {
    // Skip all tests if not root
    if (getuid() != 0) {
      GTEST_SKIP() << "This test requires root privileges to create TUN device";
    }
    adapter = std::make_unique<plain_ip_adapter>();
  }

  void TearDown() override 
  {
    if (adapter) {
      adapter->stop();
    }
  }

  std::unique_ptr<plain_ip_adapter> adapter;
};

TEST_F(plain_ip_adapter_test, init_success)
{
  ASSERT_TRUE(adapter->init());
}

TEST_F(plain_ip_adapter_test, init_stop_sequence)
{
  ASSERT_TRUE(adapter->init());
  adapter->stop();
  ASSERT_TRUE(adapter->init());
}

TEST_F(plain_ip_adapter_test, send_empty_pdu)
{
  ASSERT_TRUE(adapter->init());
  byte_buffer empty_pdu;
  ASSERT_FALSE(adapter->send_pdu(std::move(empty_pdu)));
}

TEST_F(plain_ip_adapter_test, send_invalid_pdu)
{
  ASSERT_TRUE(adapter->init());
  byte_buffer invalid_pdu;
  ASSERT_TRUE(invalid_pdu.resize(1)); // Too small for IP header
  ASSERT_FALSE(adapter->send_pdu(std::move(invalid_pdu)));
}

TEST_F(plain_ip_adapter_test, receive_pdu_before_init)
{
  byte_buffer pdu = adapter->receive_pdu();
  ASSERT_TRUE(pdu.empty());
}

TEST_F(plain_ip_adapter_test, receive_pdu_after_stop)
{
  ASSERT_TRUE(adapter->init());
  adapter->stop();
  byte_buffer pdu = adapter->receive_pdu();
  ASSERT_TRUE(pdu.empty());
}