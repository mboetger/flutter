// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.plugins

import com.android.build.gradle.BaseExtension
import com.flutter.gradle.FlutterExtension
import com.flutter.gradle.FlutterPluginUtils
import com.flutter.gradle.FlutterPluginUtilsTest.Companion.EXAMPLE_ENGINE_VERSION
import com.flutter.gradle.FlutterPluginUtilsTest.Companion.cameraDependency
import com.flutter.gradle.FlutterPluginUtilsTest.Companion.flutterPluginAndroidLifecycleDependency
import com.flutter.gradle.FlutterPluginUtilsTest.Companion.pluginListWithDevDependency
import com.flutter.gradle.FlutterPluginUtilsTest.Companion.pluginListWithoutDevDependency
import com.flutter.gradle.NativePluginLoaderReflectionBridge
import io.mockk.called
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.slot
import io.mockk.verify
import org.gradle.api.Action
import org.gradle.api.NamedDomainObjectContainer
import org.gradle.api.Project
import org.gradle.api.logging.Logger
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.assertThrows
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.nio.file.Path
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import io.mockk.unmockkAll

class PluginHandlerTest {
    @AfterEach
    fun tearDown() {
        unmockkAll()
    }

    // getPluginListWithoutDevDependencies
    @Test
    fun `getPluginListWithoutDevDependencies removes dev dependencies from list`() {
        val project = mockk<Project>()
        val pluginHandler = PluginHandler(project)
        mockkObject(NativePluginLoaderReflectionBridge)
        // mock return of NativePluginLoaderReflectionBridge.getPlugins
        every {
            NativePluginLoaderReflectionBridge.getPlugins(
                any(),
                any()
            )
        } returns pluginListWithDevDependency
        // mock method calls that are invoked by the args to NativePluginLoaderReflectionBridge
        every { project.extraProperties } returns mockk()
        every { project.extensions.findByType(FlutterExtension::class.java) } returns FlutterExtension()
        every { project.file(any()) } returns mockk()

        val result = pluginHandler.getPluginListWithoutDevDependencies()
        assertEquals(pluginListWithoutDevDependency, result)
    }

    @Test
    fun `getPluginListWithoutDevDependencies does not modify list without dev dependencies`() {
        val project = mockk<Project>()
        val pluginHandler = PluginHandler(project)
        mockkObject(NativePluginLoaderReflectionBridge)
        // mock return of NativePluginLoaderReflectionBridge.getPlugins
        every {
            NativePluginLoaderReflectionBridge.getPlugins(
                any(),
                any()
            )
        } returns pluginListWithoutDevDependency
        // mock method calls that are invoked by the args to NativePluginLoaderReflectionBridge
        every { project.extraProperties } returns mockk()
        every { project.extensions.findByType(FlutterExtension::class.java) } returns FlutterExtension()
        every { project.file(any()) } returns mockk()

        val result = pluginHandler.getPluginListWithoutDevDependencies()
        assertEquals(pluginListWithoutDevDependency, result)
    }

    // getPluginList skipped as it is a wrapper around a single reflection call

    // pluginSupportsAndroidPlatform
    @Test
    fun `pluginSupportsAndroidPlatform returns true when android directory exists with gradle build file`(
        @TempDir tempDir: Path
    ) {
        val projectDir = tempDir.resolve("my-plugin")
        projectDir.toFile().mkdirs()

        val androidDir = tempDir.resolve("android")
        androidDir.toFile().mkdirs()
        File(androidDir.toFile(), "build.gradle").createNewFile()

        val mockProject =
            mockk<Project> {
                every { this@mockk.projectDir } returns projectDir.toFile()
            }

        assertTrue {
            PluginHandler.pluginSupportsAndroidPlatform(mockProject)
        } // Replace YourClass with the actual class containing the method
    }

    @Test
    fun `pluginSupportsAndroidPlatform returns false when gradle build file does not exist`(
        @TempDir tempDir: Path
    ) {
        val projectDir = tempDir.resolve("my-plugin")
        projectDir.toFile().mkdirs()

        val mockProject =
            mockk<Project> {
                every { this@mockk.projectDir } returns projectDir.toFile()
            }

        assertFalse {
            PluginHandler.pluginSupportsAndroidPlatform(mockProject)
        }
    }

