// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@file:Suppress("PackageName")

package com.example.android_engine_test.fixtures

import android.graphics.Color
import android.graphics.Paint
import android.os.Build
import android.view.Surface
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.FlutterPlugin.FlutterPluginBinding
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.view.TextureRegistry.SurfaceTextureEntry

class OtherFaceTexturePlugin : FlutterPlugin {
    private lateinit var channel: MethodChannel
    private lateinit var surfaceTextureEntry: SurfaceTextureEntry
    private var handler: OtherFaceTexturePluginHandler? = null

    override fun onAttachedToEngine(binding: FlutterPluginBinding) {
        channel = MethodChannel(binding.binaryMessenger, "other_face_texture")
        surfaceTextureEntry = binding.textureRegistry.createSurfaceTexture()
        handler = OtherFaceTexturePluginHandler(surfaceTextureEntry)
        channel.setMethodCallHandler(handler)
    }

    override fun onDetachedFromEngine(binding: FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
        handler?.release()
        handler = null
        if (::surfaceTextureEntry.isInitialized) {
            surfaceTextureEntry.release()
        }
    }
}

class OtherFaceTexturePluginHandler(private val surfaceTextureEntry: SurfaceTextureEntry) : MethodCallHandler {
    private var surface: Surface? = null

    override fun onMethodCall(
        call: MethodCall,
        result: MethodChannel.Result
    ) {
        if (call.method == "initTexture") {
            val height = call.argument<Int>("height") ?: 1
            val width = call.argument<Int>("width") ?: 1
            surfaceTextureEntry.surfaceTexture().setDefaultBufferSize(width, height)
            result.success(updateTexture())
        } else {
            result.notImplemented()
        }
    }

    private fun updateTexture(): Long {
        val surface = this.surface ?: Surface(surfaceTextureEntry.surfaceTexture()).also { this.surface = it }
        drawOnSurface(surface)
        return surfaceTextureEntry.id()
    }

    private fun drawOnSurface(surface: Surface) {
        val canvas =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                surface.lockHardwareCanvas()
            } else {
                surface.lockCanvas(null)
            }
        try {
            // Yellow background
            canvas.drawRGB(255, 230, 15)

            val paint = Paint()
            paint.style = Paint.Style.FILL

            // Black eyes
            paint.color = Color.BLACK
            canvas.drawCircle(225f, 225f, 25f, paint) // Left eye
            canvas.drawCircle(425f, 225f, 25f, paint) // Right eye

            // Black mouth
            paint.color = Color.BLACK
            canvas.drawCircle(300f, 300f, 50f, paint) // Simple mouth
        } finally {
            surface.unlockCanvasAndPost(canvas)
        }
    }

    fun release() {
        surface?.release()
        surface = null
    }
}
