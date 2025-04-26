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

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/rtc_error.h"
#include "api/transport/data_channel_transport_interface.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"

namespace cricket {

namespace {
// Constants for QUIC transport
constexpr int kMaxMessageSize = 1200;  // Maximum size of a QUIC datagram
constexpr int kSignalingChannelId = 0;  // Channel ID for signaling messages

// Message types for signaling
enum class SignalingMessageType : uint8_t {
  kOffer = 1,
  kAnswer = 2,
  kIceCandidate = 3,
  kIceCandidateRemoval = 4
};

// Structure of a signaling message:
// [1 byte: message type][variable: payload]
}  // namespace

std::unique_ptr<QuicTransportChannel> QuicTransportChannel::Create(
    absl::string_view transport_name,
    int component,
    PortAllocator* allocator) {
  return absl::WrapUnique(
      new QuicTransportChannel(transport_name, component, allocator));
}

QuicTransportChannel::QuicTransportChannel(absl::string_view transport_name,
                                         int component,
                                         PortAllocator* allocator)
    : transport_name_(transport_name),
      component_(component),
      allocator_(allocator),
      ice_role_(ICEROLE_CONTROLLED),
      ice_tiebreaker_(rtc::CreateRandomId64()),
      remote_ice_mode_(ICEMODE_FULL),
      gathering_state_(kIceGatheringNew),
      transport_state_(IceTransportState::STATE_INIT),
      ice_transport_state_(webrtc::IceTransportState::kNew),
      selected_connection_(nullptr),
      writable_(false),
      receiving_(false),
      data_sink_(nullptr),
      network_thread_(rtc::Thread::Current()) {
  RTC_DCHECK(allocator_);
}

QuicTransportChannel::~QuicTransportChannel() {
  // Close all open channels
  for (int channel_id : open_channels_) {
    CloseChannel(channel_id);
  }
  open_channels_.clear();
  
  // Clean up the QUIC port
  quic_port_.reset();
}

// IceTransportInternal implementation
IceTransportState QuicTransportChannel::GetState() const {
  return transport_state_;
}

webrtc::IceTransportState QuicTransportChannel::GetIceTransportState() const {
  return ice_transport_state_;
}

const std::string& QuicTransportChannel::transport_name() const {
  return transport_name_;
}

int QuicTransportChannel::component() const {
  return component_;
}

bool QuicTransportChannel::writable() const {
  return writable_;
}

bool QuicTransportChannel::receiving() const {
  return receiving_;
}

void QuicTransportChannel::SetIceRole(IceRole role) {
  if (role == ice_role_) {
    return;
  }
  ice_role_ = role;
  if (quic_port_) {
    quic_port_->SetIceRole(ice_role_);
  }
}

IceRole QuicTransportChannel::GetIceRole() const {
  return ice_role_;
}

void QuicTransportChannel::SetIceTiebreaker(uint64_t tiebreaker) {
  ice_tiebreaker_ = tiebreaker;
  if (quic_port_) {
    quic_port_->SetIceTiebreaker(tiebreaker);
  }
}

void QuicTransportChannel::SetIceParameters(const IceParameters& ice_params) {
  RTC_DCHECK(network_thread_->IsCurrent());
  ice_parameters_ = ice_params;
  if (quic_port_) {
    quic_port_->SetIceParameters(ice_parameters_);
  }
}

void QuicTransportChannel::SetRemoteIceParameters(
    const IceParameters& ice_params) {
  RTC_DCHECK(network_thread_->IsCurrent());
  remote_ice_parameters_ = ice_params;
  if (quic_port_) {
    quic_port_->SetRemoteIceParameters(remote_ice_parameters_);
  }
}

void QuicTransportChannel::SetRemoteIceMode(IceMode mode) {
  remote_ice_mode_ = mode;
}

void QuicTransportChannel::SetIceConfig(const IceConfig& config) {
  ice_config_ = config;
  if (quic_port_) {
    quic_port_->SetIceConfig(ice_config_);
  }
}

absl::optional<int> QuicTransportChannel::GetRttEstimate() {
  if (selected_connection_) {
    return selected_connection_->rtt();
  }
  return absl::nullopt;
}

const Connection* QuicTransportChannel::selected_connection() const {
  return selected_connection_;
}

absl::optional<const CandidatePair> QuicTransportChannel::GetSelectedCandidatePair() const {
  if (selected_connection_) {
    return CandidatePair(selected_connection_->local_candidate(),
                         selected_connection_->remote_candidate());
  }
  return absl::nullopt;
}

void QuicTransportChannel::MaybeStartGathering() {
  if (!quic_port_) {
    CreateQuicPort();
  }
  
  if (gathering_state_ == kIceGatheringNew) {
    gathering_state_ = kIceGatheringGathering;
    SignalGatheringState(this);
  }
  
  if (quic_port_) {
    quic_port_->MaybeStartGathering();
  }
}

void QuicTransportChannel::AddRemoteCandidate(const Candidate& candidate) {
  RTC_DCHECK(network_thread_->IsCurrent());
  
  if (!quic_port_) {
    CreateQuicPort();
  }
  
  // Only process candidates matching our component.
  if (candidate.component() != component_) {
    return;
  }
  
  // Check if this is a QUIC candidate
  if (candidate.protocol() != QUIC_PROTOCOL_NAME) {
    RTC_LOG(LS_WARNING) << "Ignoring non-QUIC candidate: " << candidate.ToString();
    return;
  }
  
  quic_port_->AddRemoteCandidate(candidate);
}

void QuicTransportChannel::RemoveRemoteCandidate(const Candidate& candidate) {
  if (!quic_port_) {
    return;
  }
  quic_port_->RemoveRemoteCandidate(candidate);
}

void QuicTransportChannel::RemoveAllRemoteCandidates() {
  if (!quic_port_) {
    return;
  }
  quic_port_->RemoveAllRemoteCandidates();
}

IceGatheringState QuicTransportChannel::gathering_state() const {
  return gathering_state_;
}

void QuicTransportChannel::SetMetricsObserver(
    webrtc::MetricsObserverInterface* observer) {
  // Not implemented
}

bool QuicTransportChannel::GetStats(IceTransportStats* ice_transport_stats) {
  // Not implemented
  return false;
}

void QuicTransportChannel::SetIceTransportFactory(IceTransportFactory* factory) {
  // Not implemented
}

void QuicTransportChannel::SetIceCredentialsFromLocalCandidates(
    const Candidates& local_candidates) {
  // Not implemented
}

void QuicTransportChannel::LogCandidatePairConfig(
    const CandidatePairInterface& candidate_pair) {
  // Not implemented
}

void QuicTransportChannel::LogCandidatePairEvent(
    const CandidatePairInterface& candidate_pair,
    const std::string& event_type) {
  // Not implemented
}

// DataChannelTransportInterface implementation
RTCError QuicTransportChannel::OpenChannel(int channel_id) {
  if (channel_id < 0) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER,
                           "Invalid channel_id");
  }
  
  if (open_channels_.find(channel_id) != open_channels_.end()) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE,
                           "Channel already open");
  }
  
  open_channels_.insert(channel_id);
  return webrtc::RTCError::OK();
}

