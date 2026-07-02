// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/platform/android/message_loop_android.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "flutter/fml/platform/linux/timerfd.h"

namespace fml {

static constexpr int kClockType = CLOCK_MONOTONIC;

static ALooper* AcquireLooperForThread() {
  ALooper* looper = ALooper_forThread();

  if (looper == nullptr) {
    // No looper has been configured for the current thread. Create one and
    // return the same.
    looper = ALooper_prepare(0);
  }

  // The thread already has a looper. Acquire a reference to the same and return
  // it.
  ALooper_acquire(looper);
  return looper;
}

MessageLoopAndroid::MessageLoopAndroid()
    : looper_(AcquireLooperForThread()),
      timer_fd_(::timerfd_create(kClockType, TFD_NONBLOCK | TFD_CLOEXEC)),
      wake_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
  FML_CHECK(looper_.is_valid());
  FML_CHECK(timer_fd_.is_valid());
  FML_CHECK(wake_fd_.is_valid());

  static const int kWakeEvents = ALOOPER_EVENT_INPUT;

  ALooper_callbackFunc read_event_fd = [](int, int events, void* data) -> int {
    if (events & kWakeEvents) {
      reinterpret_cast<MessageLoopAndroid*>(data)->OnEventFired();
    }
    return 1;  // continue receiving callbacks
  };

  int add_result = ::ALooper_addFd(looper_.get(),          // looper
                                   timer_fd_.get(),        // fd
                                   ALOOPER_POLL_CALLBACK,  // ident
                                   kWakeEvents,            // events
                                   read_event_fd,          // callback
                                   this                    // baton
  );
  FML_CHECK(add_result == 1);

  ALooper_callbackFunc read_wake_fd = [](int, int events, void* data) -> int {
    if (events & kWakeEvents) {
      reinterpret_cast<MessageLoopAndroid*>(data)->OnWakeFired();
    }
    return 1;  // continue receiving callbacks
  };

  add_result = ::ALooper_addFd(looper_.get(),          // looper
                               wake_fd_.get(),         // fd
                               ALOOPER_POLL_CALLBACK,  // ident
                               kWakeEvents,            // events
                               read_wake_fd,           // callback
                               this                    // baton
  );
  FML_CHECK(add_result == 1);
}

MessageLoopAndroid::~MessageLoopAndroid() {
  int remove_result = ::ALooper_removeFd(looper_.get(), timer_fd_.get());
  FML_CHECK(remove_result == 1);
  remove_result = ::ALooper_removeFd(looper_.get(), wake_fd_.get());
  FML_CHECK(remove_result == 1);
}

void MessageLoopAndroid::Run() {
  FML_DCHECK(looper_.get() == ALooper_forThread());

  running_ = true;

  while (running_) {
    int result = ::ALooper_pollOnce(-1,       // infinite timeout
                                    nullptr,  // out fd,
                                    nullptr,  // out events,
                                    nullptr   // out data
    );
    if (result == ALOOPER_POLL_TIMEOUT || result == ALOOPER_POLL_ERROR) {
      // This handles the case where the loop is terminated using ALooper APIs.
      running_ = false;
    }
  }
}

void MessageLoopAndroid::Terminate() {
  running_ = false;
  ALooper_wake(looper_.get());
}

void MessageLoopAndroid::WakeUp(fml::TimePoint time_point) {
  if (time_point <= fml::TimePoint::Now()) {
    uint64_t value = 1;
    [[maybe_unused]] ssize_t result =
        ::write(wake_fd_.get(), &value, sizeof(value));
    FML_DCHECK(result == sizeof(value));
  } else {
    [[maybe_unused]] bool result = TimerRearm(timer_fd_.get(), time_point);
    FML_DCHECK(result);
  }
}

void MessageLoopAndroid::OnEventFired() {
  if (TimerDrain(timer_fd_.get())) {
    RunExpiredTasksNow();
  }
}

void MessageLoopAndroid::OnWakeFired() {
  uint64_t value = 0;
  [[maybe_unused]] ssize_t result =
      ::read(wake_fd_.get(), &value, sizeof(value));
  FML_DCHECK(result == sizeof(value));
  RunExpiredTasksNow();
}

}  // namespace fml
