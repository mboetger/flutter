package io.flutter.plugin.editing;

import android.os.Bundle;
import android.view.View;

public interface EventHandler {
  void sendAppPrivateCommand(View view, String action, Bundle data);
}
