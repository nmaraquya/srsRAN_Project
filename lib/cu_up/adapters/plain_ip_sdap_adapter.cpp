#include <gtest/gtest.h>
#include "plain_ip_adapter.h"
#include "srsran/support/executors/task_executor.h"
#include "srsran/srslog/srslog.h"
#include <memory>

using namespace srsran;
using namespace srs_cu_up;

// Mock executor with test validation capabilities
class mock_task_executor : public task_executor {
public:
    bool execute(unique_task task) override {
        executed_tasks++;
        task();
        return true;
    }

    bool defer(unique_task task) override {
        deferred_tasks++;
        task();
        return true;
    }

    void reset_counters() {
        executed_tasks = 0;
        deferred_tasks = 0;
    }

    unsigned get_executed_tasks() const { return executed_tasks; }
    unsigned get_deferred_tasks() const { return deferred_tasks; }

private:
    unsigned executed_tasks{0};
    unsigned deferred_tasks{0};
};

// Mock RX notifier for testing callbacks
class mock_rx_notifier : public plain_ip_rx_data_notifier {
public:
    void on_new_pdu(byte_buffer pdu) override {
        received_pdus.push_back(std::move(pdu));
    }

    size_t get_received_count() const { return received_pdus.size(); }
    const std::vector<byte_buffer>& get_received_pdus() const { return received_pdus; }
    void clear() { received_pdus.clear(); }

private:
    std::vector<byte_buffer> received_pdus;
};

class PlainIPAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        srslog::init();

        executor = std::make_shared<mock_task_executor>();
        rx_notifier = std::make_shared<mock_rx_notifier>();

        config.interface_name = "test_tun0";
        config.ip_address = "192.168.1.1";
        config.netmask = "255.255.255.0";
        config.enable_routing = false;
        config.enable_direct_forwarding = false;
        config.dn_interface = "test_dn0";

        adapter = std::make_unique<plain_ip_adapter>(config, *executor);
    }

    void TearDown() override {
        if (adapter) {
            adapter->stop();
        }
        adapter.reset();
        executor.reset();
        rx_notifier.reset();
        srslog::flush();
    }

    // Helper method to create a test IP packet
    static byte_buffer create_test_ip_packet(const std::string& src_ip = "192.168.1.2",
                                           const std::string& dst_ip = "192.168.1.3") {
        byte_buffer pkt;
        // IP header (20 bytes)
        pkt.append(0x45);  // Version 4, IHL 5
        pkt.append(0x00);  // DSCP/ECN
        pkt.append(0x00);  // Total Length (MSB)
        pkt.append(0x14);  // Total Length (LSB) = 20
        pkt.append(0x00);  // ID (MSB)
        pkt.append(0x00);  // ID (LSB)
        pkt.append(0x00);  // Flags/Fragment
        pkt.append(0x00);  // Fragment Offset
        pkt.append(0x40);  // TTL
        pkt.append(0x01);  // Protocol (ICMP)
        pkt.append(0x00);  // Checksum (MSB)
        pkt.append(0x00);  // Checksum (LSB)

        // Source IP
        std::vector<uint8_t> src = ip_to_bytes(src_ip);
        for (uint8_t b : src) {
            pkt.append(b);
        }

        // Destination IP
        std::vector<uint8_t> dst = ip_to_bytes(dst_ip);
        for (uint8_t b : dst) {
            pkt.append(b);
        }

        return pkt;
    }

    static std::vector<uint8_t> ip_to_bytes(const std::string& ip) {
        std::vector<uint8_t> bytes;
        size_t pos = 0, found;
        std::string token;
        std::string ip_copy = ip;

        while ((found = ip_copy.find('.', pos)) != std::string::npos) {
            token = ip_copy.substr(pos, found - pos);
            bytes.push_back(static_cast<uint8_t>(std::stoi(token)));
            pos = found + 1;
        }
        bytes.push_back(static_cast<uint8_t>(std::stoi(ip_copy.substr(pos))));
        return bytes;
    }

    std::shared_ptr<mock_task_executor> executor;
    std::shared_ptr<mock_rx_notifier> rx_notifier;
    plain_ip_config config;
    std::unique_ptr<plain_ip_adapter> adapter;
};

