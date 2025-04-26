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

#include <errno.h>

#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/time_delta.h"
#include "p2p/base/p2p_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/rate_tracker.h"

namespace cricket {
using ::webrtc::SafeTask;
using ::webrtc::TimeDelta;

QuicPort::QuicPort(rtc::Thread* thread,
                 rtc::PacketSocketFactory* factory,
                 const rtc::Network* network,
                 uint16_t min_port,
                 uint16_t max_port,
                 absl::string_view username,
                 absl::string_view password,
                 bool allow_listen,
                 const webrtc::FieldTrialsView* field_trials)
    : Port(thread,
           LOCAL_PORT_TYPE,
           factory,
           network,
           min_port,
           max_port,
           username,
           password,
           field_trials),
      allow_listen_(allow_listen),
      error_(0) {
  if (allow_listen_) {
    TryCreateServerSocket();
  }
  // Set QUIC-specific options here if needed
}

QuicPort::~QuicPort() {
  listen_socket_ = nullptr;
  std::list<Incoming>::iterator it;
  for (it = incoming_.begin(); it != incoming_.end(); ++it)
    delete it->socket;
  incoming_.clear();
}

Connection* QuicPort::CreateConnection(const Candidate& address,
                                      CandidateOrigin origin) {
  if (!SupportsProtocol(address.protocol())) {
    return NULL;
  }

  // We can't accept QUIC connections incoming on other ports
  if (origin == ORIGIN_OTHER_PORT)
    return NULL;

  if (!IsCompatibleAddress(address.address())) {
    return NULL;
  }

  QuicConnection* conn = NULL;
  if (rtc::AsyncPacketSocket* socket = GetIncoming(address.address(), true)) {
    // Incoming connection; we already created a socket and connected signals,
    // so we need to hand off the "read packet" responsibility to
    // QuicConnection.
    socket->SignalReadPacket.disconnect(this);
    conn = new QuicConnection(NewWeakPtr(), address, socket);
  } else {
    // Outgoing connection, which will create a new socket for which we still
    // need to connect SignalReadyToSend and SignalSentPacket.
    conn = new QuicConnection(NewWeakPtr(), address);
    if (conn->socket()) {
      conn->socket()->SignalReadyToSend.connect(this, &QuicPort::OnReadyToSend);
      conn->socket()->SignalSentPacket.connect(this, &QuicPort::OnSentPacket);
    }
  }
  AddOrReplaceConnection(conn);
  return conn;
}

void QuicPort::PrepareAddress() {
  if (listen_socket_) {
    // Socket may be in the CLOSED state if Listen()
    // failed, we still want to add the socket address.
    RTC_LOG(LS_VERBOSE) << "Preparing QUIC address, current state: "
                        << static_cast<int>(listen_socket_->GetState());
    AddAddress(listen_socket_->GetLocalAddress(),
               listen_socket_->GetLocalAddress(), rtc::SocketAddress(),
               QUIC_PROTOCOL_NAME, "", "", LOCAL_PORT_TYPE,
               ICE_TYPE_PREFERENCE_HOST_TCP, 0, "", true);
  } else {
    RTC_LOG(LS_INFO) << ToString()
                     << ": Not listening due to firewall restrictions.";
    // Note: We still add the address, since otherwise the remote side won't
    // recognize our incoming QUIC connections.
    AddAddress(rtc::SocketAddress(Network()->GetBestIP(), DISCARD_PORT),
               rtc::SocketAddress(Network()->GetBestIP(), 0),
               rtc::SocketAddress(), QUIC_PROTOCOL_NAME, "", "",
               LOCAL_PORT_TYPE, ICE_TYPE_PREFERENCE_HOST_TCP, 0, "", true);
  }
}

int QuicPort::SendTo(const void* data,
                    size_t size,
                    const rtc::SocketAddress& addr,
                    const rtc::PacketOptions& options,
                    bool payload) {
  rtc::AsyncPacketSocket* socket = NULL;
  QuicConnection* conn = static_cast<QuicConnection*>(GetConnection(addr));

  // For Connection, this is the code path used by Ping() to establish
  // WRITABLE. It has to send through the socket directly as QuicConnection::Send
  // checks writability.
  if (conn) {
    if (!conn->connected()) {
      conn->MaybeReconnect();
      return SOCKET_ERROR;
    }
    socket = conn->socket();
    if (!socket) {
      // The failure to initialize should have been logged elsewhere,
      // so this log is not important.
      RTC_LOG(LS_INFO) << ToString()
                       << ": Attempted to send to an uninitialized socket: "
                       << addr.ToSensitiveString();
      error_ = EHOSTUNREACH;
      return SOCKET_ERROR;
    }
  } else {
    socket = GetIncoming(addr);
    if (!socket) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Attempted to send to an unknown destination: "
                        << addr.ToSensitiveString();
      error_ = EHOSTUNREACH;
      return SOCKET_ERROR;
    }
  }
  rtc::PacketOptions modified_options(options);
  CopyPortInformationToPacketInfo(&modified_options.info_signaled_after_sent);
  int sent = socket->Send(data, size, modified_options);
  if (sent < 0) {
    error_ = socket->GetError();
    // Error from this code path for a Connection (instead of from a bare
    // socket) will not trigger reconnecting. In theory, this shouldn't matter
    // as OnClose should always be called and set connected to false.
    RTC_LOG(LS_ERROR) << ToString() << ": QUIC send of " << size
                      << " bytes failed with error " << error_;
  }
  return sent;
}

