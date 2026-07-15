// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@file:Suppress("PackageName")

package com.example.android_engine_test.extensions

import android.app.Activity
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import android.view.MotionEvent
import android.view.Window
import io.flutter.Log
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import java.util.concurrent.atomic.AtomicInteger

class NativeDriverSupportPlugin :
    ActivityAware,
    FlutterPlugin,
    MethodCallHandler {
    private val tag = "NativeDriverSupportPlugin"
    private var handlerThread: HandlerThread? = null
    private var frameMetricsListener: Window.OnFrameMetricsAvailableListener? = null
    private val frameCount = AtomicInteger(0)
    private lateinit var channel: MethodChannel
    private var activity: Activity? = null

    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(binding.binaryMessenger, "native_driver")
        channel.setMethodCallHandler(this)
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
    }

    override fun onMethodCall(
        call: MethodCall,
        result: MethodChannel.Result
    ) {
        val activity = this.activity
        if (activity == null) {
            Log.w(tag, "Received method channel, but no current activity")
            return
        }
        when (call.method) {
            "sdk_version" -> {
                val versionMap = mapOf("version" to Build.VERSION.SDK_INT)
                result.success(versionMap)
            }
            "is_emulator" -> {
                val isEmulator =
                    when {
                        Build.MODEL.contains("gphone") -> true
                        else -> false
                    }
                result.success(mapOf("emulator" to isEmulator))
            }
            "ping" -> {
                result.success(null)
            }
            "get_system_ui_visibility" -> {
                @Suppress("DEPRECATION")
                val flags = activity.window.decorView.systemUiVisibility
                result.success(mapOf("system_ui_visibility" to flags))
            }
            "tap_view" -> {
                // Decode the selector.
                val kind = call.argument<String>("kind")
                lateinit var selector: NativeSelector
                when (kind) {
                    "byNativeAccessibilityLabel" -> {
                        selector = NativeSelector.ByContentDescription(call.argument("label")!!)
                    }
                    "byNativeIntegerId" -> {
                        val stringId = call.argument<String>("id")!!
                        selector = NativeSelector.ByViewId(stringId.toInt())
                    }
                    else -> {
                        result.error("INVALID_SELECTOR", "Not supported", kind)
                        return
                    }
                }

                // Fail if not found.
                val found = selector.find(activity.window.decorView.rootView)
                if (found == null) {
                    result.error("VIEW_NOT_FOUND", "No view was found", call.arguments())
                    return
                }

                // Send tap event.
                val x = found.x + found.width / 2
                val y = found.y + found.height / 2
                val downTime = SystemClock.uptimeMillis()

                val pressDown = MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, x, y, 0)
                found.dispatchTouchEvent(pressDown)
                pressDown.recycle()

                val pressUp = MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_UP, x, y, 0)
                found.dispatchTouchEvent(pressUp)
                pressUp.recycle()
                result.success(null)
            }
            "register_frame_metrics_listener" -> {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    frameCount.set(0)
                    val thread = HandlerThread("FrameMetricsListenerThread")
                    thread.start()
                    handlerThread = thread
                    val handler = Handler(thread.looper)
                    val listener = Window.OnFrameMetricsAvailableListener { _, _, _ ->
                        frameCount.incrementAndGet()
                    }
                    frameMetricsListener = listener
                    activity.window.addOnFrameMetricsAvailableListener(listener, handler)
                    result.success(mapOf("success" to true))
                } else {
                    result.error("UNSUPPORTED", "Frame metrics require API 24+", null)
                }
            }
            "unregister_frame_metrics_listener" -> {
                cleanUpFrameMetrics()
                result.success(mapOf("success" to true))
            }
            "get_frame_metrics_count" -> {
                result.success(mapOf("count" to frameCount.get()))
            }
            else -> {
                result.notImplemented()
            }
        }
    }

    private fun cleanUpFrameMetrics() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            val listener = frameMetricsListener
            val act = activity
            if (listener != null && act != null) {
                try {
                    act.window.removeOnFrameMetricsAvailableListener(listener)
                } catch (e: Exception) {
                    Log.e(tag, "Failed to remove frame metrics listener", e)
                }
                frameMetricsListener = null
            }
            handlerThread?.quitSafely()
            handlerThread = null
        }
    }

    override fun onAttachedToActivity(binding: ActivityPluginBinding) {
        activity = binding.activity
    }

    override fun onDetachedFromActivityForConfigChanges() {
        cleanUpFrameMetrics()
        activity = null
    }

    override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
        activity = binding.activity
    }

    override fun onDetachedFromActivity() {
        cleanUpFrameMetrics()
        activity = null
    }
}
