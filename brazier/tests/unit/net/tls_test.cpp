#include <gtest/gtest.h>
#include "../../../include/brazier/net/TlsClient.hpp"
#include "main.h"

std::string ca_cert() {
	return brazier::ConfigManager::getGlobal().get("https_server.tls.ca_file", std::string("certs/ca.crt"));
}

class TlsClientTest : public ::testing::Test {
public:
	const int kTestPort = https_port_global;
	const std::string kTestHost = https_host_global;
};

TEST_F(TlsClientTest, ConnectToInvalidHost) {
	brazier::TlsClient client("invalid_host", 12345);
	EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(TlsClientTest, ConnectToInvalidPort) {
	brazier::TlsClient client(kTestHost, 0);
	EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(TlsClientTest, ConnectToValidHostAndPort) {
	brazier::TlsClient client(kTestHost, kTestPort);
	EXPECT_NO_THROW(client.connect());
}

TEST_F(TlsClientTest, ConnectToValidHostAndPortWithTimeout) {
	brazier::TlsClient client(kTestHost, kTestPort);
	EXPECT_NO_THROW(client.connect());
}

TEST_F(TlsClientTest, ConnectWithCaVerifiesServer) {
	brazier::TlsClient client(kTestHost, kTestPort);
	client.set_ca(ca_cert());

	EXPECT_NO_THROW(client.connect());
}

TEST_F(TlsClientTest, ConnectWithInvalidCaFails) {
	brazier::TlsClient client(kTestHost, kTestPort);
	client.set_ca("app/certs/invalid_ca.crt");
	EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(TlsClientTest, SendRawData) {
	brazier::TlsClient client(kTestHost, kTestPort);
	client.set_ca(ca_cert());

	EXPECT_NO_THROW(client.connect());

	std::string request = "GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\nConnection: close\r\n\r\n";

	EXPECT_NO_THROW(client.send_raw(request));
}

TEST_F(TlsClientTest, ReadResponse) {
	brazier::TlsClient client(kTestHost, kTestPort);
	client.set_ca(ca_cert());

	EXPECT_NO_THROW(client.connect());

	std::string request = "GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\nConnection: close\r\n\r\n";

	EXPECT_NO_THROW(client.send_raw(request));

	auto response = client.read_response();

	EXPECT_TRUE(response.has_value());
	EXPECT_EQ(response->result_int(), 404);
}