RTCError QuicTransportChannel::SendData(
    int channel_id,
    const webrtc::SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (open_channels_.find(channel_id) == open_channels_.end()) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE,
                           "Channel not open");
  }
  
  if (!selected_connection_ || !writable_) {
    return webrtc::RTCError(webrtc::RTCErrorType::NETWORK_ERROR,
                           "Not connected");
  }
  
  if (buffer.size() > kMaxMessageSize) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER,
                           "Message too large");
  }
  
  // For signaling channel, handle specially
  if (channel_id == kSignalingChannelId) {
    SendSignalingMessage(buffer);
    return webrtc::RTCError::OK();
  }
  
  // Prepend channel_id to the data
  rtc::CopyOnWriteBuffer packet(buffer.size() + sizeof(int));
  memcpy(packet.MutableData(), &channel_id, sizeof(int));
  memcpy(packet.MutableData() + sizeof(int), buffer.data(), buffer.size());
  
  // Send the data through the selected connection
  rtc::PacketOptions packet_options;
  int sent = selected_connection_->Send(packet.data(), packet.size(), packet_options);
  
  if (sent <= 0) {
    return webrtc::RTCError(webrtc::RTCErrorType::NETWORK_ERROR,
                           "Failed to send data");
  }
  
  return webrtc::RTCError::OK();
}

