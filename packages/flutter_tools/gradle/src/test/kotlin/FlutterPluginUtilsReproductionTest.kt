// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import com.android.builder.model.BuildType
import io.mockk.every
import io.mockk.mockk
import org.gradle.api.Project
import kotlin.test.Test
import kotlin.test.assertEquals

class FlutterPluginUtilsReproductionTest {
    private fun mockProjectWithProperty(propertyValue: String?): Project {
        val project = mockk<Project>()
        if (propertyValue != null) {
            every { project.hasProperty("flutter-build-mode") } returns true
            every { project.property("flutter-build-mode") } returns propertyValue
            every { project.findProperty("flutter-build-mode") } returns propertyValue
        } else {
            every { project.hasProperty("flutter-build-mode") } returns false
            every { project.findProperty("flutter-build-mode") } returns null
        }
        return project
    }

    @Test
    fun `buildModeFor with project returns overridden build mode profile when project property is set`() {
        val project = mockProjectWithProperty("profile")
        val buildType = mockk<BuildType>()
        every { buildType.name } returns "debug"
        every { buildType.isDebuggable } returns true

        // This should return "profile" because of the property override, even though buildType is debuggable.
        val result = FlutterPluginUtils.buildModeFor(project, buildType)
        assertEquals("profile", result)
    }

    @Test
    fun `buildModeFor with project returns overridden build mode release when project property is set`() {
        val project = mockProjectWithProperty("release")
        val buildType = mockk<BuildType>()
        every { buildType.name } returns "debug"
        every { buildType.isDebuggable } returns true

        // This should return "release" because of the property override, even though buildType is debuggable.
        val result = FlutterPluginUtils.buildModeFor(project, buildType)
        assertEquals("release", result)
    }

    @Test
    fun `buildModeFor with project returns overridden build mode debug when project property is set and buildType is release`() {
        val project = mockProjectWithProperty("debug")
        val buildType = mockk<BuildType>()
        every { buildType.name } returns "release"
        every { buildType.isDebuggable } returns false

        // This should return "debug" because of the property override, even though buildType is release/non-debuggable.
        val result = FlutterPluginUtils.buildModeFor(project, buildType)
        assertEquals("debug", result)
    }

    @Test
    fun `buildModeFor with project returns default build mode when project property is not set`() {
        val project = mockProjectWithProperty(null)
        val buildType = mockk<BuildType>()
        every { buildType.name } returns "debug"
        every { buildType.isDebuggable } returns true

        // This should return "debug" since the property is not set and buildType is debuggable.
        val result = FlutterPluginUtils.buildModeFor(project, buildType)
        assertEquals("debug", result)
    }

    @Test
    fun `buildModeFor with strict mock project throws and falls back to default build mode`() {
        // A strict mockk has no stubbed behaviors for hasProperty/findProperty
        val project = mockk<Project>() 
        val buildType = mockk<BuildType>()
        every { buildType.name } returns "release"
        every { buildType.isDebuggable } returns false

        // This should not throw MockKException, but gracefully catch it and return "release"
        val result = FlutterPluginUtils.buildModeFor(project, buildType)
        assertEquals("release", result)
    }
}
