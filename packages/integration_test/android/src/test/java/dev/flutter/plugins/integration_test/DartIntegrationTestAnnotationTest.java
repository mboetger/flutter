// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.flutter.plugins.integration_test;

import org.junit.Test;
import static org.junit.Assert.assertNotNull;

@DartIntegrationTest
public class DartIntegrationTestAnnotationTest {
    @Test
    public void testAnnotationExists() {
        assertNotNull(DartIntegrationTest.class);
    }
}
