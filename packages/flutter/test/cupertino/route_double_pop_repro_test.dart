import 'package:flutter/cupertino.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('CupertinoPageRoute double pop reproduction', (WidgetTester tester) async {
    await tester.pumpWidget(
      CupertinoApp(
        onGenerateRoute: (RouteSettings settings) {
          return CupertinoPageRoute<void>(
            settings: settings,
            builder: (BuildContext context) {
              return CupertinoPageScaffold(
                child: Center(
                  child: Text('Page ${settings.name}'),
                ),
              );
            },
          );
        },
        initialRoute: '1',
      ),
    );

    expect(find.text('Page 1'), findsOneWidget);

    // Push Page 2
    tester.state<NavigatorState>(find.byType(Navigator)).pushNamed('2');
    await tester.pumpAndSettle();
    expect(find.text('Page 2'), findsOneWidget);

    // Push Page 3
    tester.state<NavigatorState>(find.byType(Navigator)).pushNamed('3');
    await tester.pumpAndSettle();
    expect(find.text('Page 3'), findsOneWidget);

    // Start a back gesture on Page 3
    final TestGesture gesture = await tester.startGesture(const Offset(5.0, 300.0));
    // Drag far enough to trigger a pop
    await gesture.moveBy(const Offset(400.0, 0.0));
    // Release gesture
    await gesture.up();

    // Pump once to let the dragEnd register and call pop() and transition to popping state
    await tester.pump();

    // Now Page 3 is in the process of popping.
    // Simulate a system back press (which calls maybePop)
    await WidgetsBinding.instance.handlePopRoute();

    // Settle animations
    await tester.pumpAndSettle();

    // If the bug is present, Page 2 was also popped, so we are on Page 1.
    // If the bug is fixed, Page 2 is not popped, so we are on Page 2.
    expect(find.text('Page 2'), findsOneWidget);
    expect(find.text('Page 3'), findsNothing);
    expect(find.text('Page 1'), findsNothing);
  });
}
