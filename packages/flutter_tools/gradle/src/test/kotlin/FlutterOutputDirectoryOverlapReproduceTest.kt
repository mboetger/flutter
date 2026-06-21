// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import com.android.build.api.dsl.ApplicationBuildType
import com.android.build.api.dsl.ApplicationDefaultConfig
import com.android.build.api.dsl.ApplicationExtension
import com.android.build.api.dsl.CommonExtension
import com.android.build.api.variant.AndroidComponentsExtension
import com.android.build.gradle.AbstractAppExtension
import com.android.build.gradle.BaseExtension
import com.android.build.gradle.api.AndroidSourceDirectorySet
import com.android.build.gradle.api.AndroidSourceSet
import com.android.build.gradle.api.ApplicationVariant
import com.android.build.gradle.api.BaseVariantOutput
import com.android.build.gradle.internal.core.InternalBaseVariant
import com.android.build.gradle.tasks.MergeSourceSetFolders
import com.android.build.gradle.tasks.ProcessAndroidResources
import com.flutter.gradle.tasks.FlutterTask
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.slot
import io.mockk.unmockkObject
import io.mockk.verify
import org.gradle.api.Action
import org.gradle.api.DomainObjectCollection
import org.gradle.api.DomainObjectSet
import groovy.lang.Closure
import org.gradle.api.Project
import org.gradle.api.Task
import org.gradle.api.file.Directory
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.provider.Provider
import org.gradle.api.tasks.Copy
import org.gradle.api.tasks.TaskContainer
import org.gradle.api.tasks.TaskProvider
import org.gradle.api.tasks.bundling.Jar
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.Assertions.assertNotEquals
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.nio.file.Path
import kotlin.io.path.writeText
import kotlin.test.Test

class FlutterOutputDirectoryOverlapReproduceTest {

    @AfterEach
    fun tearDown() {
        unmockkObject(NativePluginLoaderReflectionBridge)
    }

