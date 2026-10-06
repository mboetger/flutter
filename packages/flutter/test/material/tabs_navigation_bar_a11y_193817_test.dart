import 'package:flutter/material.dart';
import 'package:flutter/semantics.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('Selected TabBar tab does not have tap action to avoid double tap hint', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: DefaultTabController(
          length: 2,
          child: Scaffold(
            appBar: AppBar(
              bottom: const TabBar(
                tabs: <Widget>[
                  Tab(text: 'Tab A'),
                  Tab(text: 'Tab B'),
                ],
              ),
            ),
          ),
        ),
      ),
    );

    final SemanticsNode tabA = tester.getSemantics(find.text('Tab A'));
    final SemanticsData data = tabA.getSemanticsData();
    expect(
      data.hasAction(SemanticsAction.tap),
      isFalse,
      reason: 'Selected tab should not have a tap action',
    );

    final SemanticsNode tabB = tester.getSemantics(find.text('Tab B'));
    final SemanticsData dataB = tabB.getSemanticsData();
    expect(
      dataB.hasAction(SemanticsAction.tap),
      isTrue,
      reason: 'Unselected tab should have a tap action',
    );
  });

  testWidgets(
    'Selected NavigationBar destination does not have tap action to avoid double tap hint',
    (WidgetTester tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            bottomNavigationBar: NavigationBar(
              destinations: const <Widget>[
                NavigationDestination(icon: Icon(Icons.home), label: 'Home'),
                NavigationDestination(icon: Icon(Icons.settings), label: 'Settings'),
              ],
            ),
          ),
        ),
      );

      final SemanticsNode home = tester.getSemantics(find.text('Home'));
      final SemanticsData data = home.getSemanticsData();
      expect(
        data.hasAction(SemanticsAction.tap),
        isFalse,
        reason: 'Selected destination should not have a tap action',
      );

      final SemanticsNode settings = tester.getSemantics(find.text('Settings'));
      final SemanticsData settingsData = settings.getSemanticsData();
      expect(
        settingsData.hasAction(SemanticsAction.tap),
        isTrue,
        reason: 'Unselected destination should have a tap action',
      );
    },
  );
}
