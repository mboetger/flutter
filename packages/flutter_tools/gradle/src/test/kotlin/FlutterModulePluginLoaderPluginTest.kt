// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.unmockkAll
import io.mockk.verify
import io.mockk.slot
import org.gradle.api.Action
import org.gradle.api.Project
import org.gradle.api.plugins.ObjectConfigurationAction
import org.gradle.api.initialization.ProjectDescriptor
import org.gradle.api.initialization.Settings
import org.gradle.api.invocation.Gradle
import org.gradle.api.plugins.ExtraPropertiesExtension
import org.gradle.api.file.DirectoryProperty
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.nio.file.Path
import kotlin.test.Test
import kotlin.test.assertNotNull

class FlutterModulePluginLoaderPluginTest {

    @AfterEach
    fun tearDown() {
        unmockkAll()
    }

    @Test
    fun `FlutterModulePluginLoaderPlugin applies native plugin loader and configures projects`(
        @TempDir tempDir: Path
    ) {
        val projectDir = tempDir.resolve("project-dir")
        projectDir.toFile().mkdirs()
        
        // 1. Set up the expected directory structure for a Flutter module
        val androidDir = projectDir.resolve(".android")
        androidDir.toFile().mkdirs()
        val flutterProjectDir = androidDir.resolve("Flutter")
        flutterProjectDir.toFile().mkdirs()
        
        // Write a fake local.properties inside .android/
        val localPropertiesFile = androidDir.resolve("local.properties")
        val fakeFlutterSdkDir = tempDir.resolve("fake-flutter-sdk")
        fakeFlutterSdkDir.toFile().mkdirs()
        localPropertiesFile.toFile().writeText("flutter.sdk=${fakeFlutterSdkDir.toAbsolutePath()}")

        // 2. Mock Gradle Settings and ProjectDescriptor
        val settings = mockk<Settings>(relaxed = true)
        val extraProperties = mockk<ExtraPropertiesExtension>(relaxed = true)
        every { settings.extraProperties } returns extraProperties
        every { extraProperties.has("flutterSdkPath") } returns false
        every { extraProperties.get("flutterSdkPath") } returns null
        
        val gradleMock = mockk<Gradle>(relaxed = true)
        every { settings.gradle } returns gradleMock
        
        val flutterProjectDescriptor = mockk<ProjectDescriptor>(relaxed = true)
        every { flutterProjectDescriptor.projectDir } returns flutterProjectDir.toFile()
        every { settings.findProject(":flutter") } returns flutterProjectDescriptor
        
        // 3. Mock NativePluginLoaderReflectionBridge to return a fake plugin list
        mockkObject(NativePluginLoaderReflectionBridge)
        val fakePlugins: List<Map<String?, Any?>> = listOf(
            mapOf(
                "name" to "plugin_a", 
                "path" to tempDir.resolve("plugin_a").toAbsolutePath().toString()
            )
        )
        // Ensure the plugin directory and its android subfolder exist
        tempDir.resolve("plugin_a").resolve("android").toFile().mkdirs()
        
        // The reflection bridge will be called with the module root directory (projectDir)
        every { 
            NativePluginLoaderReflectionBridge.getPlugins(extraProperties, projectDir.toFile()) 
        } returns fakePlugins

        // 4. Instantiate and apply the plugin
        val plugin = FlutterModulePluginLoaderPlugin()
        plugin.apply(settings)

        // 5. Verify the native plugin loader script was applied
        val applyActionSlot = slot<Action<ObjectConfigurationAction>>()
        verify {
            settings.apply(capture(applyActionSlot))
        }
        
        // 6. Verify that the plugin project was included and its path configured
        verify {
            settings.include(":plugin_a")
            settings.project(":plugin_a").projectDir = tempDir.resolve("plugin_a").resolve("android").toFile()
        }

        // 7. Verify and capture Gradle projectsLoaded lifecycle hook
        val projectsLoadedActionSlot = slot<Action<Gradle>>()
        verify {
            gradleMock.projectsLoaded(capture(projectsLoadedActionSlot))
        }

        // 8. Trigger projectsLoaded and capture beforeEvaluate/afterEvaluate
        val rootProjectMock = mockk<Project>(relaxed = true)
        val rootProjectExtraProperties = mockk<ExtraPropertiesExtension>(relaxed = true)
        every { rootProjectMock.extensions.extraProperties } returns rootProjectExtraProperties
        every { gradleMock.rootProject } returns rootProjectMock
        
        val beforeEvaluateActionSlot = slot<Action<Project>>()
        val afterEvaluateActionSlot = slot<Action<Project>>()
        
        projectsLoadedActionSlot.captured.execute(gradleMock)
        
        verify {
            rootProjectMock.beforeEvaluate(capture(beforeEvaluateActionSlot))
            rootProjectMock.afterEvaluate(capture(afterEvaluateActionSlot))
        }

        // 9. Trigger beforeEvaluate and verify subproject build directory redirect
        val subprojectMock = mockk<Project>(relaxed = true)
        every { subprojectMock.name } returns "plugin_a"
        val mockSubprojects = mutableSetOf(subprojectMock)
        
        val subprojectsActionSlot = slot<Action<Project>>()
        every { rootProjectMock.subprojects(capture(subprojectsActionSlot)) } answers {
            mockSubprojects.forEach { subprojectsActionSlot.captured.execute(it) }
        }
        
        val directoryPropertyMock = mockk<DirectoryProperty>(relaxed = true)
        every { subprojectMock.layout.buildDirectory } returns directoryPropertyMock

        // Mock mainModuleName setting in settings extra properties
        every { extraProperties.has("mainModuleName") } returns true
        every { extraProperties.get("mainModuleName") } returns "app"

        beforeEvaluateActionSlot.captured.execute(rootProjectMock)

        verify {
            // Verify build directory is redirected to plugins_build_output/plugin_a
            directoryPropertyMock.fileValue(any<File>())
            // Verify mainModuleName propagation to root project
            rootProjectExtraProperties.set("mainModuleName", "app")
        }

        // 10. Trigger afterEvaluate and verify evaluation dependency
        val otherSubprojectMock = mockk<Project>(relaxed = true)
        every { otherSubprojectMock.name } returns "some_other_project"
        val mockSubprojectsAfter = mutableSetOf(otherSubprojectMock)
        
        val subprojectsActionSlotAfter = slot<Action<Project>>()
        every { rootProjectMock.subprojects(capture(subprojectsActionSlotAfter)) } answers {
            mockSubprojectsAfter.forEach { subprojectsActionSlotAfter.captured.execute(it) }
        }

        afterEvaluateActionSlot.captured.execute(rootProjectMock)

        verify {
            otherSubprojectMock.evaluationDependsOn(":flutter")
        }
    }
}
