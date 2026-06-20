// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import java.io.File;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.junit.Test;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

public class AndroidManifestReproduceTest {
  @Test
  public void testManifestDoesNotRequestWriteExternalStoragePermission() {
    // The working directory of the test execution is the test_runner directory.
    // The production AndroidManifest.xml is located in the parent directory.
    File manifestFile = new File("../AndroidManifest.xml");
    if (!manifestFile.exists()) {
      fail("AndroidManifest.xml not found at " + manifestFile.getAbsolutePath());
    }

    try {
      DocumentBuilderFactory dbFactory = DocumentBuilderFactory.newInstance();
      DocumentBuilder dBuilder = dbFactory.newDocumentBuilder();
      Document doc = dBuilder.parse(manifestFile);
      doc.getDocumentElement().normalize();

      NodeList usesPermissions = doc.getElementsByTagName("uses-permission");
      for (int i = 0; i < usesPermissions.getLength(); i++) {
        Element permissionElement = (Element) usesPermissions.item(i);
        String permissionName = permissionElement.getAttribute("android:name");
        assertFalse(
            "The Android embedding manifest should not request WRITE_EXTERNAL_STORAGE permission.",
            "android.permission.WRITE_EXTERNAL_STORAGE".equals(permissionName));
      }
    } catch (Exception e) {
      fail("Failed to parse AndroidManifest.xml: " + e.getMessage());
    }
  }
}
