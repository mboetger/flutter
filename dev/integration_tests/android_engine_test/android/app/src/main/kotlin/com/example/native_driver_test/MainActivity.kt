// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@file:Suppress("PackageName")

package com.example.android_engine_test

import android.os.Bundle
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.example.android_engine_test.extensions.NativeDriverSupportPlugin
import com.example.android_engine_test.fixtures.BlueOrangeGradientPlatformViewFactory
import com.example.android_engine_test.fixtures.BlueOrangeGradientSurfaceViewPlatformViewFactory
import com.example.android_engine_test.fixtures.BoxPlatformViewFactory
import com.example.android_engine_test.fixtures.ChangingColorButtonPlatformViewFactory
import com.example.android_engine_test.fixtures.OtherFaceTexturePlugin
import com.example.android_engine_test.fixtures.SmileyFaceTexturePlugin
import android.content.Context
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.android.FlutterActivityLaunchConfigs
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.embedding.engine.FlutterEngineCache
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    override fun provideFlutterEngine(context: Context): FlutterEngine {
        var engine = FlutterEngineCache.getInstance().get("temp_engine")
        if (engine == null) {
            engine = FlutterEngine(context)
            FlutterEngineCache.getInstance().put("temp_engine", engine)
        }
        return engine
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        // Intentionally do not use GeneratedPluginRegistrant.

        flutterEngine
            .plugins
            .apply {
                add(SmileyFaceTexturePlugin())
                add(OtherFaceTexturePlugin())
                add(NativeDriverSupportPlugin())
            }

        flutterEngine
            .platformViewsController
            .registry
            .apply {
                registerViewFactory("blue_orange_gradient_platform_view", BlueOrangeGradientPlatformViewFactory())
                registerViewFactory("blue_orange_gradient_surface_view_platform_view", BlueOrangeGradientSurfaceViewPlatformViewFactory())
                registerViewFactory("changing_color_button_platform_view", ChangingColorButtonPlatformViewFactory())
                registerViewFactory("box_platform_view", BoxPlatformViewFactory())
            }

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "samples.flutter.dev/info")
            .setMethodCallHandler { call, result ->
                if (call.method == "launchTransparentActivity") {
                    val intent = FlutterActivity
                        .withCachedEngine("temp_engine")
                        .backgroundMode(FlutterActivityLaunchConfigs.BackgroundMode.transparent)
                        .build(this)
                    startActivity(intent)
                    result.success(true)
                } else {
                    result.notImplemented()
                }
            }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // https://developer.android.com/training/system-ui
        val windowInsetsController = WindowCompat.getInsetsController(window, window.decorView)
        windowInsetsController.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        windowInsetsController.hide(WindowInsetsCompat.Type.systemBars())
        actionBar?.hide()
    }
}
