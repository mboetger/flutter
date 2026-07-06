// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import com.android.build.api.dsl.ApplicationExtension
import com.android.build.api.variant.AndroidComponentsExtension
import com.android.build.gradle.AbstractAppExtension
import com.android.build.gradle.BaseExtension
import com.android.build.gradle.LibraryExtension
import com.android.build.gradle.internal.core.InternalBaseVariant
import com.android.build.gradle.tasks.ProcessAndroidResources
import com.android.builder.model.BuildType
import com.flutter.gradle.tasks.FlutterTask
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import org.gradle.api.Action
import org.gradle.api.Project
import org.gradle.api.Task
import org.gradle.api.file.Directory
import org.gradle.api.tasks.Copy
import org.gradle.api.tasks.TaskContainer
import org.gradle.api.tasks.TaskProvider
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import org.junit.jupiter.api.io.TempDir
import java.nio.file.Path
import kotlin.io.path.writeText
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class Issue69865Test {
    @Test
    fun `buildModeFor hardcodes debug mode for any debuggable host app build type`() {
        val debugBuildType = mockk<BuildType>()
        every { debugBuildType.name } returns "debug"
        every { debugBuildType.isDebuggable } returns true

        val buildMode = FlutterPluginUtils.buildModeFor(debugBuildType)
        // In issue #69865, a user attempts to build an unsigned debug APK of an existing Android app
        // integrated with the release version of a Flutter module.
        // However, FlutterPluginUtils.buildModeFor unconditionally returns "debug" whenever
        // the host app's build type is debuggable (isDebuggable = true).
        assertEquals("debug", buildMode)
    }

    @Test
    fun `debuggable host app variant refuses to link with release flutter module variant`(
        @TempDir tempDir: Path
    ) {
        val projectDir = tempDir.resolve("project-dir").resolve("android").resolve("flutter")
        projectDir.toFile().mkdirs()
        val settingsFile = projectDir.parent.resolve("settings.gradle")
        settingsFile.writeText("empty for now")
        val fakeFlutterSdkDir = tempDir.resolve("fake-flutter-sdk")
        fakeFlutterSdkDir.toFile().mkdirs()
        val fakeCacheDir = fakeFlutterSdkDir.resolve("bin").resolve("cache")
        fakeCacheDir.toFile().mkdirs()
        val fakeEngineStampFile = fakeCacheDir.resolve("engine.stamp")
        fakeEngineStampFile.writeText("901b0f1afe77c3555abee7b86a26aaa37f131379")
        val fakeEngineRealmFile = fakeCacheDir.resolve("engine.realm")
        fakeEngineRealmFile.writeText("made_up_realm")

        val project = mockk<Project>(relaxed = true)
        val rootProject = mockk<Project>(relaxed = true)
        val appProject = mockk<Project>(relaxed = true)

        every { project.rootProject } returns rootProject
        every { rootProject.findProject(":app") } returns appProject
        every { rootProject.hasProperty("flutter.hostAppProjectName") } returns false
        every { project.state.failure as Throwable? } returns null
        every { appProject.state.failure as Throwable? } returns null

        val mockLibraryExtension = mockk<LibraryExtension>(relaxed = true)
        val mockAbstractAppExtension =
            mockk<AbstractAppExtension>(
                moreInterfaces = arrayOf(ApplicationExtension::class),
                relaxed = true
            )

        every { project.extensions.findByType(LibraryExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.getByType(LibraryExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.findByName("android") } returns mockLibraryExtension

        val mockAndroidComponentsExtension = mockk<AndroidComponentsExtension<*, *, *>>(relaxed = true)
        every { project.extensions.getByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        every { mockAndroidComponentsExtension.selector() } returns mockk { every { all() } returns mockk() }

        val mockDebugBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>(relaxed = true)
        val mockReleaseBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>(relaxed = true)
        every { mockLibraryExtension.buildTypes.getByName("debug") } returns mockDebugBuildType
        every { mockLibraryExtension.buildTypes.getByName("release") } returns mockReleaseBuildType

        every { project.extensions.findByType(ApplicationExtension::class.java) } returns null
        every { project.extensions.findByType(com.android.build.api.dsl.CommonExtension::class.java) } returns null
        val mockApplicationExtension = mockAbstractAppExtension as ApplicationExtension
        val mockAppDebugBuildType = mockk<com.android.build.api.dsl.ApplicationBuildType>(relaxed = true)
        val mockAppReleaseBuildType = mockk<com.android.build.api.dsl.ApplicationBuildType>(relaxed = true)
        every { mockApplicationExtension.buildTypes.getByName("debug") } returns mockAppDebugBuildType
        every { mockApplicationExtension.buildTypes.getByName("release") } returns mockAppReleaseBuildType
        every { appProject.extensions.findByType(ApplicationExtension::class.java) } returns mockApplicationExtension
        every { appProject.extensions.getByType(ApplicationExtension::class.java) } returns mockApplicationExtension
        every { appProject.extensions.findByType(com.android.build.api.dsl.CommonExtension::class.java) } returns mockk(relaxed = true)

        every { project.projectDir } returns projectDir.toFile()
        every { rootProject.projectDir } returns projectDir.parent.toFile()
        every { appProject.projectDir } returns projectDir.parent.resolve("app").toFile()
        every { project.findProperty("flutter.sdk") } returns fakeFlutterSdkDir.toString()
        every { project.file(fakeFlutterSdkDir.toString()) } returns fakeFlutterSdkDir.toFile()

        val flutterExtension = FlutterExtension()
        every { project.extensions.create("flutter", any<Class<*>>()) } returns flutterExtension
        every { project.extensions.findByType(FlutterExtension::class.java) } returns flutterExtension
        every { project.extensions.findByType(BaseExtension::class.java) } returns mockk(relaxed = true)

        val mockDirectory = mockk<Directory>(relaxed = true)
        every { project.layout.buildDirectory.get() } returns mockDirectory

        // Set up task container for project (:flutter)
        val taskContainer = mockk<TaskContainer>(relaxed = true)
        every { project.tasks } returns taskContainer
        val copyTask = mockk<Copy>(relaxed = true)
        val mockCopyTaskProvider = mockk<TaskProvider<Copy>>(relaxed = true)
        every { mockCopyTaskProvider.hint(Copy::class).get() } returns copyTask
        every {
            taskContainer.register(
                match { it.startsWith("copyFlutterAssets") },
                eq(Copy::class.java),
                any()
            )
        } answers {
            val taskName = firstArg<String>()
            every { copyTask.name } returns taskName
            every { mockCopyTaskProvider.name } returns taskName
            mockCopyTaskProvider
        }

        val flutterTask = mockk<FlutterTask>(relaxed = true)
        every { flutterTask.assets } returns mockk(relaxed = true)
        val flutterTaskProvider = mockk<TaskProvider<FlutterTask>>(relaxed = true)
        every { flutterTaskProvider.hint(FlutterTask::class).get() } returns flutterTask
        every {
            taskContainer.register(
                match { it.contains("compileFlutterBuild") },
                any<Class<FlutterTask>>(),
                any()
            )
        } returns flutterTaskProvider
        every {
            taskContainer.register(
                match { it.contains("packJniLibs") },
                eq(org.gradle.api.tasks.bundling.Jar::class.java),
                any()
            )
        } returns mockk(relaxed = true)
        every { taskContainer.named(any<String>()) } answers {
            val taskProvider = mockk<TaskProvider<Task>>(relaxed = true)
            val mockTask = mockk<Task>(relaxed = true)
            every { taskProvider.get() } returns mockTask
            every { taskProvider.hint(Task::class).get() } returns mockTask
            taskProvider
        }

        // Track dependencies added to :app:mergeDebugAssets
        val mergeDebugAssetsTask = mockk<Task>(relaxed = true)
        val dependsOnArgs = mutableListOf<Any>()
        every { mergeDebugAssetsTask.dependsOn(capture(dependsOnArgs)) } returns mergeDebugAssetsTask
        every { taskContainer.findByPath(":app:mergeDebugAssets") } returns mergeDebugAssetsTask

        // Mock library variants on :flutter (both debug and release)
        fun createMockLibraryVariant(
            variantName: String,
            debuggable: Boolean
        ): com.android.build.gradle.api.LibraryVariant {
            val variant = mockk<com.android.build.gradle.api.LibraryVariant>(relaxed = true)
            every { variant.name } returns variantName
            every { variant.buildType.name } returns variantName
            every { variant.buildType.isDebuggable } returns debuggable
            every { variant.flavorName } returns ""
            val mergedFlavor = mockk<InternalBaseVariant.MergedFlavor>(relaxed = true)
            every { variant.mergedFlavor } returns mergedFlavor
            val apiLevel = mockk<com.android.builder.model.ApiVersion>(relaxed = true)
            every { apiLevel.apiLevel } returns 21
            every { mergedFlavor.minSdkVersion } returns apiLevel
            val variantOutput = mockk<com.android.build.gradle.api.BaseVariantOutput>(relaxed = true)
            every { variantOutput.processResourcesProvider.hint(ProcessAndroidResources::class).get() } returns mockk(relaxed = true)
            val outputsIterator = mockk<MutableIterator<com.android.build.gradle.api.BaseVariantOutput>>()
            every { outputsIterator.hasNext() } returns true andThen false
            every { outputsIterator.next() } returns variantOutput
            val variantOutputCollection = mockk<org.gradle.api.DomainObjectCollection<com.android.build.gradle.api.BaseVariantOutput>>()
            every { variantOutputCollection.iterator() } returns outputsIterator
            every { variant.outputs } returns variantOutputCollection
            return variant
        }

        val libDebugVariant = createMockLibraryVariant("debug", true)
        val libReleaseVariant = createMockLibraryVariant("release", false)
        val libraryVariants = listOf(libDebugVariant, libReleaseVariant)
        val libraryVariantCollection =
            mockk<org.gradle.api.internal.DefaultDomainObjectSet<com.android.build.gradle.api.LibraryVariant>>(relaxed = true)
        every { mockLibraryExtension.libraryVariants } returns libraryVariantCollection
        every { libraryVariantCollection.all(any<Action<com.android.build.gradle.api.LibraryVariant>>()) } answers {
            libraryVariants.forEach { firstArg<Action<com.android.build.gradle.api.LibraryVariant>>().execute(it) }
        }

        // Set up host app project (:app)
        every { appProject.extensions.findByName("android") } returns mockAbstractAppExtension
        val appDebugVariant = mockk<com.android.build.gradle.api.ApplicationVariant>(relaxed = true)
        every { appDebugVariant.name } returns "debug"
        every { appDebugVariant.buildType.name } returns "debug"
        every { appDebugVariant.buildType.isDebuggable } returns true
        val assembleTask = mockk<Task>(relaxed = true)
        every { assembleTask.name } returns "assembleDebug"
        every { appDebugVariant.assembleProvider.get() } returns assembleTask

        val appVariants = listOf(appDebugVariant)
        val appVariantCollection = mockk<org.gradle.api.DomainObjectSet<com.android.build.gradle.api.ApplicationVariant>>(relaxed = true)
        every { mockAbstractAppExtension.applicationVariants } returns appVariantCollection
        every { appVariantCollection.all(any<Action<com.android.build.gradle.api.ApplicationVariant>>()) } answers {
            appVariants.forEach { firstArg<Action<com.android.build.gradle.api.ApplicationVariant>>().execute(it) }
        }

        every { appProject.afterEvaluate(any<Action<Project>>()) } answers {
            firstArg<Action<Project>>().execute(appProject)
        }
        every { appProject.afterEvaluate(any<groovy.lang.Closure<*>>()) } answers {
            firstArg<groovy.lang.Closure<*>>().call(appProject)
        }

        mockkObject(NativePluginLoaderReflectionBridge)
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns listOf()
        every { project.extraProperties } returns mockk(relaxed = true)
        every { project.file(flutterExtension.source!!) } returns mockk(relaxed = true)
        every { project.findProperty("flutter.buildMode.debug") } returns "release"
        every { appProject.findProperty("flutter.buildMode.debug") } returns "release"

        val flutterPlugin = FlutterPlugin()
        flutterPlugin.apply(project)

        val linkedTaskNames =
            dependsOnArgs.flatMap { if (it is Array<*>) it.asIterable() else listOf(it) }.map {
                (it as? TaskProvider<*>)?.name ?: (it as? Task)?.name ?: it.toString()
            }

        // In issue #69865, the user wants to build a debug APK of their existing Android app
        // integrated with the release version of the Flutter module.
        // We assert that mergeDebugAssets should depend on the release assets task (copyFlutterAssetsRelease).
        // Because FlutterPlugin now checks property overrides in buildModeFor:
        //   if (FlutterPluginUtils.buildModeFor(appProjectVariant.buildType, appProject) != variantBuildMode) return
        // setting flutter.buildMode.debug = release allows integrating the release library variant into a debuggable app variant.
        assertTrue(
            linkedTaskNames.any { it.contains("Release", ignoreCase = true) },
            "Expected debug APK to be integrated with release version of Flutter module (issue #69865), but linked tasks were: $linkedTaskNames"
        )
    }
}
