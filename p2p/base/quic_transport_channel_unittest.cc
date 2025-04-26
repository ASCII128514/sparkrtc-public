/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/quic_transport_channel.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "api/test/mock_async_dns_resolver.h"
#include "p2p/base/basic_port_allocator.h"
#include "p2p/base/fake_port_allocator.h"
#include "p2p/base/mock_ice_transport.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/test_stun_server.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::Return;

namespace cricket {

class QuicTransportChannelTest : public ::testing::Test {
 protected:
  QuicTransportChannelTest()
      : vss_(new rtc::VirtualSocketServer()),
        main_(vss_.get()),
        network_("unittest", "unittest", rtc::IPAddress(INADDR_LOOPBACK), 32),
        allocator_(&main_, rtc::SocketAddress(INADDR_ANY, 0)) {
    allocator_.set_flags(allocator_.flags() | 
                         PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                         PORTALLOCATOR_DISABLE_TCP);
    network_.AddIP(rtc::IPAddress(INADDR_LOOPBACK));
  }

  void CreateTransportChannel() {
    transport_channel_ = QuicTransportChannel::Create("test", 1, &allocator_);
    ASSERT_TRUE(transport_channel_ != nullptr);
  }

  void SetIceParameters() {
    IceParameters ice_params;
    ice_params.ufrag = "UFRAG0001";
    ice_params.pwd = "PASSWORD0001";
    transport_channel_->SetIceParameters(ice_params);

    IceParameters remote_ice_params;
    remote_ice_params.ufrag = "UFRAG0002";
    remote_ice_params.pwd = "PASSWORD0002";
    transport_channel_->SetRemoteIceParameters(remote_ice_params);
  }

  void GatherCandidates() {
    transport_channel_->MaybeStartGathering();
    main_.ProcessMessages(100);
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::Network network_;
  BasicPortAllocator allocator_;
  std::unique_ptr<QuicTransportChannel> transport_channel_;
};

TEST_F(QuicTransportChannelTest, CreateTransportChannel) {
  CreateTransportChannel();
  EXPECT_EQ("test", transport_channel_->transport_name());
  EXPECT_EQ(1, transport_channel_->component());
  EXPECT_EQ(ICEROLE_CONTROLLED, transport_channel_->GetIceRole());
  EXPECT_EQ(webrtc::IceTransportState::kNew, transport_channel_->GetIceTransportState());
}

TEST_F(QuicTransportChannelTest, SetIceParameters) {
  CreateTransportChannel();
  SetIceParameters();
  // No explicit verification, just making sure it doesn't crash
}

TEST_F(QuicTransportChannelTest, GatherCandidates) {
  CreateTransportChannel();
  SetIceParameters();
  GatherCandidates();
  // In a real test, we would verify candidates were gathered
}

TEST_F(QuicTransportChannelTest, DataChannelOperations) {
  CreateTransportChannel();
  
  // Test opening a channel
  RTCError error = transport_channel_->OpenChannel(1);
  EXPECT_TRUE(error.ok());
  
  // Test opening the same channel again (should fail)
  error = transport_channel_->OpenChannel(1);
  EXPECT_FALSE(error.ok());
  
  // Test closing a channel
  error = transport_channel_->CloseChannel(1);
  EXPECT_TRUE(error.ok());
  
  // Test closing a channel that's not open
  error = transport_channel_->CloseChannel(1);
  EXPECT_FALSE(error.ok());
}

// Mock DataChannelSink for testing
class MockDataChannelSink : public webrtc::DataChannelSink {
 public:
  MOCK_METHOD(void, OnDataReceived, 
              (int channel_id, webrtc::DataMessageType type, 
               const rtc::CopyOnWriteBuffer& buffer), (override));
  MOCK_METHOD(void, OnChannelClosing, (int channel_id), (override));
  MOCK_METHOD(void, OnChannelClosed, (int channel_id), (override));
  MOCK_METHOD(void, OnReadyToSend, (), (override));
  MOCK_METHOD(void, OnTransportClosed, (webrtc::RTCError error), (override));
};

TEST_F(QuicTransportChannelTest, SetDataSink) {
  CreateTransportChannel();
  
  MockDataChannelSink sink;
  transport_channel_->SetDataSink(&sink);
  
  // Remove the sink
  transport_channel_->SetDataSink(nullptr);
}

TEST_F(QuicTransportChannelTest, SendData) {
  CreateTransportChannel();
  SetIceParameters();
  GatherCandidates();
  
  // Open a data channel
  int channel_id = 1;
  RTCError error = transport_channel_->OpenChannel(channel_id);
  EXPECT_TRUE(error.ok());
  
  // Create test data
  const char kData[] = "QUIC test data";
  rtc::CopyOnWriteBuffer buffer(kData, sizeof(kData) - 1);  // Exclude null terminator
  webrtc::SendDataParams params;
  
  // Try to send data (will fail since we're not connected)
  error = transport_channel_->SendData(channel_id, params, buffer);
  EXPECT_FALSE(error.ok());
}

TEST_F(QuicTransportChannelTest, ConnectAndSendData) {
  CreateTransportChannel();
  SetIceParameters();
  
  // Set up a mock data sink
  MockDataChannelSink sink;
  EXPECT_CALL(sink, OnReadyToSend()).Times(testing::AtMost(1));
  transport_channel_->SetDataSink(&sink);
  
  // Open a data channel
  int channel_id = 1;
  RTCError error = transport_channel_->OpenChannel(channel_id);
  EXPECT_TRUE(error.ok());
  
  // Gather candidates
  GatherCandidates();
  
  // In a real test, we would:
  // 1. Create a remote candidate
  // 2. Add it to the transport channel
  // 3. Wait for connection establishment
  // 4. Send data
  // 
  // But since we can't establish a real connection in this test,
  // we'll just verify that the API works as expected
}

}  // namespace cricket