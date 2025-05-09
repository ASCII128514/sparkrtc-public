/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_PEERCONNECTION_LOCALVIDEO_MAIN_WND_H_
#define EXAMPLES_PEERCONNECTION_LOCALVIDEO_MAIN_WND_H_

#include <map>
#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "examples/peerconnection/localvideo/peer_connection_localvideo.h"
#include "media/base/media_channel.h"
#include "media/base/video_common.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif  // WEBRTC_WIN

class MainWndCallback {
 public:
  virtual void StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer() = 0;
  virtual void UIThreadCallback(int msg_id, void* data) = 0;
  virtual void Close() = 0;

 protected:
  virtual ~MainWndCallback() {}
};

// Pure virtual interface for the main window.
class MainWindow {
 public:
  virtual ~MainWindow() {}

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
  };

  virtual void RegisterObserver(MainWndCallback* callback) = 0;

  virtual bool IsWindow() = 0;
  virtual void MessageBox(const char* caption,
                          const char* text,
                          bool is_error) = 0;

  virtual UI current_ui() = 0;

  virtual void SwitchToConnectUI() = 0;
  virtual void SwitchToPeerList(const Peers& peers) = 0;
  virtual void SwitchToStreamingUI() = 0;

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) = 0;
  virtual void StopLocalRenderer() = 0;
  virtual void StartRemoteRenderer(
      webrtc::VideoTrackInterface* remote_video) = 0;
  virtual void StopRemoteRenderer() = 0;

  virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
};

#ifdef WIN32

class MainWnd : public MainWindow {
 public:
  static const wchar_t kClassName[];

  enum WindowMessages {
    UI_THREAD_CALLBACK = WM_APP + 1,
  };

  MainWnd(const char* server, int port, bool auto_call);
  ~MainWnd();

  bool Create();
  bool Destroy();
  bool PreTranslateMessage(MSG* msg);
  void AutoConnect();

  virtual void RegisterObserver(MainWndCallback* callback);
  virtual bool IsWindow();
  virtual void SwitchToConnectUI();
  virtual void SwitchToPeerList(const Peers& peers);
  virtual void SwitchToStreamingUI();
  virtual void MessageBox(const char* caption, const char* text, bool is_error);
  virtual UI current_ui() { return ui_; }

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  virtual void StopLocalRenderer();
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  virtual void StopRemoteRenderer();

  virtual void QueueUIThreadCallback(int msg_id, void* data);

  HWND handle() const { return wnd_; }

  class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    VideoRenderer(HWND wnd,
                  int width,
                  int height,
                  webrtc::VideoTrackInterface* track_to_render);
    virtual ~VideoRenderer();

    // VideoSinkInterface implementation
    void OnFrame(const webrtc::VideoFrame& frame) override;

    const uint8_t* image() const { return image_.get(); }

   protected:
    void SetSize(int width, int height);

    HWND wnd_;
    std::unique_ptr<uint8_t[]> image_;
    int width_;
    int height_;
    FILE* file_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
  };

 protected:

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static bool RegisterWindowClass();

 private:
  std::unique_ptr<VideoRenderer> local_renderer_;
  std::unique_ptr<VideoRenderer> remote_renderer_;
  UI ui_;
  HWND wnd_;
  DWORD ui_thread_id_;
  MainWndCallback* callback_;
  static ATOM wnd_class_;
  std::string server_;
  std::string port_;
  bool auto_call_;
};

#endif  // WIN32

#endif  // EXAMPLES_PEERCONNECTION_LOCALVIDEO_MAIN_WND_H_
