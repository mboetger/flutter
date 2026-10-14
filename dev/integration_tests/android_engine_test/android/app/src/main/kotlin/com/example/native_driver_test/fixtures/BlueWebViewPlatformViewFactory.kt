// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@file:Suppress("PackageName")

package com.example.android_engine_test.fixtures

import android.content.Context
import android.graphics.Color
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory

class BlueWebViewPlatformViewFactory : PlatformViewFactory(null) {
    override fun create(
        context: Context,
        viewId: Int,
        args: Any?
    ): PlatformView = BlueWebViewPlatformView(context)
}

private class BlueWebViewPlatformView(
    context: Context
) : PlatformView {
    private val webView = WebView(context)

    init {
        webView.layoutParams =
            ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        webView.setBackgroundColor(Color.BLUE)
        webView.loadData(
            "<html><body style='margin:0;background:#0000FF;'></body></html>",
            "text/html",
            "utf-8"
        )
    }

    override fun getView(): View = webView

    override fun dispose() {
        webView.destroy()
    }
}
