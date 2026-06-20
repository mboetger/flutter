// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import com.android.build.gradle.LibraryExtension
import com.android.build.gradle.AppExtension
import io.mockk.every
import io.mockk.spyk
import org.gradle.api.Action
import org.gradle.api.plugins.ObjectConfigurationAction
import org.gradle.api.Project
import org.gradle.api.Task
import org.gradle.api.internal.project.ProjectInternal
import org.gradle.testfixtures.ProjectBuilder
import org.junit.jupiter.api.io.TempDir
import java.nio.file.Path
import kotlin.io.path.createDirectories
import kotlin.io.path.writeText
import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.test.assertFalse

class FlutterPluginReproduceTest {

    @Test
    fun `reproduce debuggable release variant asset mismatch`(@TempDir tempDir: Path) {
        val projectDir = tempDir.resolve("project-dir")
        projectDir.createDirectories()

        val fakeFlutterSdkDir = tempDir.resolve("fake-flutter-sdk")
        fakeFlutterSdkDir.createDirectories()
        val fakeCacheDir = fakeFlutterSdkDir.resolve("bin").resolve("cache")
        fakeCacheDir.createDirectories()
        val fakeEngineStampFile = fakeCacheDir.resolve("engine.stamp")
        fakeEngineStampFile.writeText("901b0f1afe77c3555abee7b86a26aaa37f131379")
        val fakeEngineRealmFile = fakeCacheDir.resolve("engine.realm")
        fakeEngineRealmFile.writeText("")

        // Create parent project and subprojects
        val parent = ProjectBuilder.builder().withProjectDir(projectDir.toFile()).build()

        val flutterProject = ProjectBuilder.builder()
            .withParent(parent)
            .withName("flutter")
            .build()

        val appProject = ProjectBuilder.builder()
            .withParent(parent)
            .withName("app")
            .build()

        // Apply Android plugins
        flutterProject.plugins.apply("com.android.library")
        appProject.plugins.apply("com.android.application")

        // Configure Android Library project (:flutter)
        val flutterAndroid = flutterProject.extensions.getByType(LibraryExtension::class.java)
        flutterAndroid.compileSdkVersion(33)
        flutterAndroid.namespace = "com.example.flutter"
        flutterAndroid.defaultConfig {
            minSdkVersion(21)
        }

        // Configure Android Application project (:app)
        val appAndroid = appProject.extensions.getByType(AppExtension::class.java)
        appAndroid.compileSdkVersion(33)
        appAndroid.namespace = "com.example.app"
        appAndroid.defaultConfig {
            applicationId = "com.example.app"
            minSdkVersion(21)
        }

        // The critical setup: Make the host app's release build debuggable!
        appAndroid.buildTypes.getByName("release") {
            isDebuggable = true
        }

        // Set the flutter.sdk property on the library project so FlutterPlugin can find it
        flutterProject.extensions.extraProperties.set("flutter.sdk", fakeFlutterSdkDir.toAbsolutePath().toString())

        // Spy on the library project to stub the `apply` method, avoiding Kotlin DSL script execution issues in ProjectBuilder
        val spiedFlutterProject = spyk(flutterProject)
        every { spiedFlutterProject.apply(any<Action<ObjectConfigurationAction>>()) } returns Unit

        // Mock NativePluginLoaderReflectionBridge to avoid executing dangerous reflection that relies on applied scripts
        io.mockk.mockkObject(NativePluginLoaderReflectionBridge)
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns listOf()
        every { NativePluginLoaderReflectionBridge.getDependenciesMetadata(any(), any()) } returns mapOf()

        // Apply FlutterPlugin to the spied library project
        val flutterPlugin = FlutterPlugin()
        flutterPlugin.apply(spiedFlutterProject)

        // Evaluate both projects to trigger the afterEvaluate configuration blocks
        (flutterProject as ProjectInternal).evaluate()
        (appProject as ProjectInternal).evaluate()

        // Get the mergeReleaseAssets task from the host app
        val mergeReleaseAssets = appProject.tasks.getByName("mergeReleaseAssets")

        // Under the bug:
        // 1. The library's release variant (which is NOT debuggable, so mapped to "release" build mode)
        //    never matches the host app's release variant (which IS debuggable, so mapped to "debug" build mode).
        // 2. Therefore, "copyFlutterAssetsRelease" is NEVER created.
        // 3. Instead, the library's debug variant (mapped to "debug" build mode) matches the host app's release variant.
        // 4. Consequently, mergeReleaseAssets depends on "copyFlutterAssetsDebug".
        // This leads to a mismatch where a release build packages debug assets!

        val copyReleaseAssetsTask = flutterProject.tasks.findByName("copyFlutterAssetsRelease")
        val copyDebugAssetsTask = flutterProject.tasks.findByName("copyFlutterAssetsDebug")

        assertNotNull(copyReleaseAssetsTask, "Expected copyFlutterAssetsRelease to be created and used for debuggable release variant")

        val dependsOn = mergeReleaseAssets.dependsOn
        val dependsOnRelease = dependsOn.any { dep ->
            dep is Task && dep.name == "copyFlutterAssetsRelease"
        }
        val dependsOnDebug = dependsOn.any { dep ->
            dep is Task && dep.name == "copyFlutterAssetsDebug"
        }
        assertTrue(dependsOnRelease, "Expected mergeReleaseAssets to depend on copyFlutterAssetsRelease")
        assertFalse(dependsOnDebug, "Expected mergeReleaseAssets to NOT depend on copyFlutterAssetsDebug")
    }
}