RTCError QuicTransportChannel::CloseChannel(int channel_id) {
  if (open_channels_.find(channel_id) == open_channels_.end()) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_STATE,
                           "Channel not open");
  }
  
  open_channels_.erase(channel_id);
  
  // Notify the sink that the channel is closed
  if (data_sink_) {
    data_sink_->OnChannelClosed(channel_id);
  }
  
  return webrtc::RTCError::OK();
}

void QuicTransportChannel::SetDataSink(webrtc::DataChannelSink* sink) {
  data_sink_ = sink;
  
  // If we're already connected, notify the sink
  if (data_sink_ && writable_) {
    data_sink_->OnReadyToSend();
  }
}

bool QuicTransportChannel::IsReadyToSend() const {
  return writable_;
}

// Signal handlers
void QuicTransportChannel::OnReadPacket(
    rtc::AsyncPacketSocket* socket,
    const char* data,
    size_t size,
    const rtc::SocketAddress& remote_addr,
    const int64_t& packet_time_us) {
  // Check if this is a signaling message
  if (size >= 1) {
    uint8_t first_byte = static_cast<uint8_t>(data[0]);
    if (first_byte <= static_cast<uint8_t>(SignalingMessageType::kIceCandidateRemoval)) {
      if (ParseSignalingMessage(data, size)) {
        return;
      }
    }
  }
  
  // If not a signaling message, it's a data channel message
  if (size >= sizeof(int)) {
    int channel_id;
    memcpy(&channel_id, data, sizeof(int));
    HandleDataPacket(data + sizeof(int), size - sizeof(int), channel_id);
  }
}

void QuicTransportChannel::OnSentPacket(
    rtc::AsyncPacketSocket* socket,
    const rtc::SentPacket& sent_packet) {
  SignalSentPacket(sent_packet);
}

void QuicTransportChannel::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  if (!writable_) {
    writable_ = true;
    SignalWritableState(this);
    
    // Notify the data sink that we're ready to send
    if (data_sink_) {
      data_sink_->OnReadyToSend();
    }
  }
}

void QuicTransportChannel::OnConnectionStateChange(Connection* connection) {
  RTC_DCHECK(network_thread_->IsCurrent());
  
  if (connection == selected_connection_) {
    bool was_writable = writable_;
    writable_ = selected_connection_->writable();
    
    if (was_writable != writable_) {
      SignalWritableState(this);
      
      if (writable_ && data_sink_) {
        data_sink_->OnReadyToSend();
      }
    }
    
    bool was_receiving = receiving_;
    receiving_ = selected_connection_->receiving();
    
    if (was_receiving != receiving_) {
      SignalReceivingState(this);
    }
    
    // Update the transport state
    UpdateConnectionStates();
  } else if (selected_connection_ == nullptr && connection->writable()) {
    // If we don't have a selected connection and this connection becomes writable,
    // select it.
    MaybeSwitchSelectedConnection(connection);
  }
}

// Private helper methods
void QuicTransportChannel::CreateQuicPort() {
  RTC_DCHECK(network_thread_->IsCurrent());
  
  if (quic_port_) {
    return;
  }
  
  // Create the allocator session
  std::string username = ice_parameters_.ufrag;
  std::string password = ice_parameters_.pwd;
  
  // Create the QUIC port
  quic_port_ = QuicPort::Create(
      network_thread_, allocator_->socket_factory(),
      allocator_->GetNetworks()[0], allocator_->min_port(),
      allocator_->max_port(), username, password, true);
  
  if (!quic_port_) {
    RTC_LOG(LS_ERROR) << "Failed to create QUIC port";
    return;
  }
  
  // Set up the port
  quic_port_->SetIceRole(ice_role_);
  quic_port_->SetIceTiebreaker(ice_tiebreaker_);
  quic_port_->SetIceParameters(ice_parameters_);
  
  // Connect signals
  quic_port_->SignalCandidateGathered.connect(
      this, &QuicTransportChannel::OnCandidateGathered);
  quic_port_->SignalCandidatesRemoved.connect(
      this, &QuicTransportChannel::OnCandidatesRemoved);
  quic_port_->SignalRoleConflict.connect(
      this, &QuicTransportChannel::OnRoleConflict);
  quic_port_->SignalConnectionStateChange.connect(
      this, &QuicTransportChannel::OnConnectionStateChange);
  
  // Start gathering candidates
  quic_port_->PrepareAddress();
}

