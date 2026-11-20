#include <gtest/gtest.h>
#include "../../../include/brazier/net/TcpClient.hpp"
#include "main.h"

class TcpClientTest : public ::testing::Test {
public:
	const int kTestPort = port_global;
	const std::string kTestHost = host_global;
};

TEST_F(TcpClientTest, ConnectToInvalidHost) {
	brazier::TcpClient client("invalid_host", 12345);

	EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(TcpClientTest, ConnectToInvalidPort) {
	brazier::TcpClient client(kTestHost, 0);

	EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(TcpClientTest, ConnectToValidHostAndPort) {
	brazier::TcpClient client(kTestHost, kTestPort);

	EXPECT_NO_THROW(client.connect());
}

TEST_F(TcpClientTest, ConnectToValidHostAndPortWithTimeout) {
	brazier::TcpClient client(kTestHost, kTestPort);

	EXPECT_NO_THROW(client.connect());
}

TEST_F(TcpClientTest, SendRawData) {
	brazier::TcpClient client(kTestHost, kTestPort);

	EXPECT_NO_THROW(client.connect());
	EXPECT_TRUE(client.send_raw("GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\n\r\n"));
}

TEST_F(TcpClientTest, ReadResponse) {
	brazier::TcpClient client(kTestHost, kTestPort);

	EXPECT_NO_THROW(client.connect());
	EXPECT_TRUE(client.send_raw("GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\n\r\n"));

	auto response = client.read_response();

	EXPECT_TRUE(response.has_value());
	EXPECT_EQ(response->result_int(), 404);
}

TEST_F(TcpClientTest, IsConnectionClosed) {
	brazier::TcpClient client(kTestHost, kTestPort);

	EXPECT_NO_THROW(client.connect());
	EXPECT_FALSE(client.is_connection_closed());
}

TEST_F(TcpClientTest, ReadResponseWithTimeout) {
	brazier::TcpClient client(kTestHost, kTestPort);

	EXPECT_NO_THROW(client.connect());
	EXPECT_TRUE(client.send_raw("GET / HTTP/1.1\r\nHost: " + kTestHost + "\r\n\r\n"));

	auto response = client.read_response(5);

	EXPECT_TRUE(response.has_value());
	EXPECT_EQ(response->result_int(), 404);
}