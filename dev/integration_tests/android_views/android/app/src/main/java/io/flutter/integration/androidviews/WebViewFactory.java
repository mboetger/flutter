// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.integration.platformviews;

import android.content.Context;
import io.flutter.plugin.common.BinaryMessenger;
import io.flutter.plugin.common.StandardMessageCodec;
import io.flutter.plugin.platform.PlatformView;
import io.flutter.plugin.platform.PlatformViewFactory;

public class WebViewFactory extends PlatformViewFactory {
  private final BinaryMessenger messenger;

  public WebViewFactory(BinaryMessenger messenger) {
    super(StandardMessageCodec.INSTANCE);
    this.messenger = messenger;
  }

  @Override
  public PlatformView create(Context context, int id, Object params) {
    return new WebViewPlatformView(context, messenger, id);
  }
}
