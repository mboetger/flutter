// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.editing;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.view.View;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.android.FlutterView;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.systemchannels.ScribeChannel;
import io.flutter.embedding.engine.systemchannels.TextInputChannel;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.plugin.platform.PlatformViewsController2;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

@Config(shadows = {TextInputPluginTest.TestImm.class, TextInputPluginTest.TestAfm.class})
@RunWith(AndroidJUnit4.class)
public class TextInputPluginSensitiveContentReproductionTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Before
  public void setUp() {
    MockitoAnnotations.openMocks(this);
  }

  @Config(minSdk = 26) // Autofill hints on View are supported from API 26
  @Test
  public void testAutofillHintsLifecycleAndTransitions() {
    FlutterView testView = new FlutterView(ctx);
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    ScribeChannel scribeChannel = new ScribeChannel(mock(DartExecutor.class));
    TextInputPlugin textInputPlugin =
        new TextInputPlugin(
            testView,
            textInputChannel,
            scribeChannel,
            mock(PlatformViewsController.class),
            mock(PlatformViewsController2.class));

    // 1. Set a sensitive client (e.g., password)
    final String[] sensitiveHints = new String[] {View.AUTOFILL_HINT_PASSWORD};
    final TextInputChannel.Configuration sensitiveConfig = createConfigWithHints(sensitiveHints);
    textInputPlugin.setTextInputClient(1, sensitiveConfig);

    // Assert: Hints are set
    assertNotNull("Parent view autofill hints should not be null", testView.getAutofillHints());
    assertArrayEquals(
        "Parent view autofill hints should be updated to match the active sensitive client's hints",
        sensitiveHints,
        testView.getAutofillHints());

    // 2. Transition to a non-sensitive client (e.g., username)
    final String[] nonSensitiveHints = new String[] {View.AUTOFILL_HINT_USERNAME};
    final TextInputChannel.Configuration nonSensitiveConfig = createConfigWithHints(nonSensitiveHints);
    textInputPlugin.setTextInputClient(2, nonSensitiveConfig);

    // Assert: Hints are updated to match the new client
    assertNotNull("Parent view autofill hints should not be null", testView.getAutofillHints());
    assertArrayEquals(
        "Parent view autofill hints should be updated to match the active non-sensitive client's hints",
        nonSensitiveHints,
        testView.getAutofillHints());

    // 3. Transition to a client with NO autofill configuration
    final TextInputChannel.Configuration noAutofillConfig = createConfigWithHints(null);
    textInputPlugin.setTextInputClient(3, noAutofillConfig);

    // Assert: Hints are cleared/null
    assertNull("Parent view autofill hints should be cleared/null when client has no autofill config", testView.getAutofillHints());

    // 4. Set it back to sensitive, then clear the text input client completely
    textInputPlugin.setTextInputClient(4, sensitiveConfig);
    assertNotNull("Parent view autofill hints should not be null", testView.getAutofillHints());
    assertArrayEquals(sensitiveHints, testView.getAutofillHints());

    textInputPlugin.clearTextInputClient();

    // Assert: Hints are cleared/null when client is cleared
    assertNull("Parent view autofill hints should be cleared/null when text input client is cleared", testView.getAutofillHints());
  }

  private TextInputChannel.Configuration createConfigWithHints(String[] hints) {
    final TextInputChannel.Configuration.Autofill autofill = hints == null ? null :
        new TextInputChannel.Configuration.Autofill(
            "unique_id",
            hints,
            "Enter text",
            new TextInputChannel.TextEditState("", 0, 0, -1, -1));

    return new TextInputChannel.Configuration(
        false,
        false,
        true,
        true,
        false,
        TextInputChannel.TextCapitalization.NONE,
        null,
        null,
        null,
        autofill,
        null,
        null,
        null);
  }
}
