// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@file:Suppress("PackageName")

package com.example.android_engine_test

import android.content.Context
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
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.android.FlutterView
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.embedding.engine.FlutterEngineGroup
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val engineGroup: FlutterEngineGroup by lazy { FlutterEngineGroup(this) }
    private var currentEngine: FlutterEngine? = null

    override fun provideFlutterEngine(context: Context): FlutterEngine? {
        val engine = engineGroup.createAndRunDefaultEngine(context)
        currentEngine = engine
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

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "com.example.android_engine_test/spawn")
            .setMethodCallHandler { call, result ->
                if (call.method == "spawn_and_destroy") {
                    val spawner = currentEngine
                    if (spawner != null) {
                        val flutterView = findViewById<FlutterView>(FlutterActivity.FLUTTER_VIEW_ID)
                        flutterView?.detachFromFlutterEngine()

                        val spawnedEngine = engineGroup.createAndRunDefaultEngine(this)
                        currentEngine = spawnedEngine
                        configureFlutterEngine(spawnedEngine)

                        flutterView?.attachToFlutterEngine(spawnedEngine)
                        spawner.destroy()
                        result.success(null)
                    } else {
                        result.error("NO_SPAWNER", "No spawner engine found", null)
                    }
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