int QuicPort::GetOption(rtc::Socket::Option opt, int* value) {
  auto const& it = socket_options_.find(opt);
  if (it == socket_options_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int QuicPort::SetOption(rtc::Socket::Option opt, int value) {
  socket_options_[opt] = value;
  return 0;
}

int QuicPort::GetError() {
  return error_;
}

bool QuicPort::SupportsProtocol(absl::string_view protocol) const {
  return protocol == QUIC_PROTOCOL_NAME;
}

ProtocolType QuicPort::GetProtocol() const {
  return PROTO_QUIC;
}

void QuicPort::OnNewConnection(rtc::AsyncListenSocket* socket,
                              rtc::AsyncPacketSocket* new_socket) {
  RTC_DCHECK_EQ(socket, listen_socket_.get());

  // Apply socket options to the new socket
  for (const auto& option : socket_options_) {
    new_socket->SetOption(option.first, option.second);
  }
  
  // Set up the incoming connection
  Incoming incoming;
  incoming.addr = new_socket->GetRemoteAddress();
  incoming.socket = new_socket;
  incoming.socket->SignalReadPacket.connect(this, &QuicPort::OnReadPacket);
  incoming.socket->SignalReadyToSend.connect(this, &QuicPort::OnReadyToSend);
  incoming.socket->SignalSentPacket.connect(this, &QuicPort::OnSentPacket);

  RTC_LOG(LS_INFO) << ToString() << ": Accepted QUIC connection from "
                  << incoming.addr.ToSensitiveString();
  
  // Store the incoming connection
  incoming_.push_back(incoming);
  
  // In a real implementation, we would initiate the QUIC handshake here
  // using the QuicTransportChannel
  
  // Create a candidate for this connection
  Candidate remote_candidate;
  remote_candidate.set_address(incoming.addr);
  remote_candidate.set_protocol(QUIC_PROTOCOL_NAME);
  remote_candidate.set_type(PRFLX_PORT_TYPE);
  
  // Try to create a connection for this remote candidate
  Connection* conn = CreateConnection(remote_candidate, Port::ORIGIN_THIS_PORT);
  if (conn) {
    AddConnection(conn);
    conn->SignalStateChange.connect(this, &QuicPort::OnConnectionStateChange);
    
    // Mark the connection as receiving since we've received data on it
    conn->OnReadPacket(incoming.socket, nullptr, 0, incoming.addr, 0);
  }
}

void QuicPort::TryCreateServerSocket() {
  // For QUIC, we need to create a UDP socket that will be used for QUIC
  // We use UDP as the underlying transport for QUIC
  rtc::SocketAddress local_addr(Network()->GetBestIP(), 0);
  
  // Create a UDP socket for QUIC
  rtc::AsyncPacketSocket* udp_socket = socket_factory()->CreateUdpSocket(
      local_addr, min_port(), max_port());
  
  if (!udp_socket) {
    RTC_LOG(LS_WARNING)
        << ToString()
        << ": QUIC UDP socket creation failed; continuing anyway.";
    return;
  }
  
  RTC_LOG(LS_INFO) << ToString() << ": Created QUIC UDP socket on "
                  << udp_socket->GetLocalAddress().ToSensitiveString();
  
  // Create a QUIC server wrapper
  auto quic_wrapper = QuicLibraryWrapper::CreateServerWrapper(udp_socket);
  
  if (!quic_wrapper->Initialize()) {
    RTC_LOG(LS_WARNING)
        << ToString()
        << ": QUIC server initialization failed; continuing anyway.";
    delete udp_socket;
    return;
  }
  
  if (!quic_wrapper->Accept()) {
    RTC_LOG(LS_WARNING)
        << ToString()
        << ": QUIC server accept failed; continuing anyway.";
    delete udp_socket;
    return;
  }
  
  // Store the UDP socket and QUIC wrapper
  server_socket_.reset(udp_socket);
  server_quic_wrapper_ = std::move(quic_wrapper);
  
  // Connect signals
  server_socket_->SignalReadPacket.connect(this, &QuicPort::OnReadPacket);
  server_quic_wrapper_->SignalConnectionEstablished.connect(
      this, &QuicPort::OnQuicConnectionEstablished);
  
  // For backward compatibility, we still use the listen_socket_ for address
  // but it's not actually used for QUIC
  listen_socket_ = absl::WrapUnique(socket_factory()->CreateServerTcpSocket(
      local_addr, min_port(), max_port(), false /* ssl */));
  
  if (listen_socket_) {
    RTC_LOG(LS_INFO) << ToString() << ": Created TCP listen socket on "
                    << listen_socket_->GetLocalAddress().ToSensitiveString();
    listen_socket_->SignalNewConnection.connect(this, &QuicPort::OnNewConnection);
  }
}

rtc::AsyncPacketSocket* QuicPort::GetIncoming(const rtc::SocketAddress& addr,
                                             bool remove) {
  rtc::AsyncPacketSocket* socket = NULL;
  for (std::list<Incoming>::iterator it = incoming_.begin();
       it != incoming_.end(); ++it) {
    if (it->addr == addr) {
      socket = it->socket;
      if (remove)
        incoming_.erase(it);
      break;
    }
  }
  return socket;
}

void QuicPort::OnReadPacket(rtc::AsyncPacketSocket* socket,
                           const char* data,
                           size_t size,
                           const rtc::SocketAddress& remote_addr,
                           const int64_t& packet_time_us) {
  // Check if this is a STUN packet
  bool is_stun = false;
  if (size >= kStunHeaderSize && IsStunMessage(data, size)) {
    is_stun = true;
  }

  // Find or create a connection for this remote address
  Connection* conn = GetConnection(remote_addr);
  if (!conn && is_stun) {
    // This is a STUN packet from a new remote address
    // Create a candidate for this connection
    Candidate remote_candidate;
    remote_candidate.set_address(remote_addr);
    remote_candidate.set_protocol(QUIC_PROTOCOL_NAME);
    remote_candidate.set_type(PRFLX_PORT_TYPE);
    
    // Create a connection for this remote candidate
    conn = CreateConnection(remote_candidate, Port::ORIGIN_THIS_PORT);
    if (conn) {
      AddConnection(conn);
      conn->SignalStateChange.connect(this, &QuicPort::OnConnectionStateChange);
    }
  }

  // Forward the packet to the base class
  Port::OnReadPacket(data, size, remote_addr, PROTO_QUIC);
  
  // In a real implementation, we would handle QUIC packets here
  // using the QuicTransportChannel
  if (!is_stun) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": Received QUIC data packet from "
                       << remote_addr.ToSensitiveString() << " (" << size << " bytes)";
    
    // Signal that we received a non-STUN packet
    SignalReadPacket(socket, data, size, remote_addr, packet_time_us);
  }
}

void QuicPort::OnSentPacket(rtc::AsyncPacketSocket* socket,
                           const rtc::SentPacket& sent_packet) {
  PortInterface::SignalSentPacket(sent_packet);
}

void QuicPort::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  Port::OnReadyToSend();
}

