/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_QUICHE_TRANSPORT_CHANNEL_H_
#define P2P_BASE_QUICHE_TRANSPORT_CHANNEL_H_

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/transport/stun.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/quic_port.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

// Forward declarations for quiche
namespace quic {
class QuicConnection;
class QuicSession;
class QuicConfig;
class QuicClock;
class QuicAlarmFactory;
class QuicConnectionHelperInterface;
class QuicCryptoClientConfig;
class QuicCryptoServerConfig;
}  // namespace quic

namespace cricket {

// QuicheTransportChannel implements a transport channel using Google's QUIC protocol
// via the quiche library. This channel can be used for signaling and data transfer
// in WebRTC.
class QuicheTransportChannel : public IceTransportInternal,
                              public webrtc::DataChannelTransportInterface,
                              public sigslot::has_slots<> {
 public:
  // Factory method to create a QuicheTransportChannel.
  static std::unique_ptr<QuicheTransportChannel> Create(
      absl::string_view transport_name,
      int component,
      PortAllocator* allocator);

  ~QuicheTransportChannel() override;

  // IceTransportInternal implementation.
  IceTransportState GetState() const override;
  webrtc::IceTransportState GetIceTransportState() const override;
  const std::string& transport_name() const override;
  int component() const override;
  bool writable() const override;
  bool receiving() const override;
  void SetIceRole(IceRole role) override;
  IceRole GetIceRole() const override;
  void SetIceTiebreaker(uint64_t tiebreaker) override;
  void SetIceParameters(const IceParameters& ice_params) override;
  void SetRemoteIceParameters(const IceParameters& ice_params) override;
  void SetRemoteIceMode(IceMode mode) override;
  void SetIceConfig(const IceConfig& config) override;
  absl::optional<int> GetRttEstimate() override;
  const Connection* selected_connection() const override;
  absl::optional<const CandidatePair> GetSelectedCandidatePair() const override;
  void MaybeStartGathering() override;
  void AddRemoteCandidate(const Candidate& candidate) override;
  void RemoveRemoteCandidate(const Candidate& candidate) override;
  void RemoveAllRemoteCandidates() override;
  IceGatheringState gathering_state() const override;
  void SetMetricsObserver(webrtc::MetricsObserverInterface* observer) override;
  bool GetStats(IceTransportStats* ice_transport_stats) override;
  void SetIceTransportFactory(IceTransportFactory* factory) override;
  void SetIceCredentialsFromLocalCandidates(
      const Candidates& local_candidates) override;
  void LogCandidatePairConfig(
      const CandidatePairInterface& candidate_pair) override;
  void LogCandidatePairEvent(const CandidatePairInterface& candidate_pair,
                             const std::string& event_type) override;

  // DataChannelTransportInterface implementation.
  RTCError OpenChannel(int channel_id) override;
  RTCError SendData(int channel_id,
                    const webrtc::SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override;
  RTCError CloseChannel(int channel_id) override;
  void SetDataSink(webrtc::DataChannelSink* sink) override;
  bool IsReadyToSend() const override;

  // Signal handlers for QuicPort.
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const int64_t& packet_time_us);
  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet);
  void OnReadyToSend(rtc::AsyncPacketSocket* socket);
  void OnConnectionStateChange(Connection* connection);

 private:
  QuicheTransportChannel(absl::string_view transport_name,
                        int component,
                        PortAllocator* allocator);

  // Helper methods
  void CreateQuicPort();
  void MaybeSwitchSelectedConnection(Connection* conn);
  void UpdateConnectionStates();
  void HandleDataPacket(const char* data, size_t size, int channel_id);
  void SendSignalingMessage(const rtc::CopyOnWriteBuffer& message);
  bool ParseSignalingMessage(const char* data, size_t size);
  
  // QUIC-specific methods
  bool InitializeQuicConnection(const rtc::SocketAddress& remote_addr);
  void CleanupQuicConnection();
  void ProcessQuicPacket(const char* data, size_t size, 
                         const rtc::SocketAddress& remote_addr);

  // ICE-related state
  std::string transport_name_;
  int component_;
  PortAllocator* allocator_;
  IceRole ice_role_;
  uint64_t ice_tiebreaker_;
  IceParameters ice_parameters_;
  IceParameters remote_ice_parameters_;
  IceMode remote_ice_mode_;
  IceConfig ice_config_;
  IceGatheringState gathering_state_;
  IceTransportState transport_state_;
  webrtc::IceTransportState ice_transport_state_;

  // QUIC-related state
  std::unique_ptr<QuicPort> quic_port_;
  Connection* selected_connection_;
  bool writable_;
  bool receiving_;

  // quiche-specific state
  std::unique_ptr<quic::QuicConnectionHelperInterface> quic_helper_;
  std::unique_ptr<quic::QuicAlarmFactory> quic_alarm_factory_;
  std::unique_ptr<quic::QuicClock> quic_clock_;
  std::unique_ptr<quic::QuicConfig> quic_config_;
  std::unique_ptr<quic::QuicConnection> quic_connection_;
  std::unique_ptr<quic::QuicSession> quic_session_;
  std::unique_ptr<quic::QuicCryptoClientConfig> crypto_client_config_;
  std::unique_ptr<quic::QuicCryptoServerConfig> crypto_server_config_;
  
  // Data channel state
  webrtc::DataChannelSink* data_sink_;
  std::set<int> open_channels_;
  std::map<int, std::string> channel_data_buffers_;

  // Thread safety
  rtc::Thread* const network_thread_;
};

}  // namespace cricket

#endif  // P2P_BASE_QUICHE_TRANSPORT_CHANNEL_H_