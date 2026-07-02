# Triage Notes - Issue 48208

The issue reports that `getExternalStorageDirectory()` on Android returns the app-specific directory instead of the root of external storage.

This is expected behavior on modern Android versions due to Scoped Storage restrictions. Direct access to the root of external storage is restricted.

The documentation for `getExternalStorageDirectory()` in `path_provider` has been updated to clarify this behavior and guide developers on how to save media files to public galleries.

Updated file: `engine/src/flutter/third_party/pkg/flutter_packages/packages/path_provider/path_provider/lib/path_provider.dart`
