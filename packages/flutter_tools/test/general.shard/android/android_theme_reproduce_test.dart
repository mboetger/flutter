// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/local.dart';
import 'package:flutter_tools/src/cache.dart';
import '../../src/common.dart';

void main() {
  testWithoutContext('Android templates use modern themes and support status bar styling', () {
    const FileSystem fs = LocalFileSystem();

    // Resolve the path to the templates using Cache.flutterRoot with a fallback to getFlutterRoot()
    final String flutterRoot = Cache.flutterRoot ?? getFlutterRoot();
    final Directory templatesDir = fs.directory(
      fs.path.join(flutterRoot, 'packages', 'flutter_tools', 'templates'),
    );

    final Directory appTemplatesDir = templatesDir.childDirectory('app/android.tmpl');
    if (!appTemplatesDir.existsSync()) {
      fail('App templates directory not found at ${appTemplatesDir.absolute.path}.');
    }

    final Directory resDir = appTemplatesDir.childDirectory('app/src/main/res');
    final File stylesFile = resDir.childDirectory('values').childFile('styles.xml');
    final File nightStylesFile = resDir.childDirectory('values-night').childFile('styles.xml');

    expect(stylesFile.existsSync(), isTrue);
    expect(nightStylesFile.existsSync(), isTrue);

    final String stylesContent = stylesFile.readAsStringSync();
    final String nightStylesContent = nightStylesFile.readAsStringSync();

    // Assert that LaunchTheme uses a modern parent theme (e.g., Theme.Material.Light.NoActionBar)
    // instead of the old Theme.Light.NoTitleBar.
    expect(stylesContent, contains('parent="@android:style/Theme.Material.Light.NoActionBar"'));

    // Assert that NormalTheme also uses a modern parent theme
    expect(
      stylesContent,
      contains(
        '<style name="NormalTheme" parent="@android:style/Theme.Material.Light.NoActionBar"',
      ),
    );

    // Assert that both themes in light mode have statusBarColor set to transparent
    expect(
      RegExp(
        '<item name="android:statusBarColor">@android:color/transparent</item>',
      ).allMatches(stylesContent).length,
      2,
    );

    // Assert that both themes in light mode have windowLightStatusBar set to true
    expect(
      RegExp(
        '<item name="android:windowLightStatusBar">true</item>',
      ).allMatches(stylesContent).length,
      2,
    );

    // Assert the same for night styles
    expect(nightStylesContent, contains('parent="@android:style/Theme.Material.NoActionBar"'));
    expect(
      nightStylesContent,
      contains('<style name="NormalTheme" parent="@android:style/Theme.Material.NoActionBar"'),
    );
    expect(
      RegExp(
        '<item name="android:statusBarColor">@android:color/transparent</item>',
      ).allMatches(nightStylesContent).length,
      2,
    );

    // Assert that night styles do not contain windowLightStatusBar
    expect(nightStylesContent, isNot(contains('android:windowLightStatusBar')));

    // Verify module template
    final Directory moduleTemplatesDir = templatesDir.childDirectory(
      'module/android/host_app_common/app.tmpl',
    );
    if (!moduleTemplatesDir.existsSync()) {
      fail('Module templates directory not found at ${moduleTemplatesDir.absolute.path}.');
    }
    final File moduleStylesFile = moduleTemplatesDir
        .childDirectory('src/main/res/values')
        .childFile('styles.xml');
    expect(moduleStylesFile.existsSync(), isTrue);
    final String moduleStylesContent = moduleStylesFile.readAsStringSync();

    expect(moduleStylesContent, contains('parent="@android:style/Theme.Material.NoActionBar"'));
    expect(
      moduleStylesContent,
      contains('<item name="android:statusBarColor">@android:color/transparent</item>'),
    );
    expect(moduleStylesContent, isNot(contains('android:windowLightStatusBar')));
  });
}
