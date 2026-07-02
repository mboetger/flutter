// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@file:Suppress("PackageName")

package com.example.android_engine_test.fixtures

import android.content.Context
import android.graphics.Color
import android.graphics.Rect
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.view.View
import android.view.ViewGroup
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class ContinuousGlSurfaceViewPlatformViewFactory : PlatformViewFactory(null) {
    override fun create(
        context: Context,
        viewId: Int,
        args: Any?
    ): PlatformView = ContinuousGlSurfaceViewPlatformView(context)
}

private class ContinuousGlRenderer : GLSurfaceView.Renderer {
    private var width = 0
    private var height = 0
    private var frame = 0

    override fun onSurfaceCreated(unused: GL10?, config: EGLConfig?) {
        GLES20.glEnable(GLES20.GL_SCISSOR_TEST)
    }

    override fun onSurfaceChanged(unused: GL10?, width: Int, height: Int) {
        this.width = width
        this.height = height
    }

    private fun drawRect(r: Rect, c: Int) {
        GLES20.glViewport(r.left, r.top, r.width(), r.height())
        GLES20.glScissor(r.left, r.top, r.width(), r.height())
        GLES20.glClearColor(Color.red(c) / 255f, Color.green(c) / 255f, Color.blue(c) / 255f, 1.0f)
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)
    }

    override fun onDrawFrame(unused: GL10?) {
        drawRect(Rect(0, 0, width, height), Color.WHITE)
        if (width > 0) {
            drawRect(Rect(frame % width, 0, (frame % width) + 10, height), Color.BLACK)
        }
        drawRect(Rect(100, 100, 300, 300), if (frame % 2 == 0) Color.RED else Color.BLUE)
        frame++
    }
}

private class ContinuousGlSurfaceViewPlatformView(
    context: Context
) : PlatformView {
    private val view: GLSurfaceView = GLSurfaceView(context)

    init {
        view.layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        )
        view.setEGLContextClientVersion(2)
        view.setEGLConfigChooser(8, 8, 8, 0, 16, 0)
        view.setRenderer(ContinuousGlRenderer())
        view.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
    }

    override fun getView(): View = view

    override fun dispose() {
        view.onPause()
    }
}
