// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

class WindowFocusObserver with WidgetsBindingObserver {
  List<AppLifecycleState> lifecycleStates = <AppLifecycleState>[];
  List<ViewFocusEvent> focusEvents = <ViewFocusEvent>[];

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    lifecycleStates.add(state);
  }

  @override
  void didChangeViewFocus(ViewFocusEvent event) {
    focusEvents.add(event);
  }
}

void main() {
  Future<void> setAppLifeCycleState(AppLifecycleState state) async {
    final ByteData? message = const StringCodec().encodeMessage(state.toString());
    await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.handlePlatformMessage(
      'flutter/lifecycle',
      message,
      (_) {},
    );
  }

  testWidgets('Android window focus change triggers both app lifecycle and view focus', (
    WidgetTester tester,
  ) async {
    final observer = WindowFocusObserver();
    WidgetsBinding.instance.addObserver(observer);
    addTearDown(() => WidgetsBinding.instance.removeObserver(observer));

    // Ensure we start with a null lifecycle state in the test environment.
    expect(WidgetsBinding.instance.lifecycleState, isNull);

    // Transition to resumed state (simulating normal app startup).
    await setAppLifeCycleState(AppLifecycleState.resumed);
    expect(WidgetsBinding.instance.lifecycleState, AppLifecycleState.resumed);
    expect(observer.lifecycleStates, contains(AppLifecycleState.resumed));

    // Simulate Android losing window focus (e.g., status bar pulled down).
    // The Android embedding sends "AppLifecycleState.inactive" over the lifecycle channel
    // and dispatches a view focus event.
    await setAppLifeCycleState(AppLifecycleState.inactive);
    tester.binding.platformDispatcher.onViewFocusChange?.call(
      const ViewFocusEvent(
        viewId: 0,
        state: ViewFocusState.unfocused,
        direction: ViewFocusDirection.undefined,
      ),
    );

    // Verify that we received the lifecycle state transition.
    expect(WidgetsBinding.instance.lifecycleState, AppLifecycleState.inactive);
    expect(observer.lifecycleStates, contains(AppLifecycleState.inactive));

    // Verify that didChangeViewFocus was called.
    expect(observer.focusEvents, hasLength(1));
    expect(observer.focusEvents.first.state, ViewFocusState.unfocused);
    expect(observer.focusEvents.first.viewId, 0);
  });
}