    @Test
    fun `configurePlugins throws IllegalArgumentException when plugin has no name`(
        @TempDir tempDir: Path
    ) {
        val project = mockk<Project>()

        // configuration for configureLegacyPluginEachProjects
        val projectDir = tempDir.resolve("my-plugin")
        projectDir.toFile().mkdirs()
        every { project.projectDir } returns projectDir.toFile()
        val settingsGradle = File(projectDir.parent.toFile(), "settings.gradle")
        settingsGradle.createNewFile()
        val mockLogger = mockk<Logger>()
        every { project.logger } returns mockLogger

        val pluginWithoutName: MutableMap<String?, Any?> = cameraDependency.toMutableMap()
        pluginWithoutName.remove("name")

        mockkObject(NativePluginLoaderReflectionBridge)
        // mock return of NativePluginLoaderReflectionBridge.getPlugins
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns
            listOf(
                pluginWithoutName
            )
        // mock method calls that are invoked by the args to NativePluginLoaderReflectionBridge
        every { project.extraProperties } returns mockk()
        every { project.extensions.findByType(FlutterExtension::class.java) } returns FlutterExtension()
        every { project.file(any()) } returns mockk()

        val pluginHandler = PluginHandler(project)
        assertThrows<IllegalArgumentException> {
            pluginHandler.configurePlugins(
                engineVersionValue = EXAMPLE_ENGINE_VERSION
            )
        }
    }

    @Test
    fun `configurePlugins adds plugin project and configures its dependencies`(
        @TempDir tempDir: Path
    ) {
        val project = mockk<Project>(relaxed = true)

        // configuration for configureLegacyPluginEachProjects
        val projectDir = tempDir.resolve("my-plugin")
        projectDir.toFile().mkdirs()
        every { project.projectDir } returns projectDir.toFile()
        val settingsGradle = File(projectDir.parent.toFile(), "settings.gradle")
        settingsGradle.createNewFile()
        val mockLogger = mockk<Logger>()
        every { project.logger } returns mockLogger

        val pluginProject = mockk<Project>(relaxed = true)
        val pluginDependencyProject = mockk<Project>(relaxed = true)
        val mockBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>()
        every { pluginProject.hasProperty("local-engine-repo") } returns false
        every { pluginProject.hasProperty("android") } returns true
        val mockPluginContainer = mockk<org.gradle.api.plugins.PluginContainer>()
        every { pluginProject.plugins } returns mockPluginContainer
        every { mockPluginContainer.hasPlugin("com.android.application") } returns false
        every { mockBuildType.name } returns "debug"
        every { mockBuildType.isDebuggable } returns true
        every { project.rootProject.findProject(":${cameraDependency["name"]}") } returns pluginProject
        every { project.rootProject.findProject(":${flutterPluginAndroidLifecycleDependency["name"]}") } returns pluginDependencyProject
        every { pluginProject.extensions.create(any(), any<Class<Any>>()) } returns mockk()
        val captureActionSlot = slot<Action<Project>>()
        val capturePluginActionSlot = mutableListOf<Action<Project>>()
        every { project.afterEvaluate(any<Action<Project>>()) } returns Unit
        every { pluginProject.afterEvaluate(any<Action<Project>>()) } returns Unit

        val mockProjectBuildTypes =
            mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        val mockPluginProjectBuildTypes =
            mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        every { project.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockProjectBuildTypes
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockPluginProjectBuildTypes
        every { mockPluginProjectBuildTypes.addAll(any()) } returns true
        every { pluginProject.configurations.named(any<String>()) } returns mockk()
        every { pluginProject.dependencies.add(any(), any()) } returns mockk()

        every {
            project.extensions
                .findByType(BaseExtension::class.java)!!
                .buildTypes
                .iterator()
        } returns
            mutableListOf(
                mockBuildType
            ).iterator() andThen
            mutableListOf( // can't return the same iterator as it is stateful
                mockBuildType
            ).iterator() andThen
            mutableListOf( // and again
                mockBuildType
            ).iterator()
        every { project.dependencies.add(any(), any()) } returns mockk()
        every { project.extensions.findByType(BaseExtension::class.java)!!.compileSdkVersion } returns "android-35"
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.compileSdkVersion } returns "android-35"

        val pluginHandler = PluginHandler(project)
        mockkObject(NativePluginLoaderReflectionBridge)
        // mock return of NativePluginLoaderReflectionBridge.getPlugins
        val pluginWithDependencies: MutableMap<String?, Any?> = cameraDependency.toMutableMap()
        pluginWithDependencies["dependencies"] =
            listOf(flutterPluginAndroidLifecycleDependency["name"])
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns
            listOf(
                pluginWithDependencies
            )
        // mock method calls that are invoked by the args to NativePluginLoaderReflectionBridge
        every { project.extraProperties } returns mockk()
        every { project.extensions.findByType(FlutterExtension::class.java) } returns FlutterExtension()
        every { project.file(any()) } returns mockk()

        pluginHandler.configurePlugins(
            engineVersionValue = EXAMPLE_ENGINE_VERSION
        )

        verify { project.afterEvaluate(capture(captureActionSlot)) }
        verify { pluginProject.afterEvaluate(capture(capturePluginActionSlot)) }
        captureActionSlot.captured.execute(project)
        capturePluginActionSlot[0].execute(pluginProject)
        capturePluginActionSlot[1].execute(pluginProject)
        verify { pluginProject.extensions.create("flutter", FlutterExtension::class.java) }
        verify {
            pluginProject.dependencies.add(
                "debugApi",
                "io.flutter:flutter_embedding_debug:$EXAMPLE_ENGINE_VERSION"
            )
        }
        verify { project.dependencies.add("debugApi", pluginProject) }
        verify { mockLogger wasNot called }
        // For library projects, individual build types should be created, not addAll
        verify(exactly = 0) { mockPluginProjectBuildTypes.addAll(any()) }

        verify { pluginProject.dependencies.add("implementation", pluginDependencyProject) }
    }