void QuicPort::OnQuicConnectionEstablished(
    QuicLibraryWrapper* wrapper,
    const rtc::SocketAddress& remote_addr) {
  RTC_DCHECK_RUN_ON(thread());
  
  RTC_LOG(LS_INFO) << ToString() << ": QUIC connection established from "
                  << remote_addr.ToSensitiveString();
  
  // Create a new candidate for this connection
  Candidate c;
  c.set_id(CreateCandidateId());
  c.set_component(component());
  c.set_type(LOCAL_PORT_TYPE);
  c.set_protocol(QUIC_PROTOCOL_NAME);
  c.set_address(wrapper->GetLocalAddress());
  c.set_priority(0);
  c.set_username(username_fragment());
  c.set_password(password());
  c.set_network_name(network()->name());
  c.set_network_type(network()->type());
  c.set_generation(generation());
  c.set_network_id(network()->id());
  c.set_foundation(ComputeFoundation(LOCAL_PORT_TYPE, QUIC_PROTOCOL_NAME,
                                    username_fragment(), network()->name()));
  
  // Add this candidate to the port
  AddOrReplaceCandidate(c);
  
  // Create a connection for this candidate
  CreateConnection(c, remote_addr, CandidateOrigin::ORIGIN_THIS_PORT);
}

QuicConnection::QuicConnection(rtc::WeakPtr<Port> quic_port,
                             const Candidate& candidate,
                             rtc::AsyncPacketSocket* socket)
    : Connection(std::move(quic_port), 0, candidate),
      socket_(socket),
      quic_wrapper_(nullptr),
      error_(0),
      outgoing_(socket == NULL),
      connection_pending_(false),
      pretending_to_be_writable_(false),
      reconnection_timeout_(cricket::CONNECTION_WRITE_CONNECT_TIMEOUT) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK_EQ(port()->GetProtocol(), PROTO_QUIC);  // Needs to be QuicPort.

  SignalDestroyed.connect(this, &QuicConnection::OnDestroyed);

  if (outgoing_) {
    CreateOutgoingQuicSocket();
  } else {
    // Incoming connections should match one of the network addresses.
    RTC_LOG(LS_VERBOSE) << ToString() << ": socket ipaddr: "
                        << socket_->GetLocalAddress().ToSensitiveString()
                        << ", port() Network:" << port()->Network()->ToString();
    RTC_DCHECK(absl::c_any_of(
        port_->Network()->GetIPs(), [this](const rtc::InterfaceAddress& addr) {
          return socket_->GetLocalAddress().ipaddr() == addr;
        }));
    ConnectSocketSignals(socket);
  }
}

