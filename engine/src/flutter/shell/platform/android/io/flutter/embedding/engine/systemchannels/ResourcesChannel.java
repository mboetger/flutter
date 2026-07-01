// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import android.content.Context;
import android.content.res.Resources;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import io.flutter.Log;
import io.flutter.plugin.common.BinaryMessenger;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * System channel that receives requests to load Android platform resources.
 */
public class ResourcesChannel {
  private static final String TAG = "ResourcesChannel";

  @NonNull private final Context context;

  public ResourcesChannel(@NonNull BinaryMessenger messenger, @NonNull Context context) {
    this.context = context;
    messenger.setMessageHandler("flutter/resources", handler);
  }

  @NonNull
  private final BinaryMessenger.BinaryMessageHandler handler =
      new BinaryMessenger.BinaryMessageHandler() {
        @Override
        public void onMessage(@Nullable ByteBuffer message, @NonNull BinaryMessenger.BinaryReply reply) {
          if (message == null) {
            reply.reply(null);
            return;
          }
          String key = StandardCharsets.UTF_8.decode(message).toString();
          byte[] data = loadResource(key);
          if (data != null) {
            reply.reply(ByteBuffer.wrap(data));
          } else {
            reply.reply(null);
          }
        }
      };

  @Nullable
  private byte[] loadResource(@NonNull String key) {
    if (!key.startsWith("res/")) {
      return null;
    }
    String[] parts = key.split("/");
    if (parts.length != 3) {
      return null;
    }
    String type = parts[1];
    String nameWithExt = parts[2];
    
    // Strip everything starting from the first dot to support 9-patch images and other extensions
    int dotIndex = nameWithExt.indexOf('.');
    String name = dotIndex >= 0 ? nameWithExt.substring(0, dotIndex) : nameWithExt;

    Resources resources = context.getResources();
    int id = resources.getIdentifier(name, type, context.getPackageName());
    if (id == 0) {
      return null;
    }

    try (InputStream is = resources.openRawResource(id);
         ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
      byte[] buffer = new byte[4096];
      int read;
      while ((read = is.read(buffer)) != -1) {
        bos.write(buffer, 0, read);
      }
      return bos.toByteArray();
    } catch (IOException | Resources.NotFoundException e) {
      Log.e(TAG, "Failed to load resource: " + key, e);
      return null;
    }
  }
}
