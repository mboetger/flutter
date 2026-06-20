// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.plugins.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.plugins.GeneratedPluginRegistrant;
import java.util.List;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLog;

@RunWith(AndroidJUnit4.class)
public class GeneratedPluginRegisterTest {
  @Before
  public void setUp() {
    GeneratedPluginRegistrant.clearRegisteredEngines();
    GeneratedPluginRegistrant.pluginRegistrationException = null;
    ShadowLog.clear();
  }

  @After
  public void tearDown() {
    GeneratedPluginRegistrant.clearRegisteredEngines();
    GeneratedPluginRegistrant.pluginRegistrationException = null;
    ShadowLog.clear();
  }

  @Test
  public void itLogsHelpfulMessageAndDoesNotWrapException() {
    // 1. Setup a plugin exception
    RuntimeException pluginException = new RuntimeException("Crash in plugin initialization");
    GeneratedPluginRegistrant.pluginRegistrationException = pluginException;

    FlutterEngine mockEngine = mock(FlutterEngine.class);

    // 2. Call registerGeneratedPlugins
    GeneratedPluginRegister.registerGeneratedPlugins(mockEngine);

    // 3. Retrieve logs
    List<ShadowLog.LogItem> logs = ShadowLog.getLogsForTag("GeneratedPluginsRegister");

    // There should be two logs: the warning/error message, and the exception log.
    assertEquals(2, logs.size());

    ShadowLog.LogItem firstLog = logs.get(0);
    ShadowLog.LogItem secondLog = logs.get(1);

    // Desired Behavior 1: The log message should be helpful and not claim it couldn't find or invoke the registrant.
    assertFalse(
        "Expected log to not contain misleading 'could not find or invoke' message when registrant exists",
        firstLog.msg.contains("could not find or invoke"));
    assertTrue(
        "Expected log to indicate that a plugin threw an exception",
        firstLog.msg.contains("An exception was thrown by an automatically registered plugin"));

    // Desired Behavior 2: The logged exception should be the actual root cause, not the wrapped InvocationTargetException.
    assertNotNull(secondLog.throwable);
    assertEquals(
        "Expected the logged exception to be the actual plugin exception",
        pluginException,
        secondLog.throwable);
  }
}