QuicConnection::~QuicConnection() {
  RTC_DCHECK_RUN_ON(network_thread_);
}

int QuicConnection::Send(const void* data,
                        size_t size,
                        const rtc::PacketOptions& options) {
  if (!socket_) {
    error_ = ENOTCONN;
    return SOCKET_ERROR;
  }

  // Sending after OnClose on active side will trigger a reconnect for a
  // outgoing connection. Note that the write state is still WRITABLE as we want
  // to spend a few seconds attempting a reconnect before saying we're
  // unwritable.
  if (!connected()) {
    MaybeReconnect();
    return SOCKET_ERROR;
  }

  // Note that this is important to put this after the previous check to give
  // the connection a chance to reconnect.
  if (pretending_to_be_writable_ || write_state() != STATE_WRITABLE) {
    // TODO(?): Should STATE_WRITE_TIMEOUT return a non-blocking error?
    error_ = ENOTCONN;
    return SOCKET_ERROR;
  }
  stats_.sent_total_packets++;
  rtc::PacketOptions modified_options(options);
  quic_port()->CopyPortInformationToPacketInfo(
      &modified_options.info_signaled_after_sent);
      
  int sent = 0;
  
  // If we have a QUIC wrapper, use it to send the data
  if (quic_wrapper_) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": Sending QUIC data packet through wrapper to "
                       << remote_candidate().address().ToSensitiveString()
                       << " (" << size << " bytes)";
    sent = quic_wrapper_->Send(data, size, modified_options);
  } else {
    // Fall back to sending directly through the socket
    RTC_LOG(LS_VERBOSE) << ToString() << ": Sending QUIC data packet directly to "
                       << remote_candidate().address().ToSensitiveString()
                       << " (" << size << " bytes)";
    sent = socket_->Send(data, size, modified_options);
  }
  int64_t now = rtc::TimeMillis();
  if (sent < 0) {
    stats_.sent_discarded_packets++;
    error_ = socket_->GetError();
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to send QUIC data packet, error="
                       << error_;
  } else {
    send_rate_tracker_.AddSamplesAtTime(now, sent);
  }
  last_send_data_ = now;
  return sent;
}

int QuicConnection::GetError() {
  return error_;
}

void QuicConnection::OnConnectionRequestResponse(StunRequest* req,
                                               StunMessage* response) {
  Connection::OnConnectionRequestResponse(req, response);

  // If we're not pretending to be writable, we're done here.
  if (!pretending_to_be_writable_)
    return;

  // Stop pretending to be writable if the connection has become writable due to
  // this response.
  if (writable()) {
    pretending_to_be_writable_ = false;
  }
}

