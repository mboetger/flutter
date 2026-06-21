// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test.io.flutter.embedding.engine;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterShellArgs;
import java.util.Arrays;
import java.util.HashSet;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterShellArgsTest {
  @Test
  // Annotation required because FlutterShellArgs was deprecated in favor of FlutterEngineFlags.
  @SuppressWarnings("deprecation")
  public void itProcessesShellFlags() {
    // Setup the test.
    Intent intent = new Intent();
    intent.putExtra("dart-flags", "--observe --no-hot --no-pub");
    intent.putExtra("trace-skia-allowlist", "skia.a,skia.b");

    // Execute the behavior under test.
    FlutterShellArgs args = FlutterShellArgs.fromIntent(intent);
    HashSet<String> argValues = new HashSet<String>(Arrays.asList(args.toArray()));

    // Verify results.
    assertEquals(2, argValues.size());
    assertTrue(argValues.contains("--dart-flags=--observe --no-hot --no-pub"));
    assertTrue(argValues.contains("--trace-skia-allowlist=skia.a,skia.b"));
  }

  @Test
  @SuppressWarnings("deprecation")
  public void itPropagatesEnableFlutterGpuFromIntent() {
    Intent intent = new Intent();
    intent.putExtra(FlutterShellArgs.ARG_KEY_ENABLE_FLUTTER_GPU, true);

    FlutterShellArgs args = FlutterShellArgs.fromIntent(intent);
    HashSet<String> argValues = new HashSet<String>(Arrays.asList(args.toArray()));

    assertEquals(1, argValues.size());
    assertTrue(argValues.contains(FlutterShellArgs.ARG_ENABLE_FLUTTER_GPU));
  }

  @Test
  @SuppressWarnings("deprecation")
  public void itDoesNotPropagateEnableFlutterGpuWhenAbsent() {
    Intent intent = new Intent();

    FlutterShellArgs args = FlutterShellArgs.fromIntent(intent);
    HashSet<String> argValues = new HashSet<String>(Arrays.asList(args.toArray()));

    assertEquals(0, argValues.size());
  }

  @Test
  @SuppressWarnings("deprecation")
  public void itDoesNotPropagateDebugFlagsWhenLaunchedFromHistory() {
    Intent intent = new Intent();
    intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY);

    // Debug / VM service-related flags that MUST be ignored
    intent.putExtra(FlutterShellArgs.ARG_KEY_START_PAUSED, true);
    intent.putExtra(FlutterShellArgs.ARG_KEY_VM_SERVICE_PORT, 50224);
    intent.putExtra(FlutterShellArgs.ARG_KEY_ENABLE_DART_PROFILING, true);
    intent.putExtra(FlutterShellArgs.ARG_KEY_DISABLE_SERVICE_AUTH_CODES, true);
    intent.putExtra(FlutterShellArgs.ARG_KEY_DART_FLAGS, "--observe");

    // Non-debug configuration flags that MUST still be propagated
    intent.putExtra(FlutterShellArgs.ARG_KEY_ENABLE_FLUTTER_GPU, true);

    FlutterShellArgs args = FlutterShellArgs.fromIntent(intent);
    HashSet<String> argValues = new HashSet<String>(Arrays.asList(args.toArray()));

    // Verify debug/debugger-related flags are excluded
    assertFalse("Should not contain --start-paused", argValues.contains(FlutterShellArgs.ARG_START_PAUSED));
    assertFalse("Should not contain --vm-service-port", argValues.contains(FlutterShellArgs.ARG_VM_SERVICE_PORT + "50224"));
    assertFalse("Should not contain --enable-dart-profiling", argValues.contains(FlutterShellArgs.ARG_ENABLE_DART_PROFILING));
    assertFalse("Should not contain --disable-service-auth-codes", argValues.contains(FlutterShellArgs.ARG_DISABLE_SERVICE_AUTH_CODES));
    assertFalse("Should not contain --dart-flags=--observe", argValues.contains(FlutterShellArgs.ARG_DART_FLAGS + "=--observe"));

    // Verify non-debug configuration flags are still preserved
    assertTrue("Should contain --enable-flutter-gpu", argValues.contains(FlutterShellArgs.ARG_ENABLE_FLUTTER_GPU));
  }
}

