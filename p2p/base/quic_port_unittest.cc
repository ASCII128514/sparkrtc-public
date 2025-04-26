/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/quic_port.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/stun.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

using cricket::QUIC_PROTOCOL_NAME;
using cricket::QuicPort;
using rtc::SocketAddress;

namespace cricket {
namespace {

class QuicPortTest : public ::testing::Test {
 protected:
  QuicPortTest()
      : ss_(new rtc::VirtualSocketServer()),
        main_(ss_.get()),
        socket_factory_(rtc::Thread::Current()),
        network_("unittest", "unittest", rtc::IPAddress(INADDR_ANY), 32),
        username_(rtc::CreateRandomString(ICE_UFRAG_LENGTH)),
        password_(rtc::CreateRandomString(ICE_PWD_LENGTH)) {}

  void SetUp() override {
    rtc::Thread::Current()->set_socketserver(ss_.get());
  }

  void TearDown() override {
    rtc::Thread::Current()->set_socketserver(nullptr);
  }

  rtc::Network* network() { return &network_; }
  const std::string& username() const { return username_; }
  const std::string& password() const { return password_; }

  std::unique_ptr<QuicPort> CreateQuicPort(bool allow_listen) {
    return QuicPort::Create(rtc::Thread::Current(), &socket_factory_, network(),
                           0, 0, username(), password(), allow_listen,
                           &field_trials_);
  }

 private:
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::AutoSocketServerThread main_;
  rtc::BasicPacketSocketFactory socket_factory_;
  rtc::Network network_;
  std::string username_;
  std::string password_;
  webrtc::test::ScopedKeyValueConfig field_trials_;
};

TEST_F(QuicPortTest, TestSupportsProtocol) {
  std::unique_ptr<QuicPort> port = CreateQuicPort(/*allow_listen=*/true);
  EXPECT_TRUE(port->SupportsProtocol(QUIC_PROTOCOL_NAME));
  EXPECT_FALSE(port->SupportsProtocol(UDP_PROTOCOL_NAME));
  EXPECT_FALSE(port->SupportsProtocol(TCP_PROTOCOL_NAME));
  EXPECT_FALSE(port->SupportsProtocol(SSLTCP_PROTOCOL_NAME));
}

TEST_F(QuicPortTest, TestGetProtocol) {
  std::unique_ptr<QuicPort> port = CreateQuicPort(/*allow_listen=*/true);
  EXPECT_EQ(PROTO_QUIC, port->GetProtocol());
}

TEST_F(QuicPortTest, TestPrepareAddress) {
  std::unique_ptr<QuicPort> port = CreateQuicPort(/*allow_listen=*/true);
  port->PrepareAddress();
  EXPECT_EQ(1U, port->Candidates().size());
  EXPECT_EQ(QUIC_PROTOCOL_NAME, port->Candidates()[0].protocol());
}

TEST_F(QuicPortTest, TestCreateConnection) {
  std::unique_ptr<QuicPort> port = CreateQuicPort(/*allow_listen=*/true);
  port->PrepareAddress();
  ASSERT_EQ(1U, port->Candidates().size());
  
  // Create a remote candidate
  cricket::Candidate remote_candidate;
  remote_candidate.set_address(rtc::SocketAddress("192.168.1.2", 1234));
  remote_candidate.set_protocol(QUIC_PROTOCOL_NAME);
  remote_candidate.set_type(cricket::HOST_PORT_TYPE);
  
  // Create a connection to the remote candidate
  cricket::Connection* conn = port->CreateConnection(remote_candidate, Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, conn);
  EXPECT_EQ(conn->remote_candidate().address(), remote_candidate.address());
  EXPECT_EQ(conn->remote_candidate().protocol(), QUIC_PROTOCOL_NAME);
}

TEST_F(QuicPortTest, TestSendData) {
  std::unique_ptr<QuicPort> port = CreateQuicPort(/*allow_listen=*/true);
  port->PrepareAddress();
  ASSERT_EQ(1U, port->Candidates().size());
  
  // Create a remote candidate
  cricket::Candidate remote_candidate;
  remote_candidate.set_address(rtc::SocketAddress("192.168.1.2", 1234));
  remote_candidate.set_protocol(QUIC_PROTOCOL_NAME);
  remote_candidate.set_type(cricket::HOST_PORT_TYPE);
  
  // Create a connection to the remote candidate
  cricket::Connection* conn = port->CreateConnection(remote_candidate, Port::ORIGIN_MESSAGE);
  ASSERT_NE(nullptr, conn);
  
  // Prepare test data
  const char kData[] = "QUIC test data";
  size_t size = sizeof(kData) - 1;  // Exclude null terminator
  
  // Send data through the connection
  // Note: This will likely fail in the test since there's no actual remote endpoint
  // but we're just testing the API
  rtc::PacketOptions options;
  int result = conn->Send(kData, size, options);
  
  // Since we're sending to a non-existent endpoint, we expect this to fail
  EXPECT_LT(result, 0);
}

}  // namespace
}  // namespace cricket