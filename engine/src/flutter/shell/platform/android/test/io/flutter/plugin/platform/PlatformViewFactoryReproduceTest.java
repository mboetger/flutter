// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.junit.Test;
import org.junit.runner.RunWith;
import androidx.test.ext.junit.runners.AndroidJUnit4;

@RunWith(AndroidJUnit4.class)
public class PlatformViewFactoryReproduceTest {

  @Test
  public void testCreateMethodHasNonNullContextAnnotation() {
    File sourceFile = findPlatformViewFactoryJava();
    assertNotNull(
        "PlatformViewFactory.java source file could not be found in the repository directory tree.",
        sourceFile
    );

    try {
      String content = new String(Files.readAllBytes(sourceFile.toPath()));

      // Regex to find the abstract create method declaration.
      // E.g., public abstract PlatformView create(..., Context context, ...)
      Pattern methodPattern = Pattern.compile(
          "public\\s+abstract\\s+PlatformView\\s+create\\s*\\(([^)]+)\\)\\s*;",
          Pattern.DOTALL
      );
      Matcher methodMatcher = methodPattern.matcher(content);

      if (methodMatcher.find()) {
        String parametersText = methodMatcher.group(1);

        // Use regex to find @NonNull followed by Context (allowing arbitrary whitespace/newlines)
        Pattern nonNullContextPattern = Pattern.compile("@NonNull\\s+Context\\b");
        Matcher nonNullContextMatcher = nonNullContextPattern.matcher(parametersText);

        assertTrue(
            "The Context parameter in PlatformViewFactory.create must be annotated with @NonNull "
            + "to prevent Kotlin compiler errors when implementing the interface (see issue #104480).\n"
            + "Found parameters: " + parametersText.trim(),
            nonNullContextMatcher.find()
        );
      } else {
        fail("Could not find the declaration of the 'create' method in PlatformViewFactory.java");
      }
    } catch (IOException e) {
      fail("Failed to read PlatformViewFactory.java: " + e.getMessage());
    }
  }

  /**
   * Climbs up the directory tree from the current working directory to robustly
   * locate PlatformViewFactory.java.
   */
  private static File findPlatformViewFactoryJava() {
    File dir = new File(".").getAbsoluteFile();
    String targetPath = "shell/platform/android/io/flutter/plugin/platform/PlatformViewFactory.java";
    String targetPathWithPrefix = "engine/src/flutter/" + targetPath;

    while (dir != null) {
      File file = new File(dir, targetPath);
      if (file.exists()) {
        return file;
      }
      file = new File(dir, targetPathWithPrefix);
      if (file.exists()) {
        return file;
      }
      dir = dir.getParentFile();
    }
    return null;
  }
}
