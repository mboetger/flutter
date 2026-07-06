# Build the ephemeral app in a module project.
# Prevents: Warning: library class <plugin-package> depends on program class io.flutter.plugin.**
# This is due to plugins (libraries) depending on the embedding (the program jar)
-dontwarn io.flutter.plugin.**

# The android.** package is provided by the OS at runtime.
-dontwarn android.**

# In some cases, R8 is incorrectly stripping plugin classes. Keep
# all implementations of FlutterPlugin until we can determine
# why this is the case.
# See https://github.com/flutter/flutter/issues/154580.
-if class * implements io.flutter.embedding.engine.plugins.FlutterPlugin
-keep,allowshrinking,allowobfuscation class <1>

# Keep ContentProvider and FileProvider implementations referenced in AndroidManifest.xml
# so that ProGuard/R8 does not strip them in release builds.
# See https://github.com/flutter/flutter/issues/74841.
-keep public class * extends android.content.ContentProvider
-keep public class * extends androidx.core.content.FileProvider

