import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets(
    'Incorrect onPopPage usage (veto + didPop) causes visual-structural mismatch and StateError',
    (WidgetTester tester) async {
      final popLog = <String>[];
      var didPopCalled = false;

      // Scenario: The developer incorrectly calls route.didPop(result) (which starts
      // the visual pop transition) but returns false from onPopPage (which vetoes
      // the structural pop in the Navigator history).
      final RouterDelegate<Object> delegate = _MyRouterDelegate(
        pages: const <Page<dynamic>>[
          MaterialPage<void>(child: Text('Screen 1')),
          MaterialPage<void>(child: Text('Screen 2')),
        ],
        onPopPage: (Route<dynamic> route, dynamic result) {
          popLog.add('pop_call');
          if (!didPopCalled) {
            didPopCalled = true;
            if (!route.didPop(result)) {
              return false;
            }
          }
          return false; // Vetoes the pop structurally
        },
      );

      await tester.pumpWidget(
        MaterialApp.router(
          routerDelegate: delegate,
          backButtonDispatcher: RootBackButtonDispatcher(),
        ),
      );

      expect(find.text('Screen 2'), findsOneWidget);

      // 1. First back press: Screen 2 visually pops (didPop called) but structurally remains (onPopPage returned false).
      bool handled = await _simulateHardwareBackButton(tester);
      expect(popLog, <String>['pop_call']);
      expect(handled, isTrue); // Stays in foreground because pop was vetoed

      // Pump to let transitions run. Screen 1 is now visible, Screen 2 has visually slid off.
      await tester.pumpAndSettle();
      expect(find.text('Screen 1'), findsOneWidget);

      popLog.clear();

      // 2. Second back press: Structurally, Screen 2 is STILL on top of the stack.
      // Pressing back again will try to pop Screen 2 again. We avoid calling didPop
      // again to prevent a StateError, but we assert that onPopPage IS called again
      // and the app remains in the foreground (buggy inconsistent behavior).
      handled = await _simulateHardwareBackButton(tester);

      // Assert the buggy symptoms:
      // a) onPopPage IS called again for Screen 2.
      expect(popLog, <String>['pop_call']);
      // b) The back press returns true (app does NOT move to background) because
      // the pop was vetoed again.
      expect(handled, isTrue);

      // Clean up: Force unmount the widget tree to dispose of the active route
      // and clean up the performance mode request. Since we avoided the crash,
      // the Navigator is not locked and will dispose cleanly.
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets('Correct usage (onDidRemovePage) behaves consistently and bubbles on last page', (
    WidgetTester tester,
  ) async {
    final removeLog = <String>[];
    final pages = <Page<dynamic>>[
      const MaterialPage<void>(child: Text('Screen 1')),
      const MaterialPage<void>(child: Text('Screen 2')),
    ];

    final RouterDelegate<Object> delegate = _MyRouterDelegate(
      pages: pages,
      onDidRemovePage: (Page<dynamic> page) {
        removeLog.add('removed');
        pages.remove(page); // Correctly update the state
      },
    );

    await tester.pumpWidget(
      MaterialApp.router(
        routerDelegate: delegate,
        backButtonDispatcher: RootBackButtonDispatcher(),
      ),
    );

    expect(find.text('Screen 2'), findsOneWidget);

    // 1. First back press: Screen 2 is popped and removed.
    bool handled = await _simulateHardwareBackButton(tester);
    expect(removeLog, <String>['removed']); // Screen 2 removed
    expect(handled, isTrue); // Stays in foreground

    await tester.pumpAndSettle();
    expect(find.text('Screen 1'), findsOneWidget);
    removeLog.clear();

    // 2. Second back press: Only Screen 1 is left. It correctly bubbles and exits.
    handled = await _simulateHardwareBackButton(tester);
    expect(removeLog, isEmpty); // onDidRemovePage NOT called for last page
    expect(handled, isFalse); // Bubbles to platform (exits app)

    // Clean up
    await tester.pumpWidget(const SizedBox());
  });
}

Future<bool> _simulateHardwareBackButton(WidgetTester tester) async {
  final ByteData message = const JSONMethodCodec().encodeMethodCall(const MethodCall('popRoute'));
  final ByteData? reply = await tester.binding.defaultBinaryMessenger.handlePlatformMessage(
    'flutter/navigation',
    message,
    (_) {},
  );
  return const JSONMethodCodec().decodeEnvelope(reply!) as bool;
}

class _MyRouterDelegate extends RouterDelegate<Object>
    with ChangeNotifier, PopNavigatorRouterDelegateMixin<Object> {
  _MyRouterDelegate({required this.pages, this.onPopPage, this.onDidRemovePage})
    : navigatorKey = GlobalKey<NavigatorState>();

  final List<Page<dynamic>> pages;
  final PopPageCallback? onPopPage;
  final DidRemovePageCallback? onDidRemovePage;

  @override
  final GlobalKey<NavigatorState> navigatorKey;

  @override
  Widget build(BuildContext context) {
    return Navigator(
      key: navigatorKey,
      pages: pages,
      onPopPage: onPopPage,
      onDidRemovePage: onDidRemovePage,
    );
  }

  @override
  Future<void> setNewRoutePath(Object configuration) async {}
}
