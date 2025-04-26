/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_QUIC_PORT_H_
#define P2P_BASE_QUIC_PORT_H_

#include <list>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "p2p/base/connection.h"
#include "p2p/base/port.h"
#include "p2p/base/quic_library_wrapper.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/containers/flat_map.h"

namespace cricket {

class QuicConnection;

// Communicates using a local QUIC port.
//
// This class is designed to allow subclasses to take advantage of the
// connection management provided by this class. A subclass should take of all
// packet sending and preparation, but when a packet is received, it should
// call this QuicPort::OnReadPacket (3 arg) to dispatch to a connection.
class QuicPort : public Port {
 public:
  static std::unique_ptr<QuicPort> Create(
      rtc::Thread* thread,
      rtc::PacketSocketFactory* factory,
      const rtc::Network* network,
      uint16_t min_port,
      uint16_t max_port,
      absl::string_view username,
      absl::string_view password,
      bool allow_listen,
      const webrtc::FieldTrialsView* field_trials = nullptr) {
    // Using `new` to access a non-public constructor.
    return absl::WrapUnique(new QuicPort(thread, factory, network, min_port,
                                        max_port, username, password,
                                        allow_listen, field_trials));
  }
  ~QuicPort() override;

  Connection* CreateConnection(const Candidate& address,
                               CandidateOrigin origin) override;

  void PrepareAddress() override;

  // Options apply to accepted sockets.
  int GetOption(rtc::Socket::Option opt, int* value) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetError() override;
  bool SupportsProtocol(absl::string_view protocol) const override;
  ProtocolType GetProtocol() const override;

 protected:
  QuicPort(rtc::Thread* thread,
          rtc::PacketSocketFactory* factory,
          const rtc::Network* network,
          uint16_t min_port,
          uint16_t max_port,
          absl::string_view username,
          absl::string_view password,
          bool allow_listen,
          const webrtc::FieldTrialsView* field_trials);

  // Handles sending using the local QUIC socket.
  int SendTo(const void* data,
             size_t size,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options,
             bool payload) override;

  // Accepts incoming QUIC connection.
  void OnNewConnection(rtc::AsyncListenSocket* socket,
                       rtc::AsyncPacketSocket* new_socket);

 private:
  struct Incoming {
    rtc::SocketAddress addr;
    rtc::AsyncPacketSocket* socket;
  };

  void TryCreateServerSocket();

  rtc::AsyncPacketSocket* GetIncoming(const rtc::SocketAddress& addr,
                                      bool remove = false);

  // Receives packet signal from the local QUIC Socket.
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const int64_t& packet_time_us);

  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) override;

  void OnReadyToSend(rtc::AsyncPacketSocket* socket);
  
  // Called when a QUIC connection is established
  void OnQuicConnectionEstablished(QuicLibraryWrapper* wrapper,
                                  const rtc::SocketAddress& remote_addr);

  bool allow_listen_;
  std::unique_ptr<rtc::AsyncListenSocket> listen_socket_;
  // QUIC server socket and wrapper
  std::unique_ptr<rtc::AsyncPacketSocket> server_socket_;
  std::unique_ptr<QuicLibraryWrapper> server_quic_wrapper_;
  // Options to be applied to accepted sockets.
  webrtc::flat_map<rtc::Socket::Option, int> socket_options_;

  int error_;
  std::list<Incoming> incoming_;

  friend class QuicConnection;
};

class QuicConnection : public Connection, public sigslot::has_slots<> {
 public:
  // Connection is outgoing unless socket is specified
  QuicConnection(rtc::WeakPtr<Port> quic_port,
                const Candidate& candidate,
                rtc::AsyncPacketSocket* socket = nullptr);
  ~QuicConnection() override;

  int Send(const void* data,
           size_t size,
           const rtc::PacketOptions& options) override;
  int GetError() override;

  rtc::AsyncPacketSocket* socket() { return socket_.get(); }
  QuicLibraryWrapper* quic_wrapper() { return quic_wrapper_.get(); }

  // Allow test cases to overwrite the default timeout period.
  int reconnection_timeout() const { return reconnection_timeout_; }
  void set_reconnection_timeout(int timeout_in_ms) {
    reconnection_timeout_ = timeout_in_ms;
  }

 protected:
  // Set waiting_for_stun_binding_complete_ to false to allow data packets in
  // addition to what Port::OnConnectionRequestResponse does.
  void OnConnectionRequestResponse(StunRequest* req,
                                   StunMessage* response) override;

 private:
  friend class QuicPort;  // For `MaybeReconnect()`.

  // Helper function to handle the case when Ping or Send fails with error
  // related to socket close.
  void MaybeReconnect();

  void CreateOutgoingQuicSocket() RTC_RUN_ON(network_thread());

  void ConnectSocketSignals(rtc::AsyncPacketSocket* socket)
      RTC_RUN_ON(network_thread());

  void DisconnectSocketSignals(rtc::AsyncPacketSocket* socket)
      RTC_RUN_ON(network_thread());

  // Initialize the QUIC wrapper for this connection
  bool InitializeQuicWrapper(rtc::AsyncPacketSocket* socket, bool is_server);

  // Connect QUIC wrapper signals
  void ConnectQuicWrapperSignals(QuicLibraryWrapper* wrapper)
      RTC_RUN_ON(network_thread());

  // Disconnect QUIC wrapper signals
  void DisconnectQuicWrapperSignals(QuicLibraryWrapper* wrapper)
      RTC_RUN_ON(network_thread());

  void OnConnect(rtc::AsyncPacketSocket* socket);
  void OnClose(rtc::AsyncPacketSocket* socket, int error);
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const int64_t& packet_time_us);
  void OnReadyToSend(rtc::AsyncPacketSocket* socket);
  void OnDestroyed(Connection* c);

  // QUIC wrapper callbacks
  void OnQuicReadPacket(QuicLibraryWrapper* wrapper,
                       const char* data,
                       size_t size);
  void OnQuicReadyToSend(QuicLibraryWrapper* wrapper);
  void OnQuicConnectionEstablished(QuicLibraryWrapper* wrapper,
                                  const rtc::SocketAddress& remote_addr);
  void OnQuicConnectionClosed(QuicLibraryWrapper* wrapper, int error);

  QuicPort* quic_port() {
    RTC_DCHECK_EQ(port()->GetProtocol(), PROTO_QUIC);
    return static_cast<QuicPort*>(port());
  }

  std::unique_ptr<rtc::AsyncPacketSocket> socket_;
  std::unique_ptr<QuicLibraryWrapper> quic_wrapper_;
  int error_;
  const bool outgoing_;

  // Guard against multiple outgoing quic connection during a reconnect.
  bool connection_pending_;

  // Guard against data packets sent when we reconnect a QUIC connection. During
  // reconnecting, when a new quic connection has being made, we can't send data
  // packets out until the STUN binding is completed (i.e. the write state is
  // set to WRITABLE again by Connection::OnConnectionRequestResponse). IPC
  // socket, when receiving data packets before that, will trigger OnError which
  // will terminate the newly created connection.
  bool pretending_to_be_writable_;

  // Allow test case to overwrite the default timeout period.
  int reconnection_timeout_;

  webrtc::ScopedTaskSafety network_safety_;
};

}  // namespace cricket

#endif  // P2P_BASE_QUIC_PORT_H_