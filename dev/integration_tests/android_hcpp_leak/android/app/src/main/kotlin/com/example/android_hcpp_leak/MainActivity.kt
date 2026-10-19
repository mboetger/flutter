package com.example.android_hcpp_leak

import android.content.Context
import android.os.Bundle
import android.os.Debug
import android.view.View
import android.widget.FrameLayout
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.StandardMessageCodec
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory

class MainActivity: FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        flutterEngine.platformViewsController.registry.registerViewFactory(
            "test_view",
            object : PlatformViewFactory(StandardMessageCodec.INSTANCE) {
                override fun create(context: Context, viewId: Int, args: Any?): PlatformView =
                    TestView(context)
            },
        )
        
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "meminfo").setMethodCallHandler { call, result ->
            if (call.method == "getGraphicsMemory") {
                val memInfo = Debug.MemoryInfo()
                Debug.getMemoryInfo(memInfo)
                val graphics = memInfo.getMemoryStat("summary.graphics")
                result.success(graphics?.toInt() ?: 0)
            } else {
                result.notImplemented()
            }
        }
    }
}

private class TestView(context: Context) : PlatformView {
    private val root = FrameLayout(context)

    override fun getView(): View = root

    override fun dispose() {}
}
