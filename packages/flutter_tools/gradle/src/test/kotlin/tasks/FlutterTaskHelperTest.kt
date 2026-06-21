// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.tasks

import com.flutter.gradle.FlutterPluginConstants
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import org.gradle.api.Action
import org.gradle.api.Project
import org.gradle.api.file.ConfigurableFileCollection
import org.gradle.api.file.CopySpec
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.file.FileCollection
import org.gradle.api.provider.ListProperty
import org.gradle.api.provider.Property
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.nio.file.Path
import kotlin.test.Test

class FlutterTaskHelperTest {
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

    private inline fun <reified T : Any> mockListProperty(value: List<T>?): ListProperty<T> {
        val mockProp = mockk<ListProperty<T>>(relaxed = true)
        every { mockProp.orNull } returns value
        if (value != null) {
            every { mockProp.get() } returns value
        }
        return mockProp
    }

    @Test
    fun `getAssetsDirectory returns correct path`() {
        val flutterTask = mockk<FlutterTask>()
        val fakeAssetsDirectory = File("${File.separator}path${File.separator}to${File.separator}assets")
        val expectedPath = "${fakeAssetsDirectory}${File.separator}flutter_assets"

        every { flutterTask.outputDirectory } returns fakeAssetsDirectory
        val result = FlutterTaskHelper.getAssetsDirectory(flutterTask)
        assert(result == expectedPath)
    }

    @Test
    fun `getAssets returns correct CopySpec`() {
        val project = mockk<Project>()
        val flutterTask = mockk<FlutterTask>()
        val mockFile = File("${File.separator}path${File.separator}to${File.separator}intermediate")
        val mockCopySpec = mockk<CopySpec>()
        val copySpecActionSlot = slot<Action<in CopySpec>>()

        val mockDirectoryProperty = mockDirectoryProperty(mockFile)
        every { flutterTask.intermediateDir } returns mockDirectoryProperty
        every { project.copySpec(capture(copySpecActionSlot)) } returns mockk()

        FlutterTaskHelper.getAssets(project, flutterTask)
        every { mockCopySpec.from(mockDirectoryProperty) } returns mockCopySpec
        every { mockCopySpec.include(FlutterTaskHelper.FLUTTER_ASSETS_INCLUDE_DIRECTORY) } returns mockCopySpec
        copySpecActionSlot.captured.execute(mockCopySpec)
        verify { mockCopySpec.from(mockDirectoryProperty) }
        verify { mockCopySpec.include(FlutterTaskHelper.FLUTTER_ASSETS_INCLUDE_DIRECTORY) }
    }

    @Test
    fun `getSnapshots returns correct CopySpec for release build`() {
        val project = mockk<Project>()
        val flutterTask = mockk<FlutterTask>()
        val mockCopySpec = mockk<CopySpec>()
        val copySpecActionSlot = slot<Action<in CopySpec>>()
        val fakeIntermediateDirectory = File("${File.separator}path${File.separator}to${File.separator}intermediate")

        val mockDirectoryProperty = mockDirectoryProperty(fakeIntermediateDirectory)
        every { flutterTask.intermediateDir } returns mockDirectoryProperty
        every { flutterTask.buildMode } returns mockProperty("release")
        every { flutterTask.targetPlatformValues } returns mockListProperty(listOf("arm64-v8a", "x64"))
        every { project.copySpec(capture(copySpecActionSlot)) } returns mockk()

        FlutterTaskHelper.getSnapshots(project, flutterTask)
        every { mockCopySpec.from(mockDirectoryProperty) } returns mockCopySpec
        every { mockCopySpec.include(any<String>()) } returns mockCopySpec
        copySpecActionSlot.captured.execute(mockCopySpec)

        verify { mockCopySpec.from(mockDirectoryProperty) }
        verify { mockCopySpec.include("${FlutterPluginConstants.PLATFORM_ARCH_MAP["arm64-v8a"]}${File.separator}app.so") }
        verify { mockCopySpec.include("${FlutterPluginConstants.PLATFORM_ARCH_MAP["x64"]}${File.separator}app.so") }
    }

    @Test
    fun `getSnapshots returns correct CopySpec for debug build`() {
        val project = mockk<Project>()
        val flutterTask = mockk<FlutterTask>()
        val mockCopySpec = mockk<CopySpec>()
        val copySpecActionSlot = slot<Action<in CopySpec>>()
        val fakeIntermediateDirectory = File("${File.separator}path${File.separator}to${File.separator}intermediate")

        val mockDirectoryProperty = mockDirectoryProperty(fakeIntermediateDirectory)
        every { flutterTask.intermediateDir } returns mockDirectoryProperty
        every { flutterTask.buildMode } returns mockProperty("debug")
        every { flutterTask.targetPlatformValues } returns mockListProperty(listOf("arm64-v8a", "x64"))
        every { project.copySpec(capture(copySpecActionSlot)) } returns mockk()

        FlutterTaskHelper.getSnapshots(project, flutterTask)
        every { mockCopySpec.from(mockDirectoryProperty) } returns mockCopySpec
        every { mockCopySpec.include(any<String>()) } returns mockCopySpec
        copySpecActionSlot.captured.execute(mockCopySpec)

        verify { mockCopySpec.from(mockDirectoryProperty) }
        verify(exactly = 0) { mockCopySpec.include(any<String>()) }
    }

