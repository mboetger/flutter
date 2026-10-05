import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

import '../services/fake_platform_views.dart';

void main() {
  testWidgets('Platform view cancels pointers when moved to another parent', (
    WidgetTester tester,
  ) async {
    final FakeAndroidPlatformViewsController viewsController = FakeAndroidPlatformViewsController();
    viewsController.registerViewType('webview');

    final GlobalKey platformViewKey = GlobalKey();

    Widget buildApp(bool useParentA) {
      final AndroidView platformView = AndroidView(
        key: platformViewKey,
        viewType: 'webview',
        layoutDirection: TextDirection.ltr,
        gestureRecognizers: <Factory<OneSequenceGestureRecognizer>>{
          Factory<EagerGestureRecognizer>(() => EagerGestureRecognizer()),
        },
      );

      return MaterialApp(
        home: Scaffold(
          body: useParentA
              ? Column(children: <Widget>[SizedBox(width: 100, height: 100, child: platformView)])
              : Row(children: <Widget>[SizedBox(width: 100, height: 100, child: platformView)]),
        ),
      );
    }

    await tester.pumpWidget(buildApp(true));

    final int viewId = platformViewsRegistry.getNextPlatformViewId() - 1;

    // 1. Start pressing on a platform view (keep finger down).
    final TestGesture gesture1 = await tester.startGesture(const Offset(50.0, 50.0));
    await tester.pump();

    // 2. The platform view is moved to another parent
    await tester.pumpWidget(buildApp(false));
    await tester.pump();

    // 3. Lift the finger
    await gesture1.up();
    await tester.pump();

    // 4. Tap the native view once
    final TestGesture gesture2 = await tester.startGesture(const Offset(50.0, 50.0));
    await tester.pump();
    await gesture2.up();
    await tester.pump();

    final List<FakeAndroidMotionEvent> events = viewsController.motionEvents[viewId]!;

    expect(events.length, 4);

    // Expected events:
    // 1. ACTION_DOWN
    expect(events[0].action, 0); // ACTION_DOWN
    // 2. ACTION_CANCEL (synthesized when detached)
    expect(events[1].action, 3); // ACTION_CANCEL
    // 3. ACTION_DOWN (second gesture)
    expect(events[2].action, 0); // ACTION_DOWN
    // 4. ACTION_UP (second gesture ends)
    expect(events[3].action, 1); // ACTION_UP
  });
}
