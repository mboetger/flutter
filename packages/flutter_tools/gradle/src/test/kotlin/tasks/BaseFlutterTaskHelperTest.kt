// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.tasks

import com.flutter.gradle.DependencyVersionChecker
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.gradle.api.Action
import org.gradle.api.GradleException
import org.gradle.api.Project
import org.gradle.api.file.ConfigurableFileCollection
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.file.RegularFileProperty
import org.gradle.api.logging.LoggingManager
import org.gradle.api.provider.ListProperty
import org.gradle.api.provider.Property
import org.gradle.kotlin.dsl.support.serviceOf
import org.gradle.process.ExecOperations
import org.gradle.process.ExecSpec
import org.gradle.process.ProcessForkOptions
import org.junit.jupiter.api.assertDoesNotThrow
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.nio.file.Path
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class BaseFlutterTaskHelperTest {
    object BaseFlutterTaskPropertiesTest {
        internal const val LOCAL_ENGINE_TEST = "android_debug_arm64"
        internal const val LOCAL_ENGINE_HOST_TEST = "host_debug"
        internal const val DART_DEFINES_TEST = "ENVIRONMENT=development"
        internal const val FLAVOR_TEST = "dev"
        internal const val EXTRA_FRONTEND_OPTIONS_TEST = "--enable-asserts"
        internal const val EXTRA_GEN_SNAPSHOT_OPTIONS_TEST = "--debugger"
        internal const val TARGET_PLATFORM_VALUES_JOINED_LIST = "android linux"
        val MIN_SDK_VERSION_TEST = DependencyVersionChecker.warnMinSdkVersion

        // Using File.separator to ensure all paths use platform-specific separators
        internal val FLUTTER_ROOT_ABSOLUTE_PATH_TEST = "/path/to/flutter".replace("/", File.separator)
        internal val FLUTTER_EXECUTABLE_ABSOLUTE_PATH_TEST = "/path/to/flutter/bin/flutter".replace("/", File.separator)
        internal val LOCAL_ENGINE_SRC_PATH_TEST = "/path/to/flutter/engine/src".replace("/", File.separator)
        internal val PERFORMANCE_MEASUREMENT_FILE_TEST = "/path/to/build/performance_file".replace("/", File.separator)
        internal val FRONTEND_SERVER_STARTER_PATH_TEST = "/path/to/starter/script_file".replace("/", File.separator)
        internal val SPLIT_DEBUG_INFO_TEST = "/path/to/build/debug_info_directory".replace("/", File.separator)
        internal val CODE_SIZE_DIRECTORY_TEST = "/path/to/build/code_size_directory".replace("/", File.separator)

        internal val BUNDLE_SK_SL_PATH_TEST = "/path/to/custom/shaders".replace("/", File.separator)
        internal val FLUTTER_TARGET_FILE_PATH = "/path/to/flutter/examples/splash/lib/main.dart".replace("/", File.separator)
        internal val FLUTTER_TARGET_PATH = "/path/to/main.dart".replace("/", File.separator)

        internal val sourceDirTest = File("/path/to/working_directory".replace("/", File.separator))
        internal val flutterRootTest = File("/path/to/flutter".replace("/", File.separator))
        internal val flutterExecutableTest = File("/path/to/flutter/bin/flutter".replace("/", File.separator))
        internal val intermediateDirFileTest = File("/path/to/build/app/intermediates/flutter/release".replace("/", File.separator))
        internal val targetPlatformValuesList = listOf("android", "linux")
    }

    private inline fun <reified T : Any> mockProperty(value: T?): Property<T> {
        val mockProp = mockk<Property<T>>(relaxed = true)
        every { mockProp.orNull } returns value
        if (value != null) {
            every { mockProp.get() } returns value
            every { mockProp.getOrElse(any()) } returns value
        } else {
            every { mockProp.getOrElse(any()) } answers { firstArg() }
        }
        return mockProp
    }

    private fun mockDirectoryProperty(file: File?): DirectoryProperty {
        val mockProp = mockk<DirectoryProperty>(relaxed = true)
        if (file != null) {
            val mockDirectory = mockk<org.gradle.api.file.Directory>(relaxed = true)
            every { mockDirectory.asFile } returns file
            every { mockProp.orNull } returns mockDirectory
            every { mockProp.get() } returns mockDirectory

            val mockRegularFileProvider = mockk<org.gradle.api.provider.Provider<org.gradle.api.file.RegularFile>>(relaxed = true)
            val mockRegularFile = mockk<org.gradle.api.file.RegularFile>(relaxed = true)
            val childFile = try {
                File(file, "flutter_build.d")
            } catch (e: NullPointerException) {
                mockk<File>(relaxed = true)
            }
            every { mockRegularFile.asFile } returns childFile
            every { mockRegularFileProvider.get() } returns mockRegularFile
            every { mockProp.file(any<String>()) } returns mockRegularFileProvider
        } else {
            every { mockProp.orNull } returns null
            every { mockProp.get() } throws IllegalStateException("Property has no value")
        }
        return mockProp
    }

    private fun mockRegularFileProperty(file: File?): RegularFileProperty {
        val mockProp = mockk<RegularFileProperty>(relaxed = true)
        if (file != null) {
            val mockFile = mockk<org.gradle.api.file.RegularFile>(relaxed = true)
            every { mockFile.asFile } returns file
            every { mockProp.orNull } returns mockFile
            every { mockProp.get() } returns mockFile
        } else {
            every { mockProp.orNull } returns null
            every { mockProp.get() } throws IllegalStateException("Property has no value")
        }
        return mockProp
    }

    private inline fun <reified T : Any> mockListProperty(value: List<T>?): ListProperty<T> {
        val mockProp = mockk<ListProperty<T>>(relaxed = true)
        every { mockProp.orNull } returns value
        if (value != null) {
            every { mockProp.get() } returns value
        }
        return mockProp
    }

    @Test
    fun `checkPreConditions throws a GradleException when sourceDir is null`() {
        val baseFlutterTask = mockk<BaseFlutterTask>()
        every { baseFlutterTask.sourceDir } returns mockDirectoryProperty(null)

        val gradleException =
            assertFailsWith<GradleException> { BaseFlutterTaskHelper.checkPreConditions(baseFlutterTask) }
        assert(
            gradleException.message ==
                BaseFlutterTaskHelper.getGradleErrorMessage(baseFlutterTask)
        )
    }

    @Test
    fun `checkPreConditions throws a GradleException when sourceDir is not a directory`() {
        val baseFlutterTask = mockk<BaseFlutterTask>()
        val mockSourceDir = File("non_existent_directory")
        every { baseFlutterTask.sourceDir } returns mockDirectoryProperty(mockSourceDir)

        val gradleException =
            assertFailsWith<GradleException> { BaseFlutterTaskHelper.checkPreConditions(baseFlutterTask) }
        assert(
            gradleException.message ==
                BaseFlutterTaskHelper.getGradleErrorMessage(baseFlutterTask)
        )
    }

    @Test
    fun `checkPreConditions does not throw a GradleException and intermediateDir is valid`(
        @TempDir tempDir: Path
    ) {
        val baseFlutterTask = mockk<BaseFlutterTask>()

        val mockSourceDir = File(".")
        every { baseFlutterTask.sourceDir } returns mockDirectoryProperty(mockSourceDir)

        val mockIntermediateDir = File(tempDir.toFile(), "intermediate")
        every { baseFlutterTask.intermediateDir } returns mockDirectoryProperty(mockIntermediateDir)

        assertDoesNotThrow { BaseFlutterTaskHelper.checkPreConditions(baseFlutterTask) }
        assert(mockIntermediateDir.exists())
    }

    @Test
    fun `generateRuleNames returns correct rule names when buildMode is debug`() {
        val baseFlutterTask = mockk<BaseFlutterTask>()
        every { baseFlutterTask.buildMode } returns mockProperty("debug")
        every { baseFlutterTask.deferredComponents } returns mockProperty(null)
        every { baseFlutterTask.targetPlatformValues } returns mockListProperty(null)

        val ruleNamesList = BaseFlutterTaskHelper.generateRuleNames(baseFlutterTask)

        assertEquals(ruleNamesList, listOf("debug_android_application"))
    }

    @Test
    fun `generateRuleNames returns correct rule names when buildMode is not debug and deferredComponents is true`() {
        val baseFlutterTask = mockk<BaseFlutterTask>()
        every { baseFlutterTask.buildMode } returns mockProperty("release")
        every { baseFlutterTask.deferredComponents } returns mockProperty(true)
        every { baseFlutterTask.targetPlatformValues } returns mockListProperty(BaseFlutterTaskPropertiesTest.targetPlatformValuesList)

        val ruleNamesList = BaseFlutterTaskHelper.generateRuleNames(baseFlutterTask)

        assertEquals(
            ruleNamesList,
            listOf(
                "android_aot_deferred_components_bundle_release_android",
                "android_aot_deferred_components_bundle_release_linux"
            )
        )
    }

    @Test
    fun `generateRuleNames returns correct rule names when buildMode is not debug and deferredComponents is false`() {
        val baseFlutterTask = mockk<BaseFlutterTask>()
        every { baseFlutterTask.buildMode } returns mockProperty("release")
        every { baseFlutterTask.deferredComponents } returns mockProperty(false)
        every { baseFlutterTask.targetPlatformValues } returns mockListProperty(BaseFlutterTaskPropertiesTest.targetPlatformValuesList)

        val ruleNamesList = BaseFlutterTaskHelper.generateRuleNames(baseFlutterTask)

        assertEquals(
            ruleNamesList,
            listOf(
                "android_aot_bundle_release_android",
                "android_aot_bundle_release_linux"
            )
        )
    }

    @Test
    fun `createSpecActionFromTask creates the correct build configurations when properties are non-null`() {
        val buildModeString = "debug"

        // Create necessary mocks.
        val baseFlutterTask = mockk<BaseFlutterTask>()
        val mockExecSpec = mockk<ExecSpec>()
        val mockProcessForkOptions = mockk<ProcessForkOptions>()

        // Mock return values of properties.
        every { baseFlutterTask.sourceDir } returns mockDirectoryProperty(BaseFlutterTaskPropertiesTest.sourceDirTest)
        every { baseFlutterTask.flutterExecutable } returns mockRegularFileProperty(BaseFlutterTaskPropertiesTest.flutterExecutableTest)
        every { baseFlutterTask.targetPath } returns mockProperty(BaseFlutterTaskPropertiesTest.FLUTTER_TARGET_FILE_PATH)
        every { baseFlutterTask.localEngine } returns mockProperty(BaseFlutterTaskPropertiesTest.LOCAL_ENGINE_TEST)
        every { baseFlutterTask.localEngineSrcPath } returns mockProperty(BaseFlutterTaskPropertiesTest.LOCAL_ENGINE_SRC_PATH_TEST)
        every { baseFlutterTask.localEngineHost } returns mockProperty(BaseFlutterTaskPropertiesTest.LOCAL_ENGINE_HOST_TEST)
        every { baseFlutterTask.verbose } returns mockProperty(true)
        every { baseFlutterTask.intermediateDir } returns mockDirectoryProperty(BaseFlutterTaskPropertiesTest.intermediateDirFileTest)
        every { baseFlutterTask.performanceMeasurementFile } returns mockProperty(BaseFlutterTaskPropertiesTest.PERFORMANCE_MEASUREMENT_FILE_TEST)
        every { baseFlutterTask.buildMode } returns mockProperty(buildModeString)
        every { baseFlutterTask.flutterRoot } returns mockDirectoryProperty(BaseFlutterTaskPropertiesTest.flutterRootTest)
        every { baseFlutterTask.trackWidgetCreation } returns mockProperty(true)
        every { baseFlutterTask.splitDebugInfo } returns mockProperty(BaseFlutterTaskPropertiesTest.SPLIT_DEBUG_INFO_TEST)
        every { baseFlutterTask.treeShakeIcons } returns mockProperty(true)
        every { baseFlutterTask.dartObfuscation } returns mockProperty(true)
        every { baseFlutterTask.dartDefines } returns mockProperty(BaseFlutterTaskPropertiesTest.DART_DEFINES_TEST)
        every { baseFlutterTask.bundleSkSLPath } returns mockProperty(BaseFlutterTaskPropertiesTest.BUNDLE_SK_SL_PATH_TEST)
        every { baseFlutterTask.codeSizeDirectory } returns mockProperty(BaseFlutterTaskPropertiesTest.CODE_SIZE_DIRECTORY_TEST)
        every { baseFlutterTask.flavor } returns mockProperty(BaseFlutterTaskPropertiesTest.FLAVOR_TEST)
        every { baseFlutterTask.extraGenSnapshotOptions } returns mockProperty(BaseFlutterTaskPropertiesTest.EXTRA_GEN_SNAPSHOT_OPTIONS_TEST)
        every { baseFlutterTask.frontendServerStarterPath } returns mockProperty(BaseFlutterTaskPropertiesTest.FRONTEND_SERVER_STARTER_PATH_TEST)
        every { baseFlutterTask.extraFrontEndOptions } returns mockProperty(BaseFlutterTaskPropertiesTest.EXTRA_FRONTEND_OPTIONS_TEST)
        every { baseFlutterTask.targetPlatformValues } returns mockListProperty(BaseFlutterTaskPropertiesTest.targetPlatformValuesList)
        every { baseFlutterTask.minSdkVersion } returns mockProperty(BaseFlutterTaskPropertiesTest.MIN_SDK_VERSION_TEST)
        every { baseFlutterTask.deferredComponents } returns mockProperty(null)

        val execSpecActionFromTask = BaseFlutterTaskHelper.createExecSpecActionFromTask(baseFlutterTask)

        // Mock the method calls. We collapse all the args mock calls into four calls.
        every { mockExecSpec.executable(any<String>()) } returns mockExecSpec
        every { mockExecSpec.workingDir(any()) } returns mockProcessForkOptions
        every { mockExecSpec.args(any<String>(), any()) } returns mockExecSpec
        every { mockExecSpec.args(any<String>(), any()) } returns mockExecSpec
        every { mockExecSpec.args(any<String>()) } returns mockExecSpec
        every { mockExecSpec.args(any<List<String>>()) } returns mockExecSpec

        // Generate rule names for verification and can only be generated after buildMode is mocked.
        val ruleNamesList: List<String> = BaseFlutterTaskHelper.generateRuleNames(baseFlutterTask)

        execSpecActionFromTask.execute(mockExecSpec)

        // After execution, we verify the functions are actually being
        // called with the expected argument passed in.
        verify { mockExecSpec.executable(BaseFlutterTaskPropertiesTest.FLUTTER_EXECUTABLE_ABSOLUTE_PATH_TEST) }
        verify { mockExecSpec.workingDir(BaseFlutterTaskPropertiesTest.sourceDirTest) }
        verify { mockExecSpec.args("--local-engine", BaseFlutterTaskPropertiesTest.LOCAL_ENGINE_TEST) }
        verify { mockExecSpec.args("--local-engine-src-path", BaseFlutterTaskPropertiesTest.LOCAL_ENGINE_SRC_PATH_TEST) }
        verify { mockExecSpec.args("--local-engine-host", BaseFlutterTaskPropertiesTest.LOCAL_ENGINE_HOST_TEST) }
        verify { mockExecSpec.args("--verbose") }
        verify { mockExecSpec.args("assemble") }
        verify { mockExecSpec.args("--no-version-check") }
        verify { mockExecSpec.args("--depfile", "${BaseFlutterTaskPropertiesTest.intermediateDirFileTest}/flutter_build.d") }
        verify { mockExecSpec.args("--output", "${BaseFlutterTaskPropertiesTest.intermediateDirFileTest}") }
        verify { mockExecSpec.args("--performance-measurement-file=${BaseFlutterTaskPropertiesTest.PERFORMANCE_MEASUREMENT_FILE_TEST}") }
        verify { mockExecSpec.args("-dTargetFile=${BaseFlutterTaskPropertiesTest.FLUTTER_TARGET_FILE_PATH}") }
        verify { mockExecSpec.args("-dTargetPlatform=android") }
        verify { mockExecSpec.args("-dBuildMode=$buildModeString") }
        verify { mockExecSpec.args("-dTrackWidgetCreation=${true}") }
        verify { mockExecSpec.args("-dSplitDebugInfo=${BaseFlutterTaskPropertiesTest.SPLIT_DEBUG_INFO_TEST}") }
        verify { mockExecSpec.args("-dTreeShakeIcons=true") }
        verify { mockExecSpec.args("-dDartObfuscation=true") }
        verify { mockExecSpec.args("--DartDefines=${BaseFlutterTaskPropertiesTest.DART_DEFINES_TEST}") }
        verify { mockExecSpec.args("-dBundleSkSLPath=${BaseFlutterTaskPropertiesTest.BUNDLE_SK_SL_PATH_TEST}") }
        verify { mockExecSpec.args("-dCodeSizeDirectory=${BaseFlutterTaskPropertiesTest.CODE_SIZE_DIRECTORY_TEST}") }
        verify { mockExecSpec.args("-dFlavor=${BaseFlutterTaskPropertiesTest.FLAVOR_TEST}") }
        verify { mockExecSpec.args("--ExtraGenSnapshotOptions=${BaseFlutterTaskPropertiesTest.EXTRA_GEN_SNAPSHOT_OPTIONS_TEST}") }
        verify { mockExecSpec.args("-dFrontendServerStarterPath=${BaseFlutterTaskPropertiesTest.FRONTEND_SERVER_STARTER_PATH_TEST}") }
        verify { mockExecSpec.args("--ExtraFrontEndOptions=${BaseFlutterTaskPropertiesTest.EXTRA_FRONTEND_OPTIONS_TEST}") }
        verify { mockExecSpec.args("-dAndroidArchs=${BaseFlutterTaskPropertiesTest.TARGET_PLATFORM_VALUES_JOINED_LIST}") }
        verify { mockExecSpec.args("-dMinSdkVersion=${BaseFlutterTaskPropertiesTest.MIN_SDK_VERSION_TEST}") }
        verify { mockExecSpec.args(ruleNamesList) }
    }

    @Test
    fun `createSpecActionFromTask creates the correct build configurations when properties are null`() {
        val buildModeString = "debug"

        // Create necessary mocks.
        val baseFlutterTask = mockk<BaseFlutterTask>()
        val mockExecSpec = mockk<ExecSpec>()
        val mockProcessForkOptions = mockk<ProcessForkOptions>()

        // Mock return values of properties.
        every { baseFlutterTask.sourceDir } returns mockDirectoryProperty(BaseFlutterTaskPropertiesTest.sourceDirTest)
        every { baseFlutterTask.flutterExecutable } returns mockRegularFileProperty(BaseFlutterTaskPropertiesTest.flutterExecutableTest)
        every { baseFlutterTask.targetPath } returns mockProperty(BaseFlutterTaskPropertiesTest.FLUTTER_TARGET_FILE_PATH)
        every { baseFlutterTask.localEngine } returns mockProperty(null)
        every { baseFlutterTask.localEngineSrcPath } returns mockProperty(null)
        every { baseFlutterTask.localEngineHost } returns mockProperty(null)
        every { baseFlutterTask.verbose } returns mockProperty(true)
        every { baseFlutterTask.intermediateDir } returns mockDirectoryProperty(BaseFlutterTaskPropertiesTest.intermediateDirFileTest)
        every { baseFlutterTask.performanceMeasurementFile } returns mockProperty(null)
        every { baseFlutterTask.buildMode } returns mockProperty(buildModeString)
        every { baseFlutterTask.flutterRoot } returns mockDirectoryProperty(BaseFlutterTaskPropertiesTest.flutterRootTest)
        every { baseFlutterTask.trackWidgetCreation } returns mockProperty(null)
        every { baseFlutterTask.splitDebugInfo } returns mockProperty(null)
        every { baseFlutterTask.treeShakeIcons } returns mockProperty(null)
        every { baseFlutterTask.dartObfuscation } returns mockProperty(null)
        every { baseFlutterTask.dartDefines } returns mockProperty(null)
        every { baseFlutterTask.bundleSkSLPath } returns mockProperty(null)
        every { baseFlutterTask.codeSizeDirectory } returns mockProperty(null)
        every { baseFlutterTask.flavor } returns mockProperty(null)
        every { baseFlutterTask.extraGenSnapshotOptions } returns mockProperty(null)
        every { baseFlutterTask.frontendServerStarterPath } returns mockProperty(null)
        every { baseFlutterTask.extraFrontEndOptions } returns mockProperty(null)
        every { baseFlutterTask.targetPlatformValues } returns mockListProperty(BaseFlutterTaskPropertiesTest.targetPlatformValuesList)
        every { baseFlutterTask.minSdkVersion } returns mockProperty(BaseFlutterTaskPropertiesTest.MIN_SDK_VERSION_TEST)
        every { baseFlutterTask.deferredComponents } returns mockProperty(null)

        val execSpecActionFromTask = BaseFlutterTaskHelper.createExecSpecActionFromTask(baseFlutterTask)

        // Mock the method calls. We collapse all the args mock calls into four calls.
        every { mockExecSpec.executable(any<String>()) } returns mockExecSpec
        every { mockExecSpec.workingDir(any()) } returns mockProcessForkOptions
        every { mockExecSpec.args(any<String>(), any()) } returns mockExecSpec
        every { mockExecSpec.args(any<String>(), any()) } returns mockExecSpec
        every { mockExecSpec.args(any<String>()) } returns mockExecSpec
        every { mockExecSpec.args(any<List<String>>()) } returns mockExecSpec

        // Generate rule names for verification and can only be generated after buildMode is mocked.
        val ruleNamesList: List<String> = BaseFlutterTaskHelper.generateRuleNames(baseFlutterTask)

        execSpecActionFromTask.execute(mockExecSpec)

        // After execution, we verify the functions are actually being
        // called with the expected argument passed in.
        verify { mockExecSpec.executable(BaseFlutterTaskPropertiesTest.FLUTTER_EXECUTABLE_ABSOLUTE_PATH_TEST) }
        verify { mockExecSpec.workingDir(BaseFlutterTaskPropertiesTest.sourceDirTest) }
        verify { mockExecSpec.args("--verbose") }
        verify { mockExecSpec.args("assemble") }
        verify { mockExecSpec.args("--no-version-check") }
        verify { mockExecSpec.args("--depfile", "${BaseFlutterTaskPropertiesTest.intermediateDirFileTest}/flutter_build.d") }
        verify { mockExecSpec.args("--output", "${BaseFlutterTaskPropertiesTest.intermediateDirFileTest}") }
        verify { mockExecSpec.args("-dTargetFile=${BaseFlutterTaskPropertiesTest.FLUTTER_TARGET_FILE_PATH}") }
        verify { mockExecSpec.args("-dTargetPlatform=android") }
        verify { mockExecSpec.args("-dBuildMode=$buildModeString") }
        verify { mockExecSpec.args("-dAndroidArchs=${BaseFlutterTaskPropertiesTest.TARGET_PLATFORM_VALUES_JOINED_LIST}") }
        verify { mockExecSpec.args("-dMinSdkVersion=${BaseFlutterTaskPropertiesTest.MIN_SDK_VERSION_TEST}") }
        verify { mockExecSpec.args(ruleNamesList) }
    }

    @Test
    fun `buildBundle calls the correct methods`() {
        val baseFlutterTask = mockk<BaseFlutterTask>()
        val mockLoggingManager = mockk<LoggingManager>()
        val mockFile = File(".")
        val mockProject = mockk<org.gradle.api.internal.project.ProjectInternal>()

        every { baseFlutterTask.sourceDir } returns mockDirectoryProperty(mockFile)
        every { baseFlutterTask.intermediateDir } returns mockDirectoryProperty(BaseFlutterTaskPropertiesTest.intermediateDirFileTest)
        every { baseFlutterTask.logging } returns mockLoggingManager
        every { mockLoggingManager.captureStandardError(any()) } returns mockLoggingManager
        every { baseFlutterTask.project } returns mockProject
        val mockExecOperations = mockk<ExecOperations>()
        every {
            mockProject.serviceOf<ExecOperations>()
        } returns mockExecOperations
        every { mockExecOperations.exec(any<Action<ExecSpec>>()) } returns mockk()

        // Also mock properties needed by buildBundle / createExecSpecActionFromTask
        every { baseFlutterTask.flutterExecutable } returns mockRegularFileProperty(BaseFlutterTaskPropertiesTest.flutterExecutableTest)
        every { baseFlutterTask.targetPath } returns mockProperty(BaseFlutterTaskPropertiesTest.FLUTTER_TARGET_FILE_PATH)
        every { baseFlutterTask.localEngine } returns mockProperty(null)
        every { baseFlutterTask.localEngineHost } returns mockProperty(null)
        every { baseFlutterTask.verbose } returns mockProperty(true)
        every { baseFlutterTask.buildMode } returns mockProperty("debug")
        every { baseFlutterTask.targetPlatformValues } returns mockListProperty(BaseFlutterTaskPropertiesTest.targetPlatformValuesList)
        every { baseFlutterTask.minSdkVersion } returns mockProperty(BaseFlutterTaskPropertiesTest.MIN_SDK_VERSION_TEST)
        every { baseFlutterTask.deferredComponents } returns mockProperty(null)

        BaseFlutterTaskHelper.buildBundle(baseFlutterTask)
    }

    @Test
    fun `getDependencyFiles returns a FileCollection of dependency file(s)`() {
        val baseFlutterTask = mockk<BaseFlutterTask>()
        val project = mockk<Project>()
        val configFileCollection = mockk<ConfigurableFileCollection>()

        every { baseFlutterTask.project } returns project
        val mockIntermediateDirFile = BaseFlutterTaskPropertiesTest.intermediateDirFileTest
        every { baseFlutterTask.intermediateDir } returns mockDirectoryProperty(mockIntermediateDirFile)

        every { project.files() } returns configFileCollection
        every { project.files(any()) } returns configFileCollection
        every { configFileCollection.plus(configFileCollection) } returns configFileCollection

        BaseFlutterTaskHelper.getDependenciesFiles(baseFlutterTask)
        verify { project.files() }
        verify { project.files(any()) }
    }
}
