package io.flutter.plugin.editing;

import static io.flutter.Build.API_LEVELS;

import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowAutofillManager;

@Implements(AutofillManager.class)
public class TestAfm extends ShadowAutofillManager {
  public static int empty = -999;

  public TestAfm() {}

  String finishState;
  int changeVirtualId = empty;
  String changeString;

  int enterId = empty;
  int exitId = empty;

  @Implementation
  public void cancel() {
    finishState = "cancel";
  }

  public void commit() {
    finishState = "commit";
  }

  public void notifyViewEntered(View view, int virtualId, Rect absBounds) {
    enterId = virtualId;
  }

  public void notifyViewExited(View view, int virtualId) {
    exitId = virtualId;
  }

  public void notifyValueChanged(View view, int virtualId, AutofillValue value) {
    if (Build.VERSION.SDK_INT < API_LEVELS.API_26) {
      return;
    }
    changeVirtualId = virtualId;
    changeString = value.getTextValue().toString();
  }

  public void resetStates() {
    finishState = null;
    changeVirtualId = empty;
    changeString = null;
    enterId = empty;
    exitId = empty;
  }
}
