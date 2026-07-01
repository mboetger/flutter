// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

val flutterSdkPath = if (project.extra.has("flutterSdkPath")) {
    project.extra.get("flutterSdkPath") as String
} else {
    System.getenv("FLUTTER_ROOT")
}

if (flutterSdkPath == null) {
    logger.warn("Flutter SDK not found. Cannot configure Standalone Flutter Android plugin dependencies.")
} else {
    var engineVersion = "+"
    val engineStampFile = project.file("$flutterSdkPath/bin/cache/engine.stamp")
    if (engineStampFile.exists()) {
        engineVersion = "1.0.0-" + engineStampFile.readText().trim()
    }

    var engineRealm = ""
    val engineRealmFile = project.file("$flutterSdkPath/bin/cache/engine.realm")
    if (engineRealmFile.exists()) {
        engineRealm = engineRealmFile.readText().trim()
    }
    if (engineRealm.isNotEmpty()) {
        engineRealm = "$engineRealm/"
    }

    val hostedRepository = System.getenv("FLUTTER_STORAGE_BASE_URL") ?: "https://storage.googleapis.com"
    val repository = "$hostedRepository/${engineRealm}download.flutter.io"

    repositories {
        maven {
            url = uri(repository)
        }
    }

    dependencies {
        add("compileOnly", "io.flutter:flutter_embedding_debug:$engineVersion")
    }
}