    @Test
    fun `getSourceFiles returns files when dependenciesFile exists`(
        @TempDir tempDir: Path
    ) {
        val mockProjectFileCollection = mockk<ConfigurableFileCollection>(relaxed = true)
        val mockDependenciesFileCollection = mockk<FileCollection>()
        val project = mockk<Project>()
        val mockFlutterTask = mockk<FlutterTask>()

        every { project.files() } returns mockProjectFileCollection
        every { project.files(any()) } returns mockProjectFileCollection

        val mockDirectoryProperty = mockDirectoryProperty(tempDir.toFile())
        every { mockFlutterTask.intermediateDir } returns mockDirectoryProperty
        every { mockFlutterTask.getDependenciesFiles() } returns mockDependenciesFileCollection
        val dependenciesFile =
            tempDir
                .resolve("${tempDir.toFile().path}${File.separator}flutter_build.d")
                .toFile()
        dependenciesFile.writeText(
            " ${tempDir.toFile().path}${File.separator}pre${File.separator}delimiter${File.separator}one ${tempDir.toFile().path}${File.separator}pre${File.separator}delimiter${File.separator}two: ${tempDir.toFile().path}${File.separator}post${File.separator}delimiter${File.separator}one ${tempDir.toFile().path}${File.separator}post${File.separator}delimiter${File.separator}two"
        )
        every { mockDependenciesFileCollection.iterator() } returns (mutableListOf(dependenciesFile).iterator())

        FlutterTaskHelper.getSourceFiles(project, mockFlutterTask)

        verify {
            project.files(
                listOf(
                    "${tempDir.toFile().path}${File.separator}post${File.separator}delimiter${File.separator}one",
                    "${tempDir.toFile().path}${File.separator}post${File.separator}delimiter${File.separator}two"
                )
            )
        }

        verify { project.files("pubspec.yaml") }
    }

    @Test
    fun `getSourceFiles correctly replaces escaped spaces`(
        @TempDir tempDir: Path
    ) {
        val mockProjectFileCollection = mockk<ConfigurableFileCollection>(relaxed = true)
        val mockDependenciesFileCollection = mockk<FileCollection>()
        val project = mockk<Project>()
        val mockFlutterTask = mockk<FlutterTask>()

        every { project.files() } returns mockProjectFileCollection
        every { project.files(any()) } returns mockProjectFileCollection

        val mockDirectoryProperty = mockDirectoryProperty(tempDir.toFile())
        every { mockFlutterTask.intermediateDir } returns mockDirectoryProperty
        every { mockFlutterTask.getDependenciesFiles() } returns mockDependenciesFileCollection
        val dependenciesFile =
            tempDir
                .resolve("${tempDir.toFile().path}${File.separator}flutter_build.d")
                .toFile()
        dependenciesFile.writeText(
            " ${tempDir.toFile().path}${File.separator}pre${File.separator}delimiter\\ space${File.separator}one: ${tempDir.toFile().path}${File.separator}post${File.separator}delimiter\\ space${File.separator}one"
        )
        every { mockDependenciesFileCollection.iterator() } returns (mutableListOf(dependenciesFile).iterator())

        FlutterTaskHelper.getSourceFiles(project, mockFlutterTask)

        verify {
            project.files(
                listOf(
                    "${tempDir.toFile().path}${File.separator}post${File.separator}delimiter space${File.separator}one"
                )
            )
        }

        verify { project.files("pubspec.yaml") }
    }

    @Test
    fun `getOutputFiles returns files when dependenciesFile exists`(
        @TempDir tempDir: Path
    ) {
        val mockProjectFileCollection = mockk<ConfigurableFileCollection>(relaxed = true)
        val mockDependenciesFileCollection = mockk<FileCollection>()
        val project = mockk<Project>()
        val mockFlutterTask = mockk<FlutterTask>()

        every { project.files() } returns mockProjectFileCollection
        every { project.files(any()) } returns mockProjectFileCollection

        val mockDirectoryProperty = mockDirectoryProperty(tempDir.toFile())
        every { mockFlutterTask.intermediateDir } returns mockDirectoryProperty
        every { mockFlutterTask.getDependenciesFiles() } returns mockDependenciesFileCollection
        val dependenciesFile =
            tempDir
                .resolve("${tempDir.toFile().path}${File.separator}flutter_build.d")
                .toFile()
        dependenciesFile.writeText(
            " ${tempDir.toFile().path}${File.separator}pre${File.separator}delimiter${File.separator}one ${tempDir.toFile().path}${File.separator}pre${File.separator}delimiter${File.separator}two: ${tempDir.toFile().path}${File.separator}post${File.separator}delimiter${File.separator}one ${tempDir.toFile().path}${File.separator}post${File.separator}delimiter${File.separator}two"
        )
        every { mockDependenciesFileCollection.iterator() } returns (mutableListOf(dependenciesFile).iterator())

        FlutterTaskHelper.getOutputFiles(project, mockFlutterTask)

        verify {
            project.files(
                listOf(
                    "${tempDir.toFile().path}${File.separator}pre${File.separator}delimiter${File.separator}one",
                    "${tempDir.toFile().path}${File.separator}pre${File.separator}delimiter${File.separator}two"
                )
            )
        }

        verify(exactly = 0) { project.files("pubspec.yaml") }
    }
}
