import 'package:flutter/gestures.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('multi-touch PointerCancelEvent does not cancel entire gesture (reproduces 182828)', (
    WidgetTester tester,
  ) async {
    final List<MethodCall> log = <MethodCall>[];
    tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
      SystemChannels.platform_views,
      (MethodCall methodCall) async {
        log.add(methodCall);
        return null;
      },
    );

    final AndroidViewController viewController = PlatformViewsService.initSurfaceAndroidView(
      id: 7,
      viewType: 'web',
      layoutDirection: TextDirection.ltr,
    );
    viewController.pointTransformer = (Offset offset) => offset;

    await viewController.dispatchPointerEvent(const PointerDownEvent(pointer: 1));
    await viewController.dispatchPointerEvent(const PointerDownEvent(pointer: 2));

    log.clear();

    await viewController.dispatchPointerEvent(const PointerCancelEvent(pointer: 2));

    expect(log, hasLength(1));
    MethodCall call = log.single;
    List<dynamic> args = call.arguments as List<dynamic>;
    
    const int kAndroidMotionEventListIndexAction = 3;
    int action = args[kAndroidMotionEventListIndexAction] as int;

    final int expectedAction = AndroidViewController.pointerAction(
      1,
      AndroidViewController.kActionPointerUp,
    );
    
    expect(action, equals(expectedAction), reason: 'Should send ACTION_POINTER_UP for the single cancelled pointer to avoid sticking the remaining pointer');

    log.clear();
    await viewController.dispatchPointerEvent(const PointerCancelEvent(pointer: 1));
    
    expect(log, hasLength(1));
    call = log.single;
    args = call.arguments as List<dynamic>;
    action = args[kAndroidMotionEventListIndexAction] as int;
    expect(action, equals(AndroidViewController.kActionCancel), reason: 'Should send ACTION_CANCEL when the last pointer is cancelled');

    await viewController.dispose();
  });
}
