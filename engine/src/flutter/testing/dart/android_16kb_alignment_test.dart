import 'dart:io';

import 'package:test/test.dart';

void main() {
  test('GNU_RELRO segment in libflutter.so is 16KB aligned', () async {
    // Resolve absolute paths
    final engineRoot = Platform.script.resolve('../../../').toFilePath();
    final soPath = '$engineRoot/out/android_release_arm64/libflutter.so';

    // Fallback to readelf in system PATH if hermetic llvm-readelf isn't found
    var readelfPath = 'readelf';
    final hostPlatform = Platform.operatingSystem == 'macos'
        ? 'mac-${Platform.version.contains('arm64') ? 'arm64' : 'x64'}'
        : 'linux-x64';
    final hermeticReadelf = '$engineRoot/flutter/buildtools/$hostPlatform/clang/bin/llvm-readelf';

    if (File(hermeticReadelf).existsSync()) {
      readelfPath = hermeticReadelf;
    }

    if (!File(soPath).existsSync()) {
      // In CI, libflutter.so should be built. If this test is run without it, we skip.
      markTestSkipped('libflutter.so not found at $soPath. Please build it first.');
      return;
    }

    final result = await Process.run(readelfPath, <String>['-lW', soPath]);
    expect(result.exitCode, 0, reason: 'readelf failed: ${result.stderr}');

    final lines = result.stdout.toString().split('\n');
    final relroLine = lines.firstWhere(
      (String line) => line.contains('GNU_RELRO'),
      orElse: () => '',
    );

    expect(relroLine, isNotEmpty, reason: 'GNU_RELRO segment not found in $soPath');

    final parts = relroLine.trim().split(RegExp(r'\s+'));
    // GNU_RELRO 0x... 0xVirtAddr ... 0x... 0xMemSiz ...
    final virtAddrHex = parts[2];
    final memSizHex = parts[5];

    final virtAddr = int.parse(virtAddrHex.replaceFirst('0x', ''), radix: 16);
    final memSiz = int.parse(memSizHex.replaceFirst('0x', ''), radix: 16);

    final endAddr = virtAddr + memSiz;
    final remainder = endAddr % 16384;

    expect(
      remainder,
      0,
      reason: 'GNU_RELRO end ($endAddr) is not aligned to 16KB (remainder $remainder)',
    );
  });
}
