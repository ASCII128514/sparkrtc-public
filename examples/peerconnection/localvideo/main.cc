/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// clang-format off
// clang formating would change include order.
#include <windows.h>
#include <shellapi.h>  // must come after windows.h
// clang-format on

#include <string>
#include <vector>

#include "absl/flags/parse.h"
#include "examples/peerconnection/localvideo/conductor.h"
#include "examples/peerconnection/localvideo/flag_defs.h"
#include "examples/peerconnection/localvideo/main_wnd.h"
#include "examples/peerconnection/localvideo/peer_connection_localvideo.h"
#include "rtc_base/checks.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/event_tracer.h"
#include "rtc_base/string_utils.h"  // For ToUtf8
#include "rtc_base/win32_socket_init.h"
#include "rtc_base/logging.h"
#include "rtc_base/log_sinks.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include <iostream>
#include <thread>
#include <chrono>


std::string local_video_filename;
std::string recon_filename;
int local_video_width;
int local_video_height;
int local_video_fps;
bool is_sender = false;
bool is_GUI = false;

namespace {
// A helper class to translate Windows command line arguments into UTF8,
// which then allows us to just pass them to the flags system.
// This encapsulates all the work of getting the command line and translating
// it to an array of 8-bit strings; all you have to do is create one of these,
// and then call argc() and argv().
class WindowsCommandLineArguments {
 public:
  WindowsCommandLineArguments();

  WindowsCommandLineArguments(const WindowsCommandLineArguments&) = delete;
  WindowsCommandLineArguments& operator=(WindowsCommandLineArguments&) = delete;

  int argc() { return argv_.size(); }
  char** argv() { return argv_.data(); }

 private:
  // Owned argument strings.
  std::vector<std::string> args_;
  // Pointers, to get layout compatible with char** argv.
  std::vector<char*> argv_;
};

WindowsCommandLineArguments::WindowsCommandLineArguments() {
  // start by getting the command line.
  LPCWSTR command_line = ::GetCommandLineW();
  // now, convert it to a list of wide char strings.
  int argc;
  LPWSTR* wide_argv = ::CommandLineToArgvW(command_line, &argc);

  // iterate over the returned wide strings;
  for (int i = 0; i < argc; ++i) {
    args_.push_back(rtc::ToUtf8(wide_argv[i], wcslen(wide_argv[i])));
    // make sure the argv array points to the string data.
    argv_.push_back(const_cast<char*>(args_.back().c_str()));
  }
  LocalFree(wide_argv);
}

class CustomSocketServer : public rtc::PhysicalSocketServer {
 public:
  bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) override {
    if (!process_io)
      return true;
    return rtc::PhysicalSocketServer::Wait(webrtc::TimeDelta::Zero(),
                                           process_io);
  }
};

}  // namespace



int PASCAL wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* cmd_line,
                    int cmd_show) {
  webrtc::metrics::Enable();
  rtc::WinsockInitializer winsock_init;
  CustomSocketServer ss;
  rtc::AutoSocketServerThread main_thread(&ss);

  WindowsCommandLineArguments win_args;
  int argc = win_args.argc();
  char** argv = win_args.argv();
  absl::ParseCommandLine(argc, argv);
  rtc::tracing::SetupInternalTracer();
  
  local_video_filename = absl::GetFlag(FLAGS_file);
  recon_filename = absl::GetFlag(FLAGS_recon);
  local_video_width = absl::GetFlag(FLAGS_width);
  local_video_height = absl::GetFlag(FLAGS_height);
  local_video_fps = absl::GetFlag(FLAGS_fps);
  is_GUI = absl::GetFlag(FLAGS_gui);


  // If the ./logs directory doesn't exist, create it.
  CreateDirectoryA("./logs", NULL);

  static const std::string  event_log_file_name = "./logs/rtc_event_" + std::to_string(::time(NULL))+ ".json";
  rtc::tracing::StartInternalCapture(event_log_file_name.c_str());

  rtc::LogMessage::LogTimestamps(true);
  rtc::LogMessage::LogThreads(true);

  rtc::FileRotatingLogSink frls("./logs", "log_" + std::to_string(::time(NULL)), 10 << 20, 10);
  frls.Init();
  rtc::LogMessage::AddLogToStream(&frls, rtc::LS_VERBOSE);

  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  const std::string forced_field_trials =
      absl::GetFlag(FLAGS_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(forced_field_trials.c_str());

  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  if ((absl::GetFlag(FLAGS_port) < 1) || (absl::GetFlag(FLAGS_port) > 65535)) {
    printf("Error: %i is not a valid port.\n", absl::GetFlag(FLAGS_port));
    return -1;
  }

  if (is_GUI) {
    RTC_LOG(LS_ERROR) << "GUI is not supported on Windows.";
    return -1;
  }

  bool autocall = false;
  if (local_video_filename != "NONE") {
    is_sender = true;
    autocall = true;
  }

  const std::string server = absl::GetFlag(FLAGS_server);
  MainWnd wnd(server.c_str(), absl::GetFlag(FLAGS_port), autocall);
  if (!wnd.Create()) {
    RTC_DCHECK_NOTREACHED();
    return -1;
  }

  rtc::InitializeSSL();
  PeerConnectionClient client;
  auto conductor = rtc::make_ref_counted<Conductor>(&client, &wnd);

  main_thread.Start();
  wnd.AutoConnect();
  // Main loop.
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
    if (!wnd.PreTranslateMessage(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  if (conductor->connection_active() || client.is_connected()) {
    while ((conductor->connection_active() || client.is_connected()) &&
           (gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
      if (!wnd.PreTranslateMessage(&msg)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }
    }
  }

  rtc::CleanupSSL();
  return 0;
}
