/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <iostream>

#include "absl/memory/memory.h"
#include "api/candidate.h"
#include "api/transport/data_channel_transport_interface.h"
#include "p2p/base/basic_port_allocator.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/quiche_transport_channel.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"

// Example data channel sink implementation
class ExampleDataChannelSink : public webrtc::DataChannelSink {
 public:
  void OnDataReceived(int channel_id,
                      webrtc::DataMessageType type,
                      const rtc::CopyOnWriteBuffer& buffer) override {
    std::string data(buffer.data<char>(), buffer.size());
    std::cout << "Received data on channel " << channel_id << ": " << data << std::endl;
  }

  void OnChannelClosing(int channel_id) override {
    std::cout << "Channel " << channel_id << " is closing" << std::endl;
  }

  void OnChannelClosed(int channel_id) override {
    std::cout << "Channel " << channel_id << " is closed" << std::endl;
  }

  void OnReadyToSend() override {
    std::cout << "Ready to send data" << std::endl;
    ready_to_send_ = true;
  }

  void OnTransportClosed(webrtc::RTCError error) override {
    std::cout << "Transport closed with error: " << error.message() << std::endl;
    ready_to_send_ = false;
  }

  bool IsReadyToSend() const { return ready_to_send_; }

 private:
  bool ready_to_send_ = false;
};

// Example of how to use the QUIC transport channel for signaling
int main(int argc, char* argv[]) {
  // Initialize logging
  rtc::LogMessage::LogToDebug(rtc::LS_INFO);
  rtc::LogMessage::LogTimestamps();
  rtc::LogMessage::LogThreads();

  // Create the thread and socket server
  rtc::PhysicalSocketServer socket_server;
  rtc::Thread main_thread(&socket_server);
  main_thread.Start();

  // Create the network manager
  rtc::NetworkManager network_manager;
  
  // Create the port allocator
  cricket::BasicPortAllocator port_allocator(&network_manager, 
                                            rtc::SocketAddress(rtc::GetAnyIP(AF_INET), 0));
  
  // Create the QUIC transport channel
  std::unique_ptr<cricket::QuicheTransportChannel> transport_channel =
      cricket::QuicheTransportChannel::Create("quic_transport", 1, &port_allocator);
  
  // Set ICE parameters
  cricket::IceParameters ice_params;
  ice_params.ufrag = "UFRAG0001";
  ice_params.pwd = "PASSWORD0001";
  transport_channel->SetIceParameters(ice_params);
  
  // Set ICE role (controlling = offerer, controlled = answerer)
  transport_channel->SetIceRole(cricket::ICEROLE_CONTROLLING);
  
  // Create the data channel sink
  ExampleDataChannelSink data_sink;
  transport_channel->SetDataSink(&data_sink);
  
  // Open the signaling channel (channel 0)
  webrtc::RTCError error = transport_channel->OpenChannel(0);
  if (!error.ok()) {
    std::cerr << "Failed to open signaling channel: " << error.message() << std::endl;
    return 1;
  }
  
  // Start gathering candidates
  transport_channel->MaybeStartGathering();
  
  // Wait for candidates to be gathered
  std::cout << "Gathering candidates..." << std::endl;
  main_thread.ProcessMessages(5000);
  
  // In a real application, you would exchange candidates with the remote peer
  // and add them using transport_channel->AddRemoteCandidate()
  
  // Wait for connection to be established
  std::cout << "Waiting for connection..." << std::endl;
  for (int i = 0; i < 30 && !transport_channel->writable(); ++i) {
    main_thread.ProcessMessages(1000);
  }
  
  if (!transport_channel->writable()) {
    std::cerr << "Failed to establish connection" << std::endl;
    return 1;
  }
  
  std::cout << "Connection established!" << std::endl;
  
  // Send a signaling message
  const char* offer_message = "This is an offer message";
  rtc::CopyOnWriteBuffer buffer(offer_message, strlen(offer_message));
  webrtc::SendDataParams params;
  params.ordered = true;
  
  error = transport_channel->SendData(0, params, buffer);
  if (!error.ok()) {
    std::cerr << "Failed to send signaling message: " << error.message() << std::endl;
    return 1;
  }
  
  // Process messages for a while to handle any incoming data
  std::cout << "Processing messages..." << std::endl;
  main_thread.ProcessMessages(10000);
  
  // Close the channel
  transport_channel->CloseChannel(0);
  
  std::cout << "Example completed successfully" << std::endl;
  return 0;
}