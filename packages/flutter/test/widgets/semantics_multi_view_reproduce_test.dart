// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';

import 'multi_view_testing.dart';

void main() {
  testWidgets('Moving/reactivating a view clears and rebuilds semantics correctly', (
    WidgetTester tester,
  ) async {
    final SemanticsHandle handle = tester.ensureSemantics();

    final GlobalKey viewKey = GlobalKey();
    final fakeView = RecordingFakeView(tester.view);

    Widget buildTree(bool move) {
      final view = View(
        key: viewKey,
        view: fakeView,
        child: Semantics(
          label: 'Target Semantics',
          container: true,
          child: const Text('Secondary View'),
        ),
      );

      return Directionality(
        textDirection: TextDirection.ltr,
        child: ViewCollection(
          views: <Widget>[
            if (move) ...<Widget>[
              KeyedSubtree(key: UniqueKey(), child: view),
              View(view: tester.view, child: const Text('Main View')),
            ] else ...<Widget>[View(view: tester.view, child: const Text('Main View')), view],
          ],
        ),
      );
    }

    // 1. Pump both views.
    await tester.pumpWidget(wrapWithView: false, buildTree(false));
    await tester.pumpAndSettle();

    // Verify semantics owner was created and semantics update was sent to fakeView.
    final RenderView renderView2 = tester.renderObject(
      find.descendant(of: find.byKey(viewKey), matching: find.byType(RawView)),
    );
    final PipelineOwner owner2 = renderView2.owner!;
    expect(owner2.semanticsOwner, isNotNull);
    expect(fakeView.updateSemanticsCount, greaterThan(0));

    // Clear updates on the recording view.
    fakeView.resetRecordedUpdates();

    // 2. Move the view within the tree (this triggers deactivate and activate in the same frame).
    await tester.pumpWidget(wrapWithView: false, buildTree(true));
    await tester.pumpAndSettle();

    // The view was reactivated. Since it was deactivated and reactivated,
    // the platform-side semantics would be destroyed/recreated.
    // The framework should have sent a new semantics update to rebuild the semantics tree for this view.
    expect(fakeView.updateSemanticsCount, greaterThan(0));

    handle.dispose();
  });
}

class RecordingFakeView extends FakeView {
  RecordingFakeView(super.view, {super.viewId});

  int updateSemanticsCount = 0;

  void resetRecordedUpdates() {
    updateSemanticsCount = 0;
  }

  @override
  void updateSemantics(ui.SemanticsUpdate update) {
    updateSemanticsCount++;
    super.updateSemantics(update);
  }
}
