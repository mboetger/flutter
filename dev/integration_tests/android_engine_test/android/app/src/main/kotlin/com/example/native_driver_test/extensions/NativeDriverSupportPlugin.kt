// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@file:Suppress("PackageName")

package com.example.android_engine_test.extensions

import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.Choreographer
import android.view.MotionEvent
import android.view.PixelCopy
import io.flutter.Log
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.embedding.engine.renderer.FlutterRenderer
import io.flutter.embedding.engine.renderer.FlutterUiDisplayListener
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler

class NativeDriverSupportPlugin :
    ActivityAware,
    FlutterPlugin,
    MethodCallHandler {
    private val tag = "NativeDriverSupportPlugin"
    private lateinit var channel: MethodChannel
    private var activity: Activity? = null
    private var flutterRenderer: FlutterRenderer? = null
    private var isFlutterUiDisplayed = false
    private val pendingDisplayResults = mutableListOf<MethodChannel.Result>()
    private var recordingFrames = false
    private var frameCounter = 0
    private var pendingPixelCopies = 0
    private var pendingStopRecordingResult: MethodChannel.Result? = null
    private val recordedFrameSummaries = mutableListOf<String>()
    private val mainHandler = Handler(Looper.getMainLooper())

    private fun findFlutterSurfaceView(root: android.view.View): android.view.SurfaceView? {
        if (root is io.flutter.embedding.android.FlutterSurfaceView) {
            return root
        }
        if (root is android.view.ViewGroup) {
            for (i in 0 until root.childCount) {
                val found = findFlutterSurfaceView(root.getChildAt(i))
                if (found != null) return found
            }
        }
        return null
    }

    private fun findChildSurfaceView(root: android.view.View): android.view.SurfaceView? {
        if (root is android.view.SurfaceView &&
            root !is io.flutter.embedding.android.FlutterSurfaceView
        ) {
            return root
        }
        if (root is android.view.ViewGroup) {
            for (i in 0 until root.childCount) {
                val found = findChildSurfaceView(root.getChildAt(i))
                if (found != null) return found
            }
        }
        return null
    }

    private fun findFlutterView(root: android.view.View): io.flutter.embedding.android.FlutterView? {
        if (root is io.flutter.embedding.android.FlutterView) {
            return root
        }
        if (root is android.view.ViewGroup) {
            for (i in 0 until root.childCount) {
                val found = findFlutterView(root.getChildAt(i))
                if (found != null) return found
            }
        }
        return null
    }

    private fun describeViewTree(v: android.view.View): String {
        val name = v.javaClass.simpleName.ifEmpty { v.javaClass.name.substringAfterLast('.') }
        val vis = when (v.visibility) {
            android.view.View.VISIBLE -> "V"
            android.view.View.INVISIBLE -> "I"
            android.view.View.GONE -> "G"
            else -> "?"
        }
        val loc = IntArray(2)
        v.getLocationOnScreen(loc)
        val sb = StringBuilder()
        sb.append("$name($vis,${v.left},${v.top}@${loc[0]},${loc[1]},${v.width}x${v.height}")
        if (v is android.view.SurfaceView && v !is io.flutter.embedding.android.FlutterSurfaceView) {
            val rf = v.holder.surfaceFrame
            sb.append(",sf=${rf.width()}x${rf.height()},val=${v.holder.surface.isValid}")
        }
        if (v is android.view.ViewGroup && v !is android.webkit.WebView && v.childCount > 0) {
            sb.append("<")
            for (i in 0 until v.childCount) {
                if (i > 0) sb.append(",")
                sb.append(describeViewTree(v.getChildAt(i)))
            }
            sb.append(">")
        }
        sb.append(")")
        return sb.toString()
    }

    private fun describeFlutterView(root: android.view.View): String {
        val fv = findFlutterView(root) ?: return "noFV"
        val sb = StringBuilder()
        for (i in 0 until fv.childCount) {
            sb.append(describeViewTree(fv.getChildAt(i))).append(";")
        }
        return sb.toString()
    }

    private fun classifyRegions(bitmap: Bitmap): String {
        val w = bitmap.width
        val h = bitmap.height
        fun countRegion(x0f: Double, x1f: Double, y0f: Double, y1f: Double): String {
            var bCnt = 0
            var rCnt = 0
            var kCnt = 0
            val x0 = (w * x0f).toInt().coerceIn(0, w - 1)
            val x1 = (w * x1f).toInt().coerceIn(x0 + 1, w)
            val y0 = (h * y0f).toInt().coerceIn(0, h - 1)
            val y1 = (h * y1f).toInt().coerceIn(y0 + 1, h)
            for (y in y0 until y1 step 4) {
                for (x in x0 until x1 step 4) {
                    val c = bitmap.getPixel(x, y)
                    val r = Color.red(c)
                    val g = Color.green(c)
                    val b = Color.blue(c)
                    if (r > 200 && g < 60 && b < 60) {
                        rCnt++
                    } else if (b > 180 && r < 100) {
                        bCnt++
                    } else if (r < 25 && g < 25 && b < 25) {
                        kCnt++
                    }
                }
            }
            return "B$bCnt/R$rCnt/K$kCnt"
        }
        val tl = countRegion(0.02, 0.45, 0.02, 0.15)
        val mid = countRegion(0.20, 0.80, 0.25, 0.70)
        val bl = countRegion(0.02, 0.22, 0.85, 0.96)
        val bot = countRegion(0.35, 0.65, 0.85, 0.96)
        return "tl($tl)_mid($mid)_bl($bl)_bot($bot)"
    }

    private fun onPixelCopyFinished() {
        pendingPixelCopies = (pendingPixelCopies - 1).coerceAtLeast(0)
        if (!recordingFrames && pendingPixelCopies == 0) {
            flushPendingStopRecordingResult()
        }
    }

    private fun flushPendingStopRecordingResult() {
        val pending = pendingStopRecordingResult ?: return
        pendingStopRecordingResult = null
        mainHandler.removeCallbacks(stopRecordingTimeoutRunnable)
        val copy = synchronized(recordedFrameSummaries) {
            recordedFrameSummaries.toList()
        }
        pending.success(copy)
    }

    private val stopRecordingTimeoutRunnable = Runnable {
        flushPendingStopRecordingResult()
    }

    private fun stopFrameRecordingInternal() {
        recordingFrames = false
        Choreographer.getInstance().removeFrameCallback(frameCallback)
        mainHandler.removeCallbacks(stopRecordingTimeoutRunnable)
    }

    private val frameCallback = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            if (!recordingFrames) return
            if (frameCounter < 120) {
                Choreographer.getInstance().postFrameCallback(this)
            }
            mainHandler.post {
                if (!recordingFrames) return@post
                val act = activity ?: return@post
                val idx = frameCounter++
                val decor = act.window.decorView
                val w = (decor.width / 4).coerceAtLeast(1)
                val h = (decor.height / 4).coerceAtLeast(1)
                val fvDesc = describeFlutterView(decor)
                val surfaceView = findFlutterSurfaceView(decor)
                val childSv = findChildSurfaceView(decor)

                val slotIndex: Int
                synchronized(recordedFrameSummaries) {
                    slotIndex = recordedFrameSummaries.size
                    recordedFrameSummaries.add("f$idx:[$fvDesc]")
                }

                // Avoid queuing overlapping PixelCopy requests across frames so SurfaceFlinger's
                // CompositionEngine is not starved when compositing VirtualDisplays.
                if (pendingPixelCopies > 0) {
                    return@post
                }

                val isImageViewActive = fvDesc.contains("FlutterImageView(V,")
                if (!isImageViewActive &&
                    surfaceView != null &&
                    surfaceView.width > 0 &&
                    surfaceView.height > 0 &&
                    surfaceView.holder.surface.isValid
                ) {
                    val bmpS = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
                    pendingPixelCopies++
                    try {
                        PixelCopy.request(surfaceView, bmpS, { res ->
                            val sStr = if (res == PixelCopy.SUCCESS) classifyRegions(bmpS) else "err$res"
                            bmpS.recycle()
                            synchronized(recordedFrameSummaries) {
                                if (slotIndex < recordedFrameSummaries.size) {
                                    recordedFrameSummaries[slotIndex] += "_S{$sStr}"
                                }
                            }
                            onPixelCopyFinished()
                        }, mainHandler)
                    } catch (e: Exception) {
                        bmpS.recycle()
                        onPixelCopyFinished()
                    }
                } else {
                    val bmpW = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
                    pendingPixelCopies++
                    try {
                        PixelCopy.request(act.window, bmpW, { res ->
                            val wStr = if (res == PixelCopy.SUCCESS) classifyRegions(bmpW) else "err$res"
                            bmpW.recycle()
                            synchronized(recordedFrameSummaries) {
                                if (slotIndex < recordedFrameSummaries.size) {
                                    recordedFrameSummaries[slotIndex] += "_W{$wStr}"
                                }
                            }
                            onPixelCopyFinished()
                        }, mainHandler)
                    } catch (e: Exception) {
                        bmpW.recycle()
                        onPixelCopyFinished()
                    }
                }

                if (childSv != null && childSv.width > 0 && childSv.height > 0 && childSv.holder.surface.isValid) {
                    val bmpSv = Bitmap.createBitmap(64, 64, Bitmap.Config.ARGB_8888)
                    pendingPixelCopies++
                    try {
                        PixelCopy.request(childSv, bmpSv, { res ->
                            val svStr = if (res == PixelCopy.SUCCESS) {
                                val c = bmpSv.getPixel(32, 32)
                                "rgb(${Color.red(c)},${Color.green(c)},${Color.blue(c)})"
                            } else {
                                "err$res"
                            }
                            bmpSv.recycle()
                            synchronized(recordedFrameSummaries) {
                                if (slotIndex < recordedFrameSummaries.size) {
                                    recordedFrameSummaries[slotIndex] += "_SV{$svStr}"
                                }
                            }
                            onPixelCopyFinished()
                        }, mainHandler)
                    } catch (e: Exception) {
                        bmpSv.recycle()
                        onPixelCopyFinished()
                    }
                }
            }
        }
    }

    private val displayListener =
        object : FlutterUiDisplayListener {
            override fun onFlutterUiDisplayed() {
                isFlutterUiDisplayed = true
                val pending = pendingDisplayResults.toList()
                pendingDisplayResults.clear()
                for (result in pending) {
                    result.success(null)
                }
            }

            override fun onFlutterUiNoLongerDisplayed() {
                isFlutterUiDisplayed = false
            }
        }

    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(binding.binaryMessenger, "native_driver")
        channel.setMethodCallHandler(this)
        flutterRenderer = binding.flutterEngine.renderer
        flutterRenderer?.addIsDisplayingFlutterUiListener(displayListener)
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        stopFrameRecordingInternal()
        pendingStopRecordingResult?.error("DETACHED", "Engine detached while recording frames", null)
        pendingStopRecordingResult = null
        channel.setMethodCallHandler(null)
        flutterRenderer?.removeIsDisplayingFlutterUiListener(displayListener)
        flutterRenderer = null
        isFlutterUiDisplayed = false
        for (result in pendingDisplayResults) {
            result.error("DETACHED", "Engine detached before frame was displayed", null)
        }
        pendingDisplayResults.clear()
    }

    override fun onMethodCall(
        call: MethodCall,
        result: MethodChannel.Result
    ) {
        if (call.method == "wait_for_first_frame_displayed") {
            if (isFlutterUiDisplayed || (flutterRenderer?.isDisplayingFlutterUi == true)) {
                isFlutterUiDisplayed = true
                result.success(null)
            } else {
                pendingDisplayResults.add(result)
            }
            return
        }

        val activity = this.activity
        if (activity == null) {
            Log.w(tag, "Received method channel, but no current activity")
            return
        }
        when (call.method) {
            "start_recording_frames" -> {
                stopFrameRecordingInternal()
                synchronized(recordedFrameSummaries) {
                    recordedFrameSummaries.clear()
                }
                frameCounter = 0
                pendingPixelCopies = 0
                recordingFrames = true
                Choreographer.getInstance().postFrameCallback(frameCallback)
                result.success(null)
            }
            "stop_recording_frames" -> {
                recordingFrames = false
                Choreographer.getInstance().removeFrameCallback(frameCallback)
                pendingStopRecordingResult = result
                if (pendingPixelCopies == 0) {
                    flushPendingStopRecordingResult()
                } else {
                    mainHandler.postDelayed(stopRecordingTimeoutRunnable, 500)
                }
            }
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
            else -> {
                result.notImplemented()
            }
        }
    }

    override fun onAttachedToActivity(binding: ActivityPluginBinding) {
        activity = binding.activity
    }

    override fun onDetachedFromActivityForConfigChanges() {
        stopFrameRecordingInternal()
        activity = null
    }

    override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
        activity = binding.activity
    }

    override fun onDetachedFromActivity() {
        stopFrameRecordingInternal()
        activity = null
    }
}
