// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.yourcompany.integration_ui;

import androidx.annotation.NonNull;
import io.flutter.embedding.android.FlutterActivity;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.plugin.common.MethodChannel;
import java.io.File;

public class MainActivity extends FlutterActivity {
    private static final String CHANNEL = "integration_ui/storage";

    @Override
    public void configureFlutterEngine(@NonNull FlutterEngine flutterEngine) {
        super.configureFlutterEngine(flutterEngine);
        new MethodChannel(flutterEngine.getDartExecutor().getBinaryMessenger(), CHANNEL)
            .setMethodCallHandler(
                (call, result) -> {
                    if (call.method.equals("getExternalFilesDir")) {
                        File dir = getExternalFilesDir(null);
                        if (dir != null) {
                            result.success(dir.getAbsolutePath());
                        } else {
                            result.error("UNAVAILABLE", "External files directory not available.", null);
                        }
                    } else {
                        result.notImplemented();
                    }
                }
            );
    }
}