    @Test
    fun `configurePlugins throws IllegalArgumentException when plugin has null dependencies`(
        @TempDir tempDir: Path
    ) {
        val project = mockk<Project>()

        // configuration for configureLegacyPluginEachProjects
        val projectDir = tempDir.resolve("my-plugin")
        projectDir.toFile().mkdirs()
        every { project.projectDir } returns projectDir.toFile()
        val settingsGradle = File(projectDir.parent.toFile(), "settings.gradle")
        settingsGradle.createNewFile()
        val mockLogger = mockk<Logger>()
        every { project.logger } returns mockLogger

        val pluginProject = mockk<Project>()
        val mockBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>()
        every { pluginProject.hasProperty("local-engine-repo") } returns false
        every { pluginProject.hasProperty("android") } returns true
        every { mockBuildType.name } returns "debug"
        every { mockBuildType.isDebuggable } returns true
        val pluginWithNullDependencies: MutableMap<String?, Any?> = cameraDependency.toMutableMap()
        pluginWithNullDependencies["dependencies"] = null
        every { project.rootProject.findProject(":${pluginWithNullDependencies["name"]}") } returns pluginProject
        every { pluginProject.extensions.create(any(), any<Class<Any>>()) } returns mockk()
        every { project.afterEvaluate(any<Action<Project>>()) } returns Unit
        every { pluginProject.afterEvaluate(any<Action<Project>>()) } returns Unit

        val mockProjectBuildTypes =
            mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        val mockPluginProjectBuildTypes =
            mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        every { project.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockProjectBuildTypes
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockPluginProjectBuildTypes
        every { mockPluginProjectBuildTypes.addAll(any()) } returns true
        every { pluginProject.configurations.named(any<String>()) } returns mockk()
        every { pluginProject.dependencies.add(any(), any()) } returns mockk()

        every {
            project.extensions
                .findByType(BaseExtension::class.java)!!
                .buildTypes
                .iterator()
        } returns
            mutableListOf(
                mockBuildType
            ).iterator() andThen
            mutableListOf( // can't return the same iterator as it is stateful
                mockBuildType
            ).iterator() andThen
            mutableListOf( // and again
                mockBuildType
            ).iterator()
        every { project.dependencies.add(any(), any()) } returns mockk()
        every { project.extensions.findByType(BaseExtension::class.java)!!.compileSdkVersion } returns "android-35"
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.compileSdkVersion } returns "android-35"

        val pluginHandler = PluginHandler(project)
        mockkObject(NativePluginLoaderReflectionBridge)
        // mock return of NativePluginLoaderReflectionBridge.getPlugins
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns
            listOf(
                pluginWithNullDependencies
            )
        // mock method calls that are invoked by the args to NativePluginLoaderReflectionBridge
        every { project.extraProperties } returns mockk()
        every { project.extensions.findByType(FlutterExtension::class.java) } returns FlutterExtension()
        every { project.file(any()) } returns mockk()

        assertThrows<IllegalArgumentException> {
            pluginHandler.configurePlugins(
                engineVersionValue = EXAMPLE_ENGINE_VERSION
            )
        }
    }

