// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';

import 'rendering_tester.dart';

class _HitTestableRenderProxyBox extends RenderProxyBox {
  _HitTestableRenderProxyBox(super.child);

  @override
  bool hitTestSelf(Offset position) => true;
}

class _TestHitTestResult extends HitTestResult {
  void testPushTransform(Matrix4 transform) {
    pushTransform(transform);
  }
}

void main() {
  TestRenderingFlutterBinding.ensureInitialized();

  test('reproduce issue 19524 without using Vector4 in test', () {
    // 1. Verify PointerEvent.removePerspectiveTransform does not use Vector4 or crash
    final transform = Matrix4.identity()
      ..setEntry(3, 2, 0.005)
      ..rotateX(-0.2)
      ..rotateY(0.2);
    final Matrix4 cleaned = PointerEvent.removePerspectiveTransform(transform);
    expect(cleaned.entry(2, 0), 0.0);
    expect(cleaned.entry(2, 1), 0.0);
    expect(cleaned.entry(2, 2), 1.0);
    expect(cleaned.entry(2, 3), 0.0);
    expect(cleaned.entry(0, 2), 0.0);
    expect(cleaned.entry(1, 2), 0.0);
    expect(cleaned.entry(3, 2), 0.0);

    // 2. Verify FollowerLayer's transformation does not crash.
    final link = LayerLink();
    final renderLeader = RenderLeaderLayer(
      link: link,
      child: RenderConstrainedBox(additionalConstraints: BoxConstraints.tight(const Size(10, 10))),
    );
    final renderFollower = RenderFollowerLayer(
      link: link,
      offset: const Offset(5, 5),
      child: _HitTestableRenderProxyBox(
        RenderConstrainedBox(additionalConstraints: BoxConstraints.tight(const Size(10, 10))),
      ),
    );
    final stack = RenderStack(
      textDirection: TextDirection.ltr,
      children: <RenderBox>[renderLeader, renderFollower],
    );

    // layout() layouts, paints, and composites the tree.
    // We specify phase: EnginePhase.composite to ensure the paint and composite phases run,
    // which instantiates and attaches the LeaderLayer and FollowerLayer.
    layout(
      stack,
      constraints: BoxConstraints.tight(const Size(100, 100)),
      phase: EnginePhase.composite,
    );

    // To trigger FollowerLayer._transformOffset, we can perform a hit test on the follower.
    // Hit testing on RenderFollowerLayer calls FollowerLayer.findAnnotations under the hood,
    // which calls _transformOffset.
    final hitTestResult = BoxHitTestResult();
    final bool hit = renderFollower.hitTest(hitTestResult, position: const Offset(5, 5));
    expect(hit, isTrue);

    // 3. Verify HitTestResult.pushTransform does not crash (asserts are active in debug/test mode)
    final gestureHitTestResult = _TestHitTestResult();
    // Verify that it throws AssertionError for invalid transform (with perspective)
    expect(() => gestureHitTestResult.testPushTransform(transform), throwsAssertionError);
    // This calls pushTransform under the hood, which asserts that the 3rd row and 3rd col are valid.
    gestureHitTestResult.testPushTransform(cleaned);
  });
}