TEST_F(PlainIPAdapterTest, InitializationTest) {
    EXPECT_NE(adapter, nullptr);
    EXPECT_FALSE(adapter->init()); // Should fail without root privileges in test environment
}

TEST_F(PlainIPAdapterTest, DirectForwardingConfigurationTest) {
    config.enable_direct_forwarding = true;
    auto df_adapter = std::make_unique<plain_ip_adapter>(config, *executor);
    EXPECT_FALSE(df_adapter->init()); // Should fail without root privileges

    // Test UE route management
    qos_params qos;
    qos.priority = 1;
    qos.min_bitrate = 1000;
    qos.max_bitrate = 10000;

    EXPECT_FALSE(df_adapter->add_ue_route("192.168.1.2", qos));
    EXPECT_FALSE(df_adapter->remove_ue_route("192.168.1.2"));
    EXPECT_FALSE(df_adapter->update_ue_qos("192.168.1.2", qos));
}

TEST_F(PlainIPAdapterTest, PDUHandlingTest) {
    adapter->connect_rx_notifier(*rx_notifier);

    // Test sending PDU when adapter is not initialized
    byte_buffer test_pdu = create_test_ip_packet();
    EXPECT_FALSE(adapter->send_pdu(test_pdu.copy()));

    // Test receiving PDU when adapter is not initialized
    byte_buffer received = adapter->receive_pdu();
    EXPECT_TRUE(received.empty());

    // Test disconnecting notifier
    adapter->disconnect_rx_notifier();
    EXPECT_EQ(rx_notifier->get_received_count(), 0);
}

TEST_F(PlainIPAdapterTest, RxLoopTest) {
    adapter->connect_rx_notifier(*rx_notifier);

    // Start RX loop
    adapter->start_rx_loop();

    // Should process async tasks
    EXPECT_GT(executor->get_executed_tasks(), 0);

    // Stop RX loop
    adapter->stop_rx_loop();
    adapter->disconnect_rx_notifier();
}

TEST_F(PlainIPAdapterTest, QoSManagementTest) {
    config.enable_direct_forwarding = true;
    auto qos_adapter = std::make_unique<plain_ip_adapter>(config, *executor);

    qos_params qos;
    qos.priority = 1;
    qos.min_bitrate = 2000;
    qos.max_bitrate = 20000;

    // Test QoS operations without initialization
    EXPECT_FALSE(qos_adapter->add_ue_route("192.168.1.2", qos));
    EXPECT_FALSE(qos_adapter->update_ue_qos("192.168.1.2", qos));

    // Update QoS for non-existent route
    qos.max_bitrate = 30000;
    EXPECT_FALSE(qos_adapter->update_ue_qos("192.168.1.2", qos));
}

TEST_F(PlainIPAdapterTest, ConcurrencyTest) {
    const int NUM_OPERATIONS = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        threads.emplace_back([this, i]() {
            std::string ip = "192.168.1." + std::to_string(i % 254 + 1);
            qos_params qos;
            qos.priority = i % 8;
            qos.max_bitrate = 1000 * (i + 1);

            adapter->add_ue_route(ip, qos);
            adapter->update_ue_qos(ip, qos);
            adapter->remove_ue_route(ip);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(PlainIPAdapterTest, StressTest) {
    const int NUM_PACKETS = 1000;

    adapter->connect_rx_notifier(*rx_notifier);
    adapter->start_rx_loop();

    for (int i = 0; i < NUM_PACKETS; ++i) {
        byte_buffer pdu = create_test_ip_packet(
            "192.168.1." + std::to_string(i % 254 + 1),
            "192.168.1." + std::to_string((i + 1) % 254 + 1)
        );
        adapter->send_pdu(std::move(pdu));
    }

    adapter->stop_rx_loop();
    adapter->disconnect_rx_notifier();
}