    @Test
    fun `configurePlugins uses addAll for app plugins`(
        @TempDir tempDir: Path
    ) {
        val project = mockk<Project>()
        val pluginProject = mockk<Project>()

        // Setup minimal mocks
        setupBasicMocks(project, pluginProject, mockk(), tempDir)
        setupPluginMocks(project)

        // Mock isBuiltAsApp to return true (app plugin)
        mockkObject(FlutterPluginUtils)
        every { FlutterPluginUtils.isBuiltAsApp(pluginProject) } returns true

        val mockProjectBuildTypes = mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        val mockPluginProjectBuildTypes = mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()

        every { project.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockProjectBuildTypes
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockPluginProjectBuildTypes
        every { mockPluginProjectBuildTypes.addAll(any()) } returns true

        // Mock the iterator to return at least one build type "debug" so addEmbeddingDependencyToPlugin is called
        val testBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>()
        every { testBuildType.name } returns "debug"
        every { testBuildType.isDebuggable } returns true
        every { testBuildType.isMinifyEnabled } returns false
        every { mockProjectBuildTypes.iterator() } answers { mutableListOf(testBuildType).iterator() }

        mockkObject(FlutterPluginUtils)
        every { FlutterPluginUtils.getLegacyAndroidExtension(project) } returns project.extensions.findByType(BaseExtension::class.java)!!
        every { FlutterPluginUtils.getLegacyAndroidExtension(pluginProject) } returns
            pluginProject.extensions.findByType(BaseExtension::class.java)!!

        val captureActionSlot = slot<Action<Project>>()
        val capturePluginActionSlot = mutableListOf<Action<Project>>()
        every { project.afterEvaluate(any<Action<Project>>()) } returns Unit
        every { pluginProject.afterEvaluate(any<Action<Project>>()) } returns Unit

        val pluginHandler = PluginHandler(project)
        pluginHandler.configurePlugins(
            engineVersionValue = EXAMPLE_ENGINE_VERSION
        )

        // Capture and execute afterEvaluate actions to simulate Gradle lifecycle
        verify { project.afterEvaluate(capture(captureActionSlot)) }
        verify { pluginProject.afterEvaluate(capture(capturePluginActionSlot)) }
        captureActionSlot.captured.execute(project)
        capturePluginActionSlot[0].execute(pluginProject)

        // Verify that addAll is called exactly once, and create is never called
        verify(exactly = 1) { mockPluginProjectBuildTypes.addAll(any()) }
        verify(exactly = 0) {
            mockPluginProjectBuildTypes.create(
                any<String>(),
                any<Action<com.android.build.gradle.internal.dsl.BuildType>>()
            )
        }
    }

    @Test
    fun `configurePlugins creates individual build types for library plugins`(
        @TempDir tempDir: Path
    ) {
        val project = mockk<Project>()
        val pluginProject = mockk<Project>()

        // Setup minimal mocks
        setupBasicMocks(project, pluginProject, mockk(), tempDir)
        setupPluginMocks(project)

        // Mock isBuiltAsApp to return false (library plugin)
        mockkObject(FlutterPluginUtils)
        every { FlutterPluginUtils.isBuiltAsApp(pluginProject) } returns false

        val mockProjectBuildTypes = mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        val mockPluginProjectBuildTypes = mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        val mockCreatedBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>(relaxed = true)

        every { project.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockProjectBuildTypes
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockPluginProjectBuildTypes
        every { mockPluginProjectBuildTypes.findByName("debug") } returns null
        every {
            mockPluginProjectBuildTypes.create(
                "debug",
                any<Action<com.android.build.gradle.internal.dsl.BuildType>>()
            )
        } returns mockCreatedBuildType

        // Mock the iterator for forEach
        val testBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>()
        every { testBuildType.name } returns "debug"
        every { testBuildType.isDebuggable } returns true
        every { testBuildType.isMinifyEnabled } returns false
        every { mockProjectBuildTypes.iterator() } answers { mutableListOf(testBuildType).iterator() }

        mockkObject(FlutterPluginUtils)
        every { FlutterPluginUtils.getLegacyAndroidExtension(project) } returns project.extensions.findByType(BaseExtension::class.java)!!
        every { FlutterPluginUtils.getLegacyAndroidExtension(pluginProject) } returns
            pluginProject.extensions.findByType(BaseExtension::class.java)!!

        val captureActionSlot = slot<Action<Project>>()
        val capturePluginActionSlot = mutableListOf<Action<Project>>()
        every { project.afterEvaluate(any<Action<Project>>()) } returns Unit
        every { pluginProject.afterEvaluate(any<Action<Project>>()) } returns Unit

        val pluginHandler = PluginHandler(project)
        pluginHandler.configurePlugins(
            engineVersionValue = EXAMPLE_ENGINE_VERSION
        )

        // Capture and execute afterEvaluate actions to simulate Gradle lifecycle
        verify { project.afterEvaluate(capture(captureActionSlot)) }
        verify { pluginProject.afterEvaluate(capture(capturePluginActionSlot)) }
        captureActionSlot.captured.execute(project)
        capturePluginActionSlot[0].execute(pluginProject)

        // Verify that individual create is called, and addAll is never called
        verify(exactly = 1) {
            mockPluginProjectBuildTypes.create(
                "debug",
                any<Action<com.android.build.gradle.internal.dsl.BuildType>>()
            )
        }
        verify(exactly = 0) { mockPluginProjectBuildTypes.addAll(any()) }
    }

    @Test
    fun `configurePlugins creates build types for library plugins even if supportsBuildMode is false`(
        @TempDir tempDir: Path
    ) {
        val project = mockk<Project>()
        val pluginProject = mockk<Project>()

        // Setup minimal mocks
        setupBasicMocks(project, pluginProject, mockk(), tempDir)
        setupPluginMocks(project)

        // Mock isBuiltAsApp to return false (library plugin)
        mockkObject(FlutterPluginUtils)
        every { FlutterPluginUtils.isBuiltAsApp(pluginProject) } returns false

        // Configure pluginProject properties to make supportsBuildMode return false for "profile" mode
        every { pluginProject.hasProperty("local-engine-repo") } returns true
        every { pluginProject.hasProperty("local-engine-build-mode") } returns true
        every { pluginProject.property("local-engine-build-mode") } returns "debug"

        val mockProjectBuildTypes = mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        val mockPluginProjectBuildTypes = mockk<NamedDomainObjectContainer<com.android.build.gradle.internal.dsl.BuildType>>()
        val mockCreatedBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>(relaxed = true)

        every { project.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockProjectBuildTypes
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.buildTypes } returns mockPluginProjectBuildTypes
        
        // We want to test that when the main project has "profile" build type,
        // and supportsBuildMode(pluginProject, "profile") is false:
        // The "profile" build type is STILL created on the plugin.
        every { mockPluginProjectBuildTypes.findByName("profile") } returns null
        every {
            mockPluginProjectBuildTypes.create(
                "profile",
                any<Action<com.android.build.gradle.internal.dsl.BuildType>>()
            )
        } returns mockCreatedBuildType

        // Mock the iterator for forEach to return "profile" build type
        val testBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>()
        every { testBuildType.name } returns "profile"
        every { testBuildType.isDebuggable } returns false
        every { testBuildType.isMinifyEnabled } returns false
        every { mockProjectBuildTypes.iterator() } answers { mutableListOf(testBuildType).iterator() }

        every { FlutterPluginUtils.getLegacyAndroidExtension(project) } returns project.extensions.findByType(BaseExtension::class.java)!!
        every { FlutterPluginUtils.getLegacyAndroidExtension(pluginProject) } returns
            pluginProject.extensions.findByType(BaseExtension::class.java)!!

        val captureActionSlot = slot<Action<Project>>()
        val capturePluginActionSlot = mutableListOf<Action<Project>>()
        every { project.afterEvaluate(any<Action<Project>>()) } returns Unit
        every { pluginProject.afterEvaluate(any<Action<Project>>()) } returns Unit

        val pluginHandler = PluginHandler(project)
        pluginHandler.configurePlugins(
            engineVersionValue = EXAMPLE_ENGINE_VERSION
        )

        // Capture and execute afterEvaluate actions to simulate Gradle lifecycle
        verify { project.afterEvaluate(capture(captureActionSlot)) }
        verify { pluginProject.afterEvaluate(capture(capturePluginActionSlot)) }
        captureActionSlot.captured.execute(project)
        capturePluginActionSlot[0].execute(pluginProject)

        // Verify that individual create was called for "profile" even though supportsBuildMode is false
        verify(exactly = 1) {
            mockPluginProjectBuildTypes.create(
                "profile",
                any<Action<com.android.build.gradle.internal.dsl.BuildType>>()
            )
        }
        // Verify that the embedding dependency was NOT added for "profile" (since supportsBuildMode is false)
        verify(exactly = 0) {
            pluginProject.dependencies.add(
                "profileApi",
                any()
            )
        }
    }

    private fun setupBasicMocks(
        project: Project,
        pluginProject: Project,
        mockBuildType: com.android.build.gradle.internal.dsl.BuildType,
        tempDir: Path
    ) {
        // Configuration for project directory
        val projectDir = tempDir.resolve("my-plugin")
        projectDir.toFile().mkdirs()
        every { project.projectDir } returns projectDir.toFile()
        val settingsGradle = File(projectDir.parent.toFile(), "settings.gradle")
        settingsGradle.createNewFile()
        val mockLogger = mockk<Logger>()
        every { project.logger } returns mockLogger

        // Plugin project setup
        every { project.getName() } returns "app"
        every { pluginProject.getName() } returns (cameraDependency["name"] as? String ?: "plugin")
        every { pluginProject.hasProperty("local-engine-repo") } returns false
        every { pluginProject.hasProperty("android") } returns true
        val mockPluginContainer = mockk<org.gradle.api.plugins.PluginContainer>()
        every { pluginProject.plugins } returns mockPluginContainer
        every { mockPluginContainer.hasPlugin("com.android.application") } returns false
        every { mockBuildType.name } returns "debug"
        every { mockBuildType.isDebuggable } returns true
        every { project.rootProject.findProject(":${cameraDependency["name"]}") } returns pluginProject
        every { pluginProject.extensions.create(any(), any<Class<Any>>()) } returns mockk()
        every { project.afterEvaluate(any<Action<Project>>()) } returns Unit
        every { pluginProject.afterEvaluate(any<Action<Project>>()) } returns Unit

        // Dependencies and configurations
        every { pluginProject.configurations.named(any<String>()) } returns mockk()
        every { pluginProject.dependencies.add(any(), any()) } returns mockk()
        every { project.dependencies.add(any(), any()) } returns mockk()
        every { project.extensions.findByType(BaseExtension::class.java)!!.compileSdkVersion } returns "android-35"
        every { pluginProject.extensions.findByType(BaseExtension::class.java)!!.compileSdkVersion } returns "android-35"
    }

    private fun setupPluginMocks(project: Project) {
        mockkObject(NativePluginLoaderReflectionBridge)
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns listOf(cameraDependency)
        every { project.extraProperties } returns mockk()
        every { project.extensions.findByType(FlutterExtension::class.java) } returns FlutterExtension()
        every { project.file(any()) } returns mockk()
    }
}
