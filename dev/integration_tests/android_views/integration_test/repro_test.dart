import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

Map<String, dynamic> createEvent({
  required int action,
  required int pointerCount,
  required List<int> ids,
  int downTime = 100,
  required int eventTime,
}) {
  return <String, dynamic>{
    'downTime': downTime,
    'eventTime': eventTime,
    'action': action,
    'pointerCount': pointerCount,
    'pointerProperties': ids.map((id) => <String, dynamic>{
      'id': id,
      'toolType': 1,
    }).toList(),
    'pointerCoords': ids.map((id) => <String, dynamic>{
      'orientation': 0.0,
      'pressure': 1.0,
      'size': 1.0,
      'toolMajor': 1.0,
      'toolMinor': 1.0,
      'touchMajor': 1.0,
      'touchMinor': 1.0,
      'x': 100.0 * id,
      'y': 100.0 * id,
    }).toList(),
    'metaState': 0,
    'buttonState': 0,
    'xPrecision': 1.0,
    'yPrecision': 1.0,
    'deviceId': 0,
    'edgeFlags': 0,
    'source': 4098,
    'flags': 0,
  };
}

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('MotionEvent pointer ID disagreement bug', (WidgetTester tester) async {
    final methodChannel = const MethodChannel('android_views_integration');
    MethodChannel? viewChannel;
    List<Map<String, dynamic>> embeddedViewEvents = [];
    
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: SizedBox(
            height: 300,
            child: AndroidView(
              viewType: 'simple_view',
              onPlatformViewCreated: (id) {
                viewChannel = MethodChannel('simple_view/$id');
                viewChannel!.setMethodCallHandler((call) async {
                  if (call.method == 'onTouch') {
                    embeddedViewEvents.insert(0, (call.arguments as Map).cast<String, dynamic>());
                  }
                });
              },
            ),
          ),
        ),
      ),
    );
    await tester.pump(const Duration(milliseconds: 500));
    
    await viewChannel?.invokeMethod<void>('pipeTouchEvents');
    
    final recordedEvents = [
      createEvent(action: 0, pointerCount: 1, ids: [0], eventTime: 100),
      createEvent(action: (1 << 8) | 5, pointerCount: 2, ids: [0, 1], eventTime: 200),
      createEvent(action: (1 << 8) | 6, pointerCount: 2, ids: [0, 1], eventTime: 300),
      createEvent(action: (1 << 8) | 5, pointerCount: 2, ids: [0, 2], eventTime: 300),
      createEvent(action: 2, pointerCount: 2, ids: [0, 2], eventTime: 400),
      createEvent(action: (1 << 8) | 5, pointerCount: 3, ids: [0, 1, 2], eventTime: 500),
    ];
    
    for (final event in recordedEvents) {
      await methodChannel.invokeMethod<void>('synthesizeEvent', event);
    }
    
    await Future<void>.delayed(const Duration(milliseconds: 500));
    await tester.pump(const Duration(milliseconds: 500));
    
    expect(embeddedViewEvents.length, greaterThanOrEqualTo(6), reason: 'Not enough events received');
    
    final lastEvent = embeddedViewEvents[0];
    final prevEvent = embeddedViewEvents[1];
    
    final lastProps = lastEvent['pointerProperties'] as List<dynamic>;
    final prevProps = prevEvent['pointerProperties'] as List<dynamic>;
    
    final prevIds = prevProps.map((p) => p['id']).toSet();
    final newPointerIndex = (lastEvent['action'] as int) >> 8;
    final newId = lastProps[newPointerIndex]['id'];
    
    // The bug happens if the new ID was already in prevIds!
    expect(
      prevIds.contains(newId),
      isFalse,
      reason: 'BUG REPRODUCED: POINTER_DOWN for id $newId, which is already down! prevIds=$prevIds',
    );
    
    await viewChannel?.invokeMethod<void>('stopTouchEvents');
  });
}
