// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.example.integration_test_example;

import androidx.annotation.NonNull;
import io.flutter.embedding.android.FlutterActivity;
import io.flutter.embedding.engine.FlutterEngine;

public class MainActivity extends FlutterActivity {
    @Override
    public void configureFlutterEngine(@NonNull FlutterEngine flutterEngine) {
        super.configureFlutterEngine(flutterEngine); // CRITICAL: Ensures plugins are registered
        flutterEngine
            .getPlatformViewsController()
            .getRegistry()
            .registerViewFactory("simple-platform-view", new SimplePlatformViewFactory());
    }
}
