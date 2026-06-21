// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.example.integration_test_example;

import android.content.Context;
import android.graphics.Color;
import android.view.View;
import androidx.annotation.NonNull;
import io.flutter.plugin.platform.PlatformView;

public class SimplePlatformView implements PlatformView {
    @NonNull
    private final View view;

    SimplePlatformView(@NonNull Context context) {
        view = new View(context);
        view.setBackgroundColor(Color.BLUE);
    }

    @NonNull
    @Override
    public View getView() {
        return view;
    }

    @Override
    public void dispose() {
        // No-op
    }
}