    @Test
    fun `copyFlutterAssets task must NOT share the same output directory as mergeAssets task`(
        @TempDir tempDir: Path
    ) {
        val projectDir = tempDir.resolve("project-dir").resolve("android").resolve("app")
        projectDir.toFile().mkdirs()
        val settingsFile = projectDir.parent.resolve("settings.gradle")
        settingsFile.writeText("empty for now")
        val fakeFlutterSdkDir = tempDir.resolve("fake-flutter-sdk")
        fakeFlutterSdkDir.toFile().mkdirs()
        val fakeCacheDir = fakeFlutterSdkDir.resolve("bin").resolve("cache")
        fakeCacheDir.toFile().mkdirs()
        val fakeEngineStampFile = fakeCacheDir.resolve("engine.stamp")
        fakeEngineStampFile.writeText(FAKE_ENGINE_STAMP)
        val fakeEngineRealmFile = fakeCacheDir.resolve("engine.realm")
        fakeEngineRealmFile.writeText(FAKE_ENGINE_REALM)
        val project = mockk<Project>(relaxed = true)
        val mockAbstractAppExtension =
            mockk<AbstractAppExtension>(
                moreInterfaces = arrayOf(ApplicationExtension::class),
                relaxed = true
            )
        every { project.extensions.findByType(AbstractAppExtension::class.java) } returns mockAbstractAppExtension
        every { project.extensions.getByType(AbstractAppExtension::class.java) } returns mockAbstractAppExtension
        every { project.extensions.findByName("android") } returns mockAbstractAppExtension
        val mockAndroidComponentsExtension = mockk<AndroidComponentsExtension<*, *, *>>(relaxed = true)
        every { project.extensions.getByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        every { mockAndroidComponentsExtension.selector() } returns
            mockk {
                every { all() } returns mockk()
            }
        every { project.projectDir } returns projectDir.toFile()
        every { project.findProperty("flutter.sdk") } returns fakeFlutterSdkDir.toString()
        every { project.file(fakeFlutterSdkDir.toString()) } returns fakeFlutterSdkDir.toFile()
        every { project.file("local.properties") } returns projectDir.resolve("local.properties").toFile()
        val flutterExtension = FlutterExtension()
        every { project.extensions.create("flutter", any<Class<*>>()) } returns flutterExtension
        every { project.extensions.findByType(FlutterExtension::class.java) } returns flutterExtension
        val mockBaseExtension = mockk<BaseExtension>(relaxed = true)
        val mockCommonExtension = mockk<CommonExtension<*, *, *, *, *, *>>(relaxed = true)
        val mockDebugBuildType = mockk<ApplicationBuildType>(relaxed = true)
        val mockReleaseBuildType = mockk<ApplicationBuildType>(relaxed = true)

        val mockApplicationExtension = mockAbstractAppExtension as ApplicationExtension
        every { mockApplicationExtension.buildTypes.getByName("debug") } returns mockDebugBuildType
        every { mockApplicationExtension.buildTypes.getByName("release") } returns mockReleaseBuildType

        every { mockCommonExtension.buildTypes.getByName("debug") } returns mockDebugBuildType
        every { mockCommonExtension.buildTypes.getByName("release") } returns mockReleaseBuildType

        every { project.extensions.findByType(BaseExtension::class.java) } returns mockBaseExtension
        every { project.extensions.findByType(CommonExtension::class.java) } returns mockCommonExtension
        every { project.extensions.findByType(ApplicationExtension::class.java) } returns mockApplicationExtension
        every { project.extensions.getByType(ApplicationExtension::class.java) } returns mockApplicationExtension

        val mockApplicationDefaultConfig =
            mockk<com.android.build.gradle.internal.dsl.DefaultConfig>(
                moreInterfaces = arrayOf(ApplicationDefaultConfig::class),
                relaxed = true
            )
        every { mockApplicationExtension.defaultConfig } returns mockApplicationDefaultConfig
        every { project.rootProject } returns project
        every { project.state.failure as Throwable? } returns null
        val mockDirectory = mockk<Directory>(relaxed = true)
        every { project.layout.buildDirectory.get() } returns mockDirectory
        val mockAssetsDirProvider = mockk<Provider<Directory>>(relaxed = true)
        val mockAssetsDir = mockk<Directory>(relaxed = true)
        val realAssetsDirFile = projectDir.resolve("intermediates/flutter/debug/assets").toFile()
        every { mockAssetsDir.asFile } returns realAssetsDirFile
        every { mockAssetsDirProvider.get() } returns mockAssetsDir
        every { project.layout.buildDirectory.dir(any<String>()) } returns mockAssetsDirProvider
        val mockAndroidSourceSet = mockk<AndroidSourceSet>(relaxed = true)
        val mockAndroidSourceDirectorySet = mockk<AndroidSourceDirectorySet>(relaxed = true)
        every { mockAndroidSourceSet.jniLibs.srcDir(any()) } returns mockAndroidSourceDirectorySet
        every { mockAbstractAppExtension.sourceSets.getByName("main") } returns mockAndroidSourceSet
        
        mockkObject(NativePluginLoaderReflectionBridge)
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns listOf()
        every { project.extraProperties } returns mockk()
        every { project.file(flutterExtension.source!!) } returns mockk()
        
        val taskContainer = mockk<TaskContainer>(relaxed = true)
        every { project.tasks } returns taskContainer
        val copyTaskActionCaptor = slot<Action<Copy>>()
        
        // Return a mock Copy task so we can capture and test its configuration
        val copyTask = mockk<Copy>(relaxed = true)
        var capturedDestinationDir: File = projectDir.resolve("dummy").toFile()
        every { copyTask.into(any()) } answers {
            val arg = it.invocation.args[0]
            if (arg is File) {
                capturedDestinationDir = arg
            } else if (arg is Directory) {
                capturedDestinationDir = arg.asFile
            } else if (arg is DirectoryProperty) {
                capturedDestinationDir = arg.get().asFile
            } else if (arg is Provider<*>) {
                val value = arg.get()
                if (value is Directory) {
                    capturedDestinationDir = value.asFile
                } else if (value is File) {
                    capturedDestinationDir = value
                } else {
                    capturedDestinationDir = project.file(value.toString())
                }
            } else {
                capturedDestinationDir = project.file(arg.toString())
            }
            copyTask
        }
        every { copyTask.destinationDir } answers { capturedDestinationDir }
        
        val mockVariant = mockk<ApplicationVariant>(relaxed = true)
        every { mockVariant.name } returns "debug"
        every { mockVariant.buildType.name } returns "debug"
        every { mockVariant.flavorName } returns ""
        val mergedFlavor = mockk<InternalBaseVariant.MergedFlavor>(relaxed = true)
        every { mockVariant.mergedFlavor } returns mergedFlavor
        val apiLevel = mockk<com.android.builder.model.ApiVersion>(relaxed = true)
        every { apiLevel.apiLevel } returns 21
        every { mergedFlavor.minSdkVersion } returns apiLevel
        val variantOutput = mockk<BaseVariantOutput>(relaxed = true)
        val variantOutputCollection = TestVariantOutputCollection()
        variantOutputCollection.addOutput(variantOutput)
        every { mockVariant.outputs } returns variantOutputCollection
        val processResourcesProvider = mockk<TaskProvider<ProcessAndroidResources>>(relaxed = true)
        every { processResourcesProvider.hint(ProcessAndroidResources::class).get() } returns mockk<ProcessAndroidResources>(relaxed = true)
        every { variantOutput.processResourcesProvider } returns processResourcesProvider
        val assembleTask = mockk<Task>(relaxed = true)
        val assembleTaskProvider = mockk<TaskProvider<Task>>(relaxed = true)
        every { assembleTaskProvider.get() } returns assembleTask
        every { mockVariant.assembleProvider } returns assembleTaskProvider
        
        val variants = listOf(mockVariant)
        val variantsIterator = mockk<MutableIterator<ApplicationVariant>>()
        every { variantsIterator.hasNext() } returns true andThen false
        every { variantsIterator.next() } returns mockVariant
        val variantCollection = mockk<DomainObjectSet<ApplicationVariant>>()
        every { mockAbstractAppExtension.applicationVariants } returns variantCollection
        every { variantCollection.iterator() } returns variantsIterator
        every {
            variantCollection.configureEach(any<Action<ApplicationVariant>>())
        } answers {
            val action = it.invocation.args[0] as Action<ApplicationVariant>
            variants.forEach { action.execute(it) }
        }
        
        val mockMergeAssets = mockk<MergeSourceSetFolders>(relaxed = true)
        val mockMergeAssetsOutputDir = projectDir.resolve("mergeDebugAssets").toFile()
        
        // Mock outputDir Property
        val mockOutputDirProperty = mockk<DirectoryProperty>(relaxed = true)
        val mockDirectoryEntry = mockk<Directory>(relaxed = true)
        every { mockDirectoryEntry.asFile } returns mockMergeAssetsOutputDir
        every { mockOutputDirProperty.get() } returns mockDirectoryEntry
        every { mockMergeAssets.outputDir } returns mockOutputDirProperty
        
        every { mockVariant.mergeAssetsProvider.hint(MergeSourceSetFolders::class).get() } returns mockMergeAssets
        
        val flutterTask = mockk<FlutterTask>(relaxed = true)
        val copySpec = mockk<org.gradle.api.file.CopySpec>(relaxed = true)
        every { (flutterTask).assets } returns copySpec
        val flutterTaskProvider = mockk<TaskProvider<FlutterTask>>(relaxed = true)
        every { flutterTaskProvider.hint(FlutterTask::class).get() } returns flutterTask
        every {
            taskContainer.register(
                match { it.contains("compileFlutterBuild") },
                any<Class<FlutterTask>>(),
                any()
            )
        } answers {
            flutterTaskProvider
        }
        
        val mockCopyTaskProvider = mockk<TaskProvider<Copy>>(relaxed = true)
        every { mockCopyTaskProvider.hint(Copy::class).get() } returns copyTask
        every {
            taskContainer.register(
                match { it.startsWith("copyFlutterAssets") },
                eq(Copy::class.java),
                capture(copyTaskActionCaptor)
            )
        } answers {
            mockCopyTaskProvider
        }
        
        val mockJarTaskProvider = mockk<TaskProvider<Jar>>(relaxed = true)
        every { mockJarTaskProvider.hint(Jar::class).get() } returns mockk(relaxed = true)
        every {
            taskContainer.register(
                match { it.contains("packJniLibs") },
                eq(Jar::class.java),
                any()
            )
        } answers {
            mockJarTaskProvider
        }
        
        val mockTaskProvider = mockk<TaskProvider<Task>>(relaxed = true)
        every { mockTaskProvider.hint(Task::class).get() } returns mockk(relaxed = true)
        every { taskContainer.named(any<String>()) } answers {
            val name = it.invocation.args[0] as String
            if (name.contains("package") && name.contains("Assets")) {
                throw org.gradle.api.UnknownTaskException("Task not found")
            }
            mockTaskProvider
        }
        
        val flutterPlugin = FlutterPlugin()
        flutterPlugin.apply(project)
        
        // Execute the copyFlutterAssets configuration action
        copyTaskActionCaptor.captured.execute(copyTask)
        
        // The destinationDir of copyFlutterAssets must NOT be the outputDir of mergeAssets task
        assertNotEquals(mockMergeAssetsOutputDir, copyTask.destinationDir)
    }

    companion object {
        const val FAKE_ENGINE_STAMP = "901b0f1afe77c3555abee7b86a26aaa37f131379"
        const val FAKE_ENGINE_REALM = "made_up_realm"
    }
}

open class TestVariantOutputCollection : DomainObjectCollection<BaseVariantOutput> {
    private val list = mutableListOf<BaseVariantOutput>()
    
