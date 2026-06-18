package io.flutter.plugin.editing;

import android.os.Bundle;
import android.util.SparseIntArray;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;
import java.util.ArrayList;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowInputMethodManager;

@Implements(InputMethodManager.class)
public class TestImm extends ShadowInputMethodManager {
  private InputMethodSubtype currentInputMethodSubtype;
  private SparseIntArray restartCounter = new SparseIntArray();
  private CursorAnchorInfo cursorAnchorInfo;
  private ArrayList<Integer> selectionUpdateValues;
  private boolean trackSelection = false;
  private EventHandler handler;
  private boolean showSoftInputResult = true;
  private boolean hideSoftInputResult = true;

  public void setShowSoftInputResult(boolean result) {
    showSoftInputResult = result;
  }

  public void setHideSoftInputResult(boolean result) {
    hideSoftInputResult = result;
  }

  @Implementation
  public boolean showSoftInput(View view, int flags) {
    return showSoftInputResult;
  }

  @Implementation
  public boolean hideSoftInputFromWindow(android.os.IBinder windowToken, int flags) {
    return hideSoftInputResult;
  }

  public TestImm() {
    selectionUpdateValues = new ArrayList<Integer>();
  }

  @Implementation
  public InputMethodSubtype getCurrentInputMethodSubtype() {
    return currentInputMethodSubtype;
  }

  @Implementation
  public void restartInput(View view) {
    int count = restartCounter.get(view.hashCode(), /*defaultValue=*/ 0) + 1;
    restartCounter.put(view.hashCode(), count);
  }

  public void setCurrentInputMethodSubtype(InputMethodSubtype inputMethodSubtype) {
    this.currentInputMethodSubtype = inputMethodSubtype;
  }

  public int getRestartCount(View view) {
    return restartCounter.get(view.hashCode(), /*defaultValue=*/ 0);
  }

  public void setEventHandler(EventHandler eventHandler) {
    handler = eventHandler;
  }

  @Implementation
  public void sendAppPrivateCommand(View view, String action, Bundle data) {
    if (handler != null) {
      handler.sendAppPrivateCommand(view, action, data);
    }
  }

  @Implementation
  public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
    this.cursorAnchorInfo = cursorAnchorInfo;
  }

  // We simply store the values to verify later.
  @Implementation
  public void updateSelection(
      View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
    if (trackSelection) {
      this.selectionUpdateValues.add(selStart);
      this.selectionUpdateValues.add(selEnd);
      this.selectionUpdateValues.add(candidatesStart);
      this.selectionUpdateValues.add(candidatesEnd);
    }
  }

  // only track values when enabled via this.
  public void setTrackSelection(boolean val) {
    trackSelection = val;
  }

  // Returns true if the last updateSelection call passed the following values.
  public ArrayList<Integer> getSelectionUpdateValues() {
    return selectionUpdateValues;
  }

  public CursorAnchorInfo getLastCursorAnchorInfo() {
    return cursorAnchorInfo;
  }
}
