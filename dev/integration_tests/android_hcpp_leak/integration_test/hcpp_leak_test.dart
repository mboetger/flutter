import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:flutter/services.dart';
import 'package:android_hcpp_leak/main.dart' as app;

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('meminfo');

  testWidgets('HCPP does not leak GPU memory', (WidgetTester tester) async {
    app.main();
    await tester.pumpAndSettle();
    
    // Warm up
    await tester.pump(const Duration(seconds: 2));
    final initialMemStr = await channel.invokeMethod('getGraphicsMemory');
    final initialMem = int.tryParse(initialMemStr.toString()) ?? 0;
    
    // Toggle the overlay 40 times with 1.5 second pauses, like the original repro
    for (int i = 0; i < 40; i++) {
      app.hcppLeakBodyState!.toggle();
      await tester.pump(const Duration(milliseconds: 1500));
      app.hcppLeakBodyState!.toggle();
      await tester.pump(const Duration(milliseconds: 1500));
    }
    
    final finalMemStr = await channel.invokeMethod('getGraphicsMemory');
    final finalMem = int.tryParse(finalMemStr.toString()) ?? 0;
    
    final diff = finalMem - initialMem;
    print('Initial: $initialMem KB, Final: $finalMem KB, Diff: $diff KB');
    
    expect(diff, lessThan(30000), reason: 'Graphics memory grew by ${diff} KB, indicating a leak!');
  });
}
