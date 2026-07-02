package io.flutter.embedding.engine.loader;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.graphics.Typeface;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import java.lang.reflect.Method;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class XmlFontReproductionTest {

  @Test
  public void testXmlFontConsumptionLimitations() {
    // 1. Demonstrate that we cannot easily get raw bytes from a Typeface.
    // In Android, a Typeface object (which would be loaded from an XML font using
    // ResourcesCompat.getFont) does not expose its underlying font data bytes.
    // There is no public API to get bytes. We verify this by checking that
    // no such method exists on the Typeface class.
    for (Method method : Typeface.class.getMethods()) {
      if (method.getName().equals("getFontData") || method.getName().equals("getBytes")) {
        fail("Typeface unexpectedly exposes font bytes, which would make a workaround possible.");
      }
    }

    // 2. Demonstrate that FlutterEngine does not have any API to consume a Typeface.
    // If we cannot get bytes, the only way to support XML fonts is if FlutterEngine
    // accepts a Typeface directly. We assert that such a method exists, which will
    // fail because the feature is not yet implemented.
    boolean hasRegisterTypefaceMethod = false;
    for (Method method : FlutterEngine.class.getMethods()) {
      if (method.getName().contains("Typeface")) {
        hasRegisterTypefaceMethod = true;
        break;
      }
      for (Class<?> paramType : method.getParameterTypes()) {
        if (paramType.equals(Typeface.class)) {
          hasRegisterTypefaceMethod = true;
          break;
        }
      }
    }

    assertTrue(
        "FlutterEngine should have a method to register or consume a Typeface to support XML fonts (flutter/flutter#47699).",
        hasRegisterTypefaceMethod
    );
  }
}
