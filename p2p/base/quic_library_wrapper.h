/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_QUIC_LIBRARY_WRAPPER_H_
#define P2P_BASE_QUIC_LIBRARY_WRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace cricket {

// Forward declarations for QUIC library types
// In a real implementation, these would be replaced with actual QUIC types
class QuicConnection;
class QuicSession;
class QuicStream;
class QuicConfig;
class QuicCryptoClientConfig;
class QuicCryptoServerConfig;

// QuicLibraryWrapper provides a simplified interface to the QUIC library.
// It handles the details of creating and managing QUIC connections and streams.
class QuicLibraryWrapper : public sigslot::has_slots<> {
 public:
  // Signals
  sigslot::signal5<QuicLibraryWrapper*,
                   const char*,
                   size_t,
                   const rtc::SocketAddress&,
                   const int64_t&>
      SignalReadPacket;
  sigslot::signal1<QuicLibraryWrapper*> SignalReadyToSend;
  sigslot::signal2<QuicLibraryWrapper*, const rtc::SocketAddress&>
      SignalConnectionEstablished;
  sigslot::signal2<QuicLibraryWrapper*, int> SignalConnectionClosed;

  // Create a client QUIC wrapper
  static std::unique_ptr<QuicLibraryWrapper> CreateClientWrapper(
      rtc::AsyncPacketSocket* socket,
      const rtc::SocketAddress& server_address);

  // Create a server QUIC wrapper
  static std::unique_ptr<QuicLibraryWrapper> CreateServerWrapper(
      rtc::AsyncPacketSocket* socket);

  virtual ~QuicLibraryWrapper() {}

  // Initialize the QUIC library
  virtual bool Initialize() = 0;

  // Connect to a QUIC server
  virtual bool Connect() = 0;

  // Accept a connection from a QUIC client
  virtual bool Accept() = 0;

  // Send data over the QUIC connection
  virtual int Send(const void* data,
                   size_t size,
                   const rtc::PacketOptions& options) = 0;

  // Close the QUIC connection
  virtual void Close() = 0;

  // Check if the connection is established
  virtual bool IsConnected() const = 0;

  // Get the remote address
  virtual rtc::SocketAddress GetRemoteAddress() const = 0;

  // Get the local address
  virtual rtc::SocketAddress GetLocalAddress() const = 0;

  // Get the last error
  virtual int GetError() const = 0;
};

// Implementation of QuicLibraryWrapper that uses Google's QUIC implementation
class GoogleQuicLibraryWrapper : public QuicLibraryWrapper {
 public:
  GoogleQuicLibraryWrapper(rtc::AsyncPacketSocket* socket,
                          const rtc::SocketAddress& server_address,
                          bool is_server);
  ~GoogleQuicLibraryWrapper() override;

  // QuicLibraryWrapper implementation
  bool Initialize() override;
  bool Connect() override;
  bool Accept() override;
  int Send(const void* data,
           size_t size,
           const rtc::PacketOptions& options) override;
  void Close() override;
  bool IsConnected() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  rtc::SocketAddress GetLocalAddress() const override;
  int GetError() const override;

 private:
  // Called when a packet is received on the socket
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const int64_t& packet_time_us);

  // Called when the socket is ready to send
  void OnReadyToSend(rtc::AsyncPacketSocket* socket);

  // Called when the socket is closed
  void OnClose(rtc::AsyncPacketSocket* socket, int error);

  // The underlying UDP socket used for QUIC
  rtc::AsyncPacketSocket* socket_;

  // The remote address (for client mode)
  rtc::SocketAddress server_address_;

  // Whether this is a server or client
  bool is_server_;

  // QUIC-specific objects
  // In a real implementation, these would be actual QUIC objects
  std::unique_ptr<QuicConnection> quic_connection_;
  std::unique_ptr<QuicSession> quic_session_;
  std::unique_ptr<QuicConfig> quic_config_;
  std::unique_ptr<QuicCryptoClientConfig> crypto_client_config_;
  std::unique_ptr<QuicCryptoServerConfig> crypto_server_config_;

  // Connection state
  bool is_connected_;
  int error_;
};

}  // namespace cricket

#endif  // P2P_BASE_QUIC_LIBRARY_WRAPPER_H_