void QuicConnection::MaybeReconnect() {
  // Only reconnect if the connection had been established before, and if we're
  // not already reconnecting.
  if (outgoing_ && !connection_pending_ && socket_ != NULL) {
    connection_pending_ = true;
    pretending_to_be_writable_ = true;

    // Create a new socket before deleting the old one. This ensures we don't
    // get a new socket with the same local port.
    CreateOutgoingQuicSocket();
    // We're reconnecting, so we need to set the local candidate to the new
    // socket's address.
    port()->GetSignalAddressReady().emit(port(), local_candidate_->address());
  }
}

void QuicConnection::CreateOutgoingQuicSocket() {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  // Create a UDP socket as the underlying transport for QUIC
  rtc::AsyncPacketSocket* socket = port()->socket_factory()->CreateUdpSocket(
      rtc::SocketAddress(port()->Network()->GetBestIP(), 0),
      port()->min_port(), port()->max_port());
  
  if (!socket) {
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to create UDP socket for QUIC";
    connection_pending_ = false;
    return;
  }

  RTC_LOG(LS_INFO) << ToString() << ": Created UDP socket for QUIC on "
                  << socket->GetLocalAddress().ToSensitiveString();

  // Connect socket signals
  ConnectSocketSignals(socket);

  // Store the socket
  socket_.reset(socket);
  
  // Connect the socket to the remote address
  int err = socket_->Connect(remote_candidate().address());
  if (err < 0) {
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to connect QUIC socket to "
                       << remote_candidate().address().ToSensitiveString()
                       << ", error=" << socket_->GetError();
    connection_pending_ = false;
    return;
  }

  // Initialize QUIC wrapper
  if (!InitializeQuicWrapper(socket_.get(), /*is_server=*/false)) {
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to initialize QUIC wrapper";
    connection_pending_ = false;
    return;
  }
  
  // Connect the QUIC wrapper to the remote address
  if (!quic_wrapper_->Connect()) {
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to connect QUIC wrapper to "
                       << remote_candidate().address().ToSensitiveString()
                       << ", error=" << quic_wrapper_->GetError();
    connection_pending_ = false;
    return;
  }
  
  // Send a STUN ping to the remote side to establish connectivity
  Connection::Ping(rtc::TimeMillis());

  RTC_LOG(LS_INFO) << ToString() << ": Initiated QUIC connection to "
                  << remote_candidate().address().ToSensitiveString();
}

bool QuicConnection::InitializeQuicWrapper(rtc::AsyncPacketSocket* socket, bool is_server) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  if (is_server) {
    quic_wrapper_ = QuicLibraryWrapper::CreateServerWrapper(socket);
  } else {
    quic_wrapper_ = QuicLibraryWrapper::CreateClientWrapper(
        socket, remote_candidate().address());
  }
  
  if (!quic_wrapper_) {
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to create QUIC wrapper";
    return false;
  }
  
  // Connect QUIC wrapper signals
  ConnectQuicWrapperSignals(quic_wrapper_.get());
  
  // Initialize the QUIC wrapper
  if (!quic_wrapper_->Initialize()) {
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to initialize QUIC wrapper";
    return false;
  }
  
  return true;
}

void QuicConnection::ConnectQuicWrapperSignals(QuicLibraryWrapper* wrapper) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  wrapper->SignalReadPacket.connect(this, &QuicConnection::OnQuicReadPacket);
  wrapper->SignalReadyToSend.connect(this, &QuicConnection::OnQuicReadyToSend);
  wrapper->SignalConnectionEstablished.connect(
      this, &QuicConnection::OnQuicConnectionEstablished);
  wrapper->SignalConnectionClosed.connect(
      this, &QuicConnection::OnQuicConnectionClosed);
}

void QuicConnection::DisconnectQuicWrapperSignals(QuicLibraryWrapper* wrapper) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  wrapper->SignalReadPacket.disconnect(this);
  wrapper->SignalReadyToSend.disconnect(this);
  wrapper->SignalConnectionEstablished.disconnect(this);
  wrapper->SignalConnectionClosed.disconnect(this);
}

void QuicConnection::ConnectSocketSignals(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!socket)
    return;

  socket->SignalReadPacket.connect(this, &QuicConnection::OnReadPacket);
  socket->SignalReadyToSend.connect(this, &QuicConnection::OnReadyToSend);
  socket->SignalClose.connect(this, &QuicConnection::OnClose);
}

