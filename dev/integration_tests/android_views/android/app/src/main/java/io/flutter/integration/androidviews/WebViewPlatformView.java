/*
 * Copyright 2014 The Flutter Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

package io.flutter.integration.platformviews;

import android.content.Context;
import android.view.View;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import io.flutter.plugin.common.BinaryMessenger;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.platform.PlatformView;

public class WebViewPlatformView implements PlatformView, MethodChannel.MethodCallHandler {
  private final WebView webView;
  private final MethodChannel methodChannel;

  WebViewPlatformView(Context context, BinaryMessenger messenger, int id) {
    methodChannel = new MethodChannel(messenger, "platform_views_integration/webview_" + id);
    methodChannel.setMethodCallHandler(this);

    webView = new WebView(context);
    webView.setFocusable(true);
    webView.setFocusableInTouchMode(true);
    webView.getSettings().setJavaScriptEnabled(true);
    webView.setWebViewClient(
        new WebViewClient() {
          @Override
          public void onPageFinished(WebView view, String url) {
            methodChannel.invokeMethod("onPageFinished", null);
          }
        });

    webView.addJavascriptInterface(
        new Object() {
          @JavascriptInterface
          public void postMessage(String message) {
            webView.post(
                new Runnable() {
                  @Override
                  public void run() {
                    methodChannel.invokeMethod("onTextChange", message);
                  }
                });
          }

          @JavascriptInterface
          public void onFocus() {
            webView.post(
                new Runnable() {
                  @Override
                  public void run() {
                    methodChannel.invokeMethod("onFocus", null);
                  }
                });
          }
        },
        "FlutterTest");

    String html =
        "<!DOCTYPE html>"
            + "<html>"
            + "<head>"
            + "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            + "<style>"
            + "  html, body { height: 100%; margin: 0; padding: 0; }"
            + "  #text-input { width: 100%; height: 100%; border: none; font-size: 30px;"
            + " box-sizing: border-box; }"
            + "</style>"
            + "</head>"
            + "<body>"
            + "<input type='text' id='text-input' oninput='FlutterTest.postMessage(this.value)' onfocus='FlutterTest.onFocus()'>"
            + "</body>"
            + "</html>";
    webView.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null);
  }

  @Override
  public View getView() {
    return webView;
  }

  @Override
  public void dispose() {
    methodChannel.setMethodCallHandler(null);
    webView.removeJavascriptInterface("FlutterTest");
    webView.destroy();
  }

  @Override
  public void onMethodCall(MethodCall methodCall, MethodChannel.Result result) {
    if (methodCall.method.equals("focus")) {
      webView.requestFocus();
      webView.evaluateJavascript("document.getElementById('text-input').focus();", null);
      result.success(null);
    } else {
      result.notImplemented();
    }
  }
}