void QuicTransportChannel::MaybeSwitchSelectedConnection(Connection* conn) {
  if (conn == selected_connection_) {
    return;
  }
  
  RTC_LOG(LS_INFO) << "Switching selected connection from "
                  << (selected_connection_ ? selected_connection_->ToString() : "none")
                  << " to " << conn->ToString();
  
  selected_connection_ = conn;
  writable_ = selected_connection_->writable();
  receiving_ = selected_connection_->receiving();
  
  SignalWritableState(this);
  SignalReceivingState(this);
  
  // Update the transport state
  UpdateConnectionStates();
  
  // Notify the data sink that we're ready to send if we're writable
  if (writable_ && data_sink_) {
    data_sink_->OnReadyToSend();
  }
}

void QuicTransportChannel::UpdateConnectionStates() {
  IceTransportState new_state = IceTransportState::STATE_INIT;
  webrtc::IceTransportState new_ice_state = webrtc::IceTransportState::kNew;
  
  if (selected_connection_) {
    if (selected_connection_->writable() && selected_connection_->receiving()) {
      new_state = IceTransportState::STATE_COMPLETED;
      new_ice_state = webrtc::IceTransportState::kCompleted;
    } else if (selected_connection_->writable()) {
      new_state = IceTransportState::STATE_CONNECTING;
      new_ice_state = webrtc::IceTransportState::kConnected;
    } else {
      new_state = IceTransportState::STATE_FAILED;
      new_ice_state = webrtc::IceTransportState::kFailed;
    }
  } else {
    new_state = IceTransportState::STATE_FAILED;
    new_ice_state = webrtc::IceTransportState::kFailed;
  }
  
  if (transport_state_ != new_state) {
    transport_state_ = new_state;
    SignalStateChanged(this);
  }
  
  if (ice_transport_state_ != new_ice_state) {
    ice_transport_state_ = new_ice_state;
    SignalIceTransportStateChanged(this);
  }
}

void QuicTransportChannel::HandleDataPacket(
    const char* data, size_t size, int channel_id) {
  if (!data_sink_) {
    return;
  }
  
  // Check if the channel is open
  if (open_channels_.find(channel_id) == open_channels_.end()) {
    // Auto-open the channel if it's not already open
    open_channels_.insert(channel_id);
  }
  
  // Create a buffer with the data
  rtc::CopyOnWriteBuffer buffer(data, size);
  
  // Notify the data sink
  data_sink_->OnDataReceived(
      channel_id, webrtc::DataMessageType::kBinary, buffer);
}

void QuicTransportChannel::SendSignalingMessage(
    const rtc::CopyOnWriteBuffer& message) {
  if (!selected_connection_ || !writable_) {
    RTC_LOG(LS_WARNING) << "Cannot send signaling message - not connected";
    return;
  }
  
  // Send the message through the selected connection
  rtc::PacketOptions packet_options;
  selected_connection_->Send(message.data(), message.size(), packet_options);
}

bool QuicTransportChannel::ParseSignalingMessage(const char* data, size_t size) {
  if (size < 1) {
    return false;
  }
  
  SignalingMessageType type = static_cast<SignalingMessageType>(data[0]);
  const char* payload = data + 1;
  size_t payload_size = size - 1;
  
  switch (type) {
    case SignalingMessageType::kOffer:
      // Handle offer
      RTC_LOG(LS_INFO) << "Received offer message";
      return true;
      
    case SignalingMessageType::kAnswer:
      // Handle answer
      RTC_LOG(LS_INFO) << "Received answer message";
      return true;
      
    case SignalingMessageType::kIceCandidate: {
      // Handle ICE candidate
      RTC_LOG(LS_INFO) << "Received ICE candidate message";
      // In a real implementation, we would parse the candidate and add it
      return true;
    }
      
    case SignalingMessageType::kIceCandidateRemoval:
      // Handle ICE candidate removal
      RTC_LOG(LS_INFO) << "Received ICE candidate removal message";
      return true;
      
    default:
      return false;
  }
}

}  // namespace cricket