void QuicConnection::DisconnectSocketSignals(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!socket)
    return;

  socket->SignalReadPacket.disconnect(this);
  socket->SignalReadyToSend.disconnect(this);
  socket->SignalClose.disconnect(this);
}

void QuicConnection::OnConnect(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK_RUN_ON(network_thread_);
  // This is called when the QUIC handshake is complete
  // In a real implementation, this would be triggered by the QUIC library
  connection_pending_ = false;
}

void QuicConnection::OnClose(rtc::AsyncPacketSocket* socket, int error) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << ToString() << ": Connection closed with error " << error;

  // When the socket is closed, update our state accordingly. Since we're no
  // longer connected, we can't send/receive anymore, so we're not writable or
  // readable.
  set_connected(false);
  set_write_state(STATE_WRITE_TIMEOUT);
  set_state(IceCandidatePairState::FAILED);
}

void QuicConnection::OnQuicReadPacket(QuicLibraryWrapper* wrapper,
                                     const char* data,
                                     size_t size) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  // Process data received from the QUIC wrapper
  // This is application data that has been decrypted by the QUIC stack
  RTC_LOG(LS_INFO) << ToString() << ": Received decrypted QUIC data, size=" << size;
  
  // Signal the data to the upper layers
  SignalReadPacket(this, data, size, rtc::SocketAddress());
}

void QuicConnection::OnQuicReadyToSend(QuicLibraryWrapper* wrapper) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  // Signal that the connection is ready to send data
  SignalReadyToSend(this);
}

void QuicConnection::OnQuicConnectionEstablished(
    QuicLibraryWrapper* wrapper,
    const rtc::SocketAddress& remote_addr) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  RTC_LOG(LS_INFO) << ToString() << ": QUIC connection established to "
                  << remote_addr.ToSensitiveString();
  
  // Set the connection state to writable
  set_write_state(Connection::STATE_WRITABLE);
  
  // Signal that the connection is ready to send data
  SignalReadyToSend(this);
}

void QuicConnection::OnQuicConnectionClosed(
    QuicLibraryWrapper* wrapper,
    int error) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  RTC_LOG(LS_INFO) << ToString() << ": QUIC connection closed, error=" << error;
  
  // Set the connection state to not writable
  set_write_state(Connection::STATE_WRITE_UNRELIABLE);
  
  // Signal that the connection is closed
  SignalClose(this, error);
}

void QuicConnection::OnReadPacket(rtc::AsyncPacketSocket* socket,
                                 const char* data,
                                 size_t size,
                                 const rtc::SocketAddress& remote_addr,
                                 const int64_t& packet_time_us) {
  RTC_DCHECK_RUN_ON(network_thread_);
  
  // Update last received time
  last_received_ = rtc::TimeMillis();
  
  // First, check if this is a STUN packet.
  if (size >= kStunHeaderSize && IsStunMessage(data, size)) {
    // This is a STUN packet, process it.
    if (HandleStunPacket(socket, data, size, remote_addr, packet_time_us)) {
      // The packet was handled as a STUN packet.
      return;
    }
  } else {
    // This is not a STUN packet, it's a QUIC packet
    // Mark the connection as receiving
    UpdateReceiving(true);
    
    // Pass the packet to the QUIC wrapper if available
    if (quic_wrapper_) {
      quic_wrapper_->ProcessPacket(data, size, remote_addr);
      RTC_LOG(LS_VERBOSE) << ToString() << ": Passed QUIC packet from "
                         << remote_addr.ToSensitiveString() 
                         << " (" << size << " bytes) to QUIC wrapper";
    } else {
      RTC_LOG(LS_VERBOSE) << ToString() << ": Received QUIC packet from "
                         << remote_addr.ToSensitiveString() 
                         << " (" << size << " bytes) but no QUIC wrapper available";
      // Let the base Connection handle it for now
      Connection::OnReadPacket(data, size, packet_time_us);
    }
    return;
  }

  // The packet wasn't handled as a STUN packet.
  RTC_LOG(LS_ERROR) << ToString()
                    << ": Received non-STUN packet from unknown address: "
                    << remote_addr.ToSensitiveString();
}

void QuicConnection::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK_RUN_ON(network_thread_);
  Connection::OnReadyToSend();
}

void QuicConnection::OnDestroyed(Connection* c) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(c == this);
  RTC_LOG(LS_VERBOSE) << ToString() << ": Connection destroyed";
}

}  // namespace cricket