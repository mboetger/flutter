// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('runApp schedules root widget and first frame which triggers splash screen removal', (
    WidgetTester tester,
  ) async {
    final log = <String>[];
    final WidgetsBinding binding = WidgetsFlutterBinding.ensureInitialized();

    // Verify normal runApp startup behavior.
    runApp(
      Builder(
        builder: (BuildContext context) {
          log.add('root built');
          return const Center(child: Text('Root Widget', textDirection: TextDirection.ltr));
        },
      ),
    );

    expect(binding.isRootWidgetAttached, isTrue);
    await tester.pump();
    expect(log, equals(<String>['root built']));
    expect(find.text('Root Widget'), findsOneWidget);
  });

  testWidgets('deferFirstFrame allows preparing root widget before splash screen is dismissed', (
    WidgetTester tester,
  ) async {
    expect(RendererBinding.instance.sendFramesToEngine, isTrue);

    final completer = Completer<void>();
    await tester.pumpWidget(
      Directionality(
        textDirection: TextDirection.ltr,
        child: _DeferringWidget(key: UniqueKey(), loader: completer.future),
      ),
    );

    final _DeferringWidgetState state = tester.state<_DeferringWidgetState>(
      find.byType(_DeferringWidget),
    );

    // While deferred, frames are not sent to the engine, so native splash screen stays visible.
    expect(find.text('Loading...'), findsOneWidget);
    expect(find.text('Logo'), findsNothing);
    expect(RendererBinding.instance.sendFramesToEngine, isFalse);

    await tester.pump();
    expect(RendererBinding.instance.sendFramesToEngine, isFalse);
    expect(state.doneLoading, isFalse);

    // Once ready, allowing the first frame causes the engine to render and notify native platform.
    completer.complete();
    await tester.idle();
    expect(state.doneLoading, isTrue);
    expect(RendererBinding.instance.sendFramesToEngine, isTrue);

    await tester.pump();
    expect(find.text('Loading...'), findsNothing);
    expect(find.text('Logo'), findsOneWidget);
  });

  testWidgets(
    'SystemChrome.setSplashScreenFadeEnabled sends platform message to configure splash screen fade behavior (flutter/flutter#63156)',
    (WidgetTester tester) async {
      // In issue flutter/flutter#63156, developers requested an option to disable the default fade-out
      // animation when the native splash screen transitions to the Flutter root widget.
      //
      // Developers can call SystemChrome.setSplashScreenFadeEnabled(false) prior to or during runApp
      // to instruct the native embedder (e.g. iOS FlutterViewController / SplashScreenManager) to
      // dismiss the splash screen synchronously without fading.
      //
      // This test verifies that calling SystemChrome.setSplashScreenFadeEnabled sends the expected
      // system channel message on SystemChannels.platform.

      final platformCalls = <MethodCall>[];
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        (MethodCall methodCall) async {
          platformCalls.add(methodCall);
          return null;
        },
      );

      await SystemChrome.setSplashScreenFadeEnabled(false);
      runApp(const Center(child: Text('Seamless Root', textDirection: TextDirection.ltr)));
      await tester.pump();

      // Verify that platform message was sent to instruct the native embedder
      // to disable splash screen fading.
      final bool hasSplashFadeCommand = platformCalls.any(
        (MethodCall call) =>
            call.method == 'SystemChrome.setSplashScreenFadeEnabled' && call.arguments == false,
      );
      expect(
        hasSplashFadeCommand,
        isTrue,
        reason: 'Platform message should be sent to disable splash screen fading.',
      );

      // Cleanup mock handler.
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        null,
      );
    },
  );
}

class _DeferringWidget extends StatefulWidget {
  const _DeferringWidget({required super.key, required this.loader});

  final Future<void> loader;

  @override
  State<_DeferringWidget> createState() => _DeferringWidgetState();
}

class _DeferringWidgetState extends State<_DeferringWidget> {
  bool doneLoading = false;

  @override
  void initState() {
    super.initState();
    RendererBinding.instance.deferFirstFrame();
    widget.loader.then((_) {
      if (mounted) {
        setState(() {
          doneLoading = true;
          RendererBinding.instance.allowFirstFrame();
        });
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return doneLoading ? const Text('Logo') : const Text('Loading...');
  }
}