    fun addOutput(output: BaseVariantOutput) {
        list.add(output)
    }
    
    override val size: Int get() = list.size
    override fun contains(element: BaseVariantOutput): Boolean = list.contains(element)
    override fun containsAll(elements: Collection<BaseVariantOutput>): Boolean = list.containsAll(elements)
    override fun isEmpty(): Boolean = list.isEmpty()
    override fun iterator(): MutableIterator<BaseVariantOutput> = list.iterator()
    
    override fun add(element: BaseVariantOutput): Boolean = throw UnsupportedOperationException()
    override fun addAll(elements: Collection<BaseVariantOutput>): Boolean = throw UnsupportedOperationException()
    override fun clear() = throw UnsupportedOperationException()
    override fun remove(element: BaseVariantOutput): Boolean = throw UnsupportedOperationException()
    override fun removeAll(elements: Collection<BaseVariantOutput>): Boolean = throw UnsupportedOperationException()
    override fun retainAll(elements: Collection<BaseVariantOutput>): Boolean = throw UnsupportedOperationException()
    
    override fun addLater(provider: Provider<out BaseVariantOutput>) = throw UnsupportedOperationException()
    override fun addAllLater(provider: Provider<out Iterable<BaseVariantOutput>>) = throw UnsupportedOperationException()
    override fun <S : BaseVariantOutput> withType(type: Class<S>): DomainObjectCollection<S> = throw UnsupportedOperationException()
    override fun <S : BaseVariantOutput> withType(type: Class<S>, configureAction: Action<in S>): DomainObjectCollection<S> = throw UnsupportedOperationException()
    override fun <S : BaseVariantOutput> withType(type: Class<S>, configureClosure: Closure<*>): DomainObjectCollection<S> = throw UnsupportedOperationException()
    override fun matching(spec: org.gradle.api.specs.Spec<in BaseVariantOutput>): DomainObjectCollection<BaseVariantOutput> = throw UnsupportedOperationException()
    override fun matching(spec: Closure<*>): DomainObjectCollection<BaseVariantOutput> = throw UnsupportedOperationException()
    override fun findAll(spec: Closure<*>): Collection<BaseVariantOutput> = throw UnsupportedOperationException()
    
    override fun all(action: Action<in BaseVariantOutput>) = throw UnsupportedOperationException()
    override fun all(action: Closure<*>) = throw UnsupportedOperationException()
    override fun configureEach(action: Action<in BaseVariantOutput>) = throw UnsupportedOperationException()
    
    override fun whenObjectAdded(action: Action<in BaseVariantOutput>): Action<in BaseVariantOutput> = throw UnsupportedOperationException()
    override fun whenObjectAdded(action: Closure<*>) = throw UnsupportedOperationException()
    override fun whenObjectRemoved(action: Action<in BaseVariantOutput>): Action<in BaseVariantOutput> = throw UnsupportedOperationException()
    override fun whenObjectRemoved(action: Closure<*>) = throw UnsupportedOperationException()
}
