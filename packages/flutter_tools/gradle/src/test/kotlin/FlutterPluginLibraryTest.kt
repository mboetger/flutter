package com.flutter.gradle

import com.android.build.gradle.internal.dsl.BuildType
import com.android.build.api.dsl.LibraryExtension
import org.gradle.api.NamedDomainObjectContainer
import com.android.build.api.variant.AndroidComponentsExtension
import com.android.build.gradle.BaseExtension
import com.android.build.gradle.LibraryExtension as LegacyLibraryExtension
import com.android.build.gradle.api.LibraryVariant
import com.android.build.gradle.internal.core.InternalBaseVariant
import com.android.build.gradle.tasks.MergeSourceSetFolders
import com.android.build.gradle.tasks.ProcessAndroidResources
import com.flutter.gradle.tasks.FlutterTask
import com.flutter.gradle.tasks.PrintTask
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.slot
import io.mockk.verify
import org.gradle.api.Action
import org.gradle.api.Project
import org.gradle.api.Task
import org.gradle.api.file.Directory
import org.gradle.api.tasks.Copy
import org.gradle.api.tasks.TaskContainer
import org.gradle.api.tasks.TaskProvider
import org.gradle.api.internal.DefaultDomainObjectSet
import org.junit.jupiter.api.io.TempDir
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import java.nio.file.Path
import kotlin.io.path.writeText
import kotlin.test.Test
import kotlin.test.assertContains

class FlutterPluginLibraryTest {
    @Test
    fun `FlutterPlugin apply succeeds and configures tasks on standalone library project`(@TempDir tempDir: Path) {
        val projectDir = tempDir.resolve("project-dir").resolve("android").resolve("plugin")
        projectDir.toFile().mkdirs()
        val settingsFile = projectDir.parent.resolve("settings.gradle")
        settingsFile.writeText("empty for now")
        val fakeFlutterSdkDir = tempDir.resolve("fake-flutter-sdk")
        fakeFlutterSdkDir.toFile().mkdirs()
        val fakeCacheDir = fakeFlutterSdkDir.resolve("bin").resolve("cache")
        fakeCacheDir.toFile().mkdirs()
        val fakeEngineStampFile = fakeCacheDir.resolve("engine.stamp")
        fakeEngineStampFile.writeText("12345")
        val fakeEngineRealmFile = fakeCacheDir.resolve("engine.realm")
        fakeEngineRealmFile.writeText("")

        val project = mockk<Project>(relaxed = true)
        val mockLibraryExtension = mockk<LegacyLibraryExtension>(
            moreInterfaces = arrayOf(LibraryExtension::class),
            relaxed = true
        )
        
        // This is a library project, so findByType(ApplicationExtension) should return null
        every { project.extensions.findByType(com.android.build.api.dsl.ApplicationExtension::class.java) } returns null
        every { project.extensions.findByType(com.android.build.gradle.AbstractAppExtension::class.java) } returns null
        
        // It is a library project, so it has LibraryExtension
        every { project.extensions.findByType(LegacyLibraryExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.getByType(LegacyLibraryExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.findByName("android") } returns mockLibraryExtension

        val mockBuildTypes = mockk<NamedDomainObjectContainer<BuildType>>(relaxed = true)
        val mockDebugBuildType = mockk<BuildType>(relaxed = true)
        val mockReleaseBuildType = mockk<BuildType>(relaxed = true)
        every { mockBuildTypes.getByName("debug") } returns mockDebugBuildType
        every { mockBuildTypes.getByName("release") } returns mockReleaseBuildType
        every { mockLibraryExtension.buildTypes } returns mockBuildTypes
        every { project.extensions.findByType(BaseExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.getByType(BaseExtension::class.java) } returns mockLibraryExtension

        val mockAndroidComponentsExtension = mockk<AndroidComponentsExtension<*, *, *>>(relaxed = true)
        every { project.extensions.getByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        every { project.extensions.findByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        val mockSelector = mockk<com.android.build.api.variant.VariantSelector>(relaxed = true)
        every { mockAndroidComponentsExtension.selector() } returns mockSelector
        every { mockSelector.all() } returns mockSelector
        every { mockSelector.withName(any<String>()) } returns mockSelector

        every { project.projectDir } returns projectDir.toFile()
        every { project.findProperty("flutter.sdk") } returns fakeFlutterSdkDir.toString()
        every { project.file(fakeFlutterSdkDir.toString()) } returns fakeFlutterSdkDir.toFile()
        
        // Mock local.properties to avoid NPE on exists()
        every { project.file("local.properties") } returns projectDir.parent.resolve("local.properties").toFile()

        val flutterExtension = FlutterExtension()
        every { project.extensions.create("flutter", any<Class<*>>()) } returns flutterExtension
        every { project.extensions.findByType(FlutterExtension::class.java) } returns flutterExtension

        every { project.rootProject } returns project
        every { project.state.failure as Throwable? } returns null

        // Explicitly mock that there is no host app project (standalone library scenario)
        // Only mock on 'project' to avoid chained mock issues that might overwrite 'project.rootProject'
        every { project.findProject(":app") } returns null

        // mock return of NativePluginLoaderReflectionBridge.getPlugins
        mockkObject(NativePluginLoaderReflectionBridge)
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns listOf()
        every { project.extraProperties } returns mockk()

        // Set up the task container and our task capture
        val taskContainer = mockk<TaskContainer>(relaxed = true)
        every { project.tasks } returns taskContainer
        val mockTaskProvider = mockk<TaskProvider<Task>>(relaxed = true)
        val mockTask = mockk<Task>(relaxed = true)
        every { mockTaskProvider.get() } returns mockTask
        every { taskContainer.named(any<String>()) } returns mockTaskProvider

        val mockVariant = mockk<LibraryVariant>(relaxed = true)
        every { mockVariant.name } returns "debug"
        every { mockVariant.buildType.name } returns "debug"
        every { mockVariant.flavorName } returns ""
        val mergedFlavor = mockk<InternalBaseVariant.MergedFlavor>(relaxed = true)
        every { mockVariant.mergedFlavor } returns mergedFlavor
        val apiLevel = mockk<com.android.builder.model.ApiVersion>(relaxed = true)
        every { apiLevel.apiLevel } returns 21
        every { mergedFlavor.minSdkVersion } returns apiLevel
        
        val variantOutput = mockk<com.android.build.gradle.api.BaseVariantOutput>(relaxed = true)
        val outputsIterator = mockk<MutableIterator<com.android.build.gradle.api.BaseVariantOutput>>()
        every { outputsIterator.hasNext() } returns true andThen false
        every { outputsIterator.next() } returns variantOutput
        val variantOutputCollection = mockk<org.gradle.api.DomainObjectCollection<com.android.build.gradle.api.BaseVariantOutput>>()
        every { variantOutputCollection.iterator() } returns outputsIterator
        every { mockVariant.outputs } returns variantOutputCollection
        
        val processResourcesProvider = mockk<TaskProvider<ProcessAndroidResources>>(relaxed = true)
        every { processResourcesProvider.hint(ProcessAndroidResources::class).get() } returns mockk<ProcessAndroidResources>(relaxed = true)
        every { variantOutput.processResourcesProvider } returns processResourcesProvider
        
        val assembleTask = mockk<Task>(relaxed = true)
        val assembleTaskProvider = mockk<TaskProvider<Task>>(relaxed = true)
        every { assembleTaskProvider.get() } returns assembleTask
        every { mockVariant.assembleProvider } returns assembleTaskProvider
        
        val variants = listOf(mockVariant)
        val variantsIterator = mockk<MutableIterator<LibraryVariant>>()
        every { variantsIterator.hasNext() } returns true andThen false
        every { variantsIterator.next() } returns mockVariant
        val variantCollection = mockk<DefaultDomainObjectSet<LibraryVariant>>()
        every { mockLibraryExtension.libraryVariants } returns variantCollection
        every { variantCollection.iterator() } returns variantsIterator
        every {
            variantCollection.configureEach(any<Action<LibraryVariant>>())
        } answers {
            variants.forEach { firstArg<Action<LibraryVariant>>().execute(it) }
        }
        every {
            variantCollection.all(any<Action<LibraryVariant>>())
        } answers {
            variants.forEach { firstArg<Action<LibraryVariant>>().execute(it) }
        }

        every { mockVariant.mergeAssetsProvider.hint(MergeSourceSetFolders::class).get() } returns
            mockk<MergeSourceSetFolders>(relaxed = true)
        
        val flutterTask = mockk<FlutterTask>(relaxed = true)
        val copySpec = mockk<org.gradle.api.file.CopySpec>(relaxed = true)
        every { flutterTask.assets } returns copySpec
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
        every { mockCopyTaskProvider.hint(Copy::class).get() } returns mockk<Copy>(relaxed = true)
        every {
            taskContainer.register<Copy>(
                match { it.startsWith("copyFlutterAssets") },
                match { it?.simpleName == "Copy" },
                any<Action<in Copy>>()
            )
        } answers {
            mockCopyTaskProvider
        }

        val flutterPlugin = FlutterPlugin()

        // This should succeed without throwing "Project :app doesn't exist"
        // and should configure the library variants.
        flutterPlugin.apply(project)

        // Verify that it registered the compile and copy tasks for the library variant
        verify {
            taskContainer.register(
                "compileFlutterBuildDebug",
                FlutterTask::class.java,
                any<Action<in FlutterTask>>()
            )
            taskContainer.register(
                "copyFlutterAssetsDebug",
                Copy::class.java,
                any<Action<in Copy>>()
            )
        }
    }

    @Test
    fun `FlutterPlugin apply fails when hostAppProjectName is explicitly set but project does not exist`(@TempDir tempDir: Path) {
        val projectDir = tempDir.resolve("project-dir").resolve("android").resolve("plugin")
        projectDir.toFile().mkdirs()
        
        val project = mockk<Project>(relaxed = true)
        val mockLibraryExtension = mockk<LegacyLibraryExtension>(
            moreInterfaces = arrayOf(LibraryExtension::class),
            relaxed = true
        )
        every { project.extensions.findByType(com.android.build.api.dsl.ApplicationExtension::class.java) } returns null
        every { project.extensions.findByType(com.android.build.gradle.AbstractAppExtension::class.java) } returns null
        every { project.extensions.findByType(LegacyLibraryExtension::class.java) } returns mockLibraryExtension

        val mockAndroidComponentsExtension = mockk<AndroidComponentsExtension<*, *, *>>(relaxed = true)
        every { project.extensions.getByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        every { project.extensions.findByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        
        val fakeFlutterSdkDir = tempDir.resolve("fake-flutter-sdk")
        fakeFlutterSdkDir.toFile().mkdirs()
        val fakeCacheDir = fakeFlutterSdkDir.resolve("bin").resolve("cache")
        fakeCacheDir.toFile().mkdirs()
        fakeCacheDir.resolve("engine.stamp").writeText("12345")
        fakeCacheDir.resolve("engine.realm").writeText("")
        
        every { project.projectDir } returns projectDir.toFile()
        every { project.findProperty("flutter.sdk") } returns fakeFlutterSdkDir.toString()
        every { project.file(fakeFlutterSdkDir.toString()) } returns fakeFlutterSdkDir.toFile()
        every { project.file("local.properties") } returns projectDir.parent.resolve("local.properties").toFile()
        
        val flutterExtension = FlutterExtension()
        every { project.extensions.create("flutter", any<Class<*>>()) } returns flutterExtension
        every { project.extensions.findByType(FlutterExtension::class.java) } returns flutterExtension

        every { project.state.failure as Throwable? } returns null
        
        // Explicitly set hostAppProjectName property
        every { project.rootProject } returns project
        every { project.hasProperty("flutter.hostAppProjectName") } returns true
        every { project.property("flutter.hostAppProjectName") } returns "custom-app"
        every { project.findProject(":custom-app") } returns null
        
        val flutterPlugin = FlutterPlugin()
        
        val exception = org.junit.jupiter.api.assertThrows<org.gradle.api.GradleException> {
            flutterPlugin.apply(project)
        }
        assertContains(exception.message!!, "Project :custom-app doesn't exist")
    }

    @Test
    fun `FlutterPlugin apply succeeds on standalone library project even when split-per-abi is enabled`(@TempDir tempDir: Path) {
        val projectDir = tempDir.resolve("project-dir").resolve("android").resolve("plugin")
        projectDir.toFile().mkdirs()
        val settingsFile = projectDir.parent.resolve("settings.gradle")
        settingsFile.writeText("empty for now")
        val fakeFlutterSdkDir = tempDir.resolve("fake-flutter-sdk")
        fakeFlutterSdkDir.toFile().mkdirs()
        val fakeCacheDir = fakeFlutterSdkDir.resolve("bin").resolve("cache")
        fakeCacheDir.toFile().mkdirs()
        fakeCacheDir.resolve("engine.stamp").writeText("12345")
        fakeCacheDir.resolve("engine.realm").writeText("")

        val project = mockk<Project>(relaxed = true)
        val mockLibraryExtension = mockk<LegacyLibraryExtension>(
            moreInterfaces = arrayOf(LibraryExtension::class),
            relaxed = true
        )
        
        every { project.extensions.findByType(com.android.build.api.dsl.ApplicationExtension::class.java) } returns null
        every { project.extensions.findByType(com.android.build.gradle.AbstractAppExtension::class.java) } returns null
        every { project.extensions.findByType(LegacyLibraryExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.getByType(LegacyLibraryExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.findByName("android") } returns mockLibraryExtension

        val mockBuildTypes = mockk<NamedDomainObjectContainer<BuildType>>(relaxed = true)
        val mockDebugBuildType = mockk<BuildType>(relaxed = true)
        val mockReleaseBuildType = mockk<BuildType>(relaxed = true)
        every { mockBuildTypes.getByName("debug") } returns mockDebugBuildType
        every { mockBuildTypes.getByName("release") } returns mockReleaseBuildType
        every { mockLibraryExtension.buildTypes } returns mockBuildTypes
        every { project.extensions.findByType(BaseExtension::class.java) } returns mockLibraryExtension
        every { project.extensions.getByType(BaseExtension::class.java) } returns mockLibraryExtension

        val mockAndroidComponentsExtension = mockk<AndroidComponentsExtension<*, *, *>>(relaxed = true)
        every { project.extensions.getByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        every { project.extensions.findByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        val mockSelector = mockk<com.android.build.api.variant.VariantSelector>(relaxed = true)
        every { mockAndroidComponentsExtension.selector() } returns mockSelector
        every { mockSelector.all() } returns mockSelector
        every { mockSelector.withName(any<String>()) } returns mockSelector

        every { project.projectDir } returns projectDir.toFile()
        every { project.findProperty("flutter.sdk") } returns fakeFlutterSdkDir.toString()
        every { project.file(fakeFlutterSdkDir.toString()) } returns fakeFlutterSdkDir.toFile()
        every { project.file("local.properties") } returns projectDir.parent.resolve("local.properties").toFile()

        val flutterExtension = FlutterExtension()
        every { project.extensions.create("flutter", any<Class<*>>()) } returns flutterExtension
        every { project.extensions.findByType(FlutterExtension::class.java) } returns flutterExtension

        every { project.rootProject } returns project
        every { project.state.failure as Throwable? } returns null
        every { project.findProject(":app") } returns null

        // Enable split-per-abi
        every { project.hasProperty("split-per-abi") } returns true
        every { project.property("split-per-abi") } returns "true"
        every { project.findProperty("split-per-abi") } returns "true"

        mockkObject(NativePluginLoaderReflectionBridge)
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns listOf()
        every { project.extraProperties } returns mockk()

        val taskContainer = mockk<TaskContainer>(relaxed = true)
        every { project.tasks } returns taskContainer
        val mockTaskProvider = mockk<TaskProvider<Task>>(relaxed = true)
        val mockTask = mockk<Task>(relaxed = true)
        every { mockTaskProvider.get() } returns mockTask
        every { taskContainer.named(any<String>()) } returns mockTaskProvider

        val mockVariant = mockk<LibraryVariant>(relaxed = true)
        every { mockVariant.name } returns "debug"
        every { mockVariant.buildType.name } returns "debug"
        every { mockVariant.flavorName } returns ""
        val mergedFlavor = mockk<InternalBaseVariant.MergedFlavor>(relaxed = true)
        every { mockVariant.mergedFlavor } returns mergedFlavor
        val apiLevel = mockk<com.android.builder.model.ApiVersion>(relaxed = true)
        every { apiLevel.apiLevel } returns 21
        every { mergedFlavor.minSdkVersion } returns apiLevel
        
        // Mock outputs of the LibraryVariant. They are BaseVariantOutput, not ApkVariantOutput.
        val variantOutput = mockk<com.android.build.gradle.api.BaseVariantOutput>(relaxed = true)
        val outputsIterator = mockk<MutableIterator<com.android.build.gradle.api.BaseVariantOutput>>()
        every { outputsIterator.hasNext() } returns true andThen false
        every { outputsIterator.next() } returns variantOutput
        val variantOutputCollection = mockk<org.gradle.api.DomainObjectCollection<com.android.build.gradle.api.BaseVariantOutput>>()
        every { variantOutputCollection.iterator() } returns outputsIterator
        every { mockVariant.outputs } returns variantOutputCollection
        
        val processResourcesProvider = mockk<TaskProvider<ProcessAndroidResources>>(relaxed = true)
        every { processResourcesProvider.hint(ProcessAndroidResources::class).get() } returns mockk<ProcessAndroidResources>(relaxed = true)
        every { variantOutput.processResourcesProvider } returns processResourcesProvider
        
        val assembleTask = mockk<Task>(relaxed = true)
        val assembleTaskProvider = mockk<TaskProvider<Task>>(relaxed = true)
        every { assembleTaskProvider.get() } returns assembleTask
        every { mockVariant.assembleProvider } returns assembleTaskProvider
        
        val variants = listOf(mockVariant)
        val variantsIterator = mockk<MutableIterator<LibraryVariant>>()
        every { variantsIterator.hasNext() } returns true andThen false
        every { variantsIterator.next() } returns mockVariant
        val variantCollection = mockk<DefaultDomainObjectSet<LibraryVariant>>()
        every { mockLibraryExtension.libraryVariants } returns variantCollection
        every { variantCollection.iterator() } returns variantsIterator
        every {
            variantCollection.configureEach(any<Action<LibraryVariant>>())
        } answers {
            variants.forEach { firstArg<Action<LibraryVariant>>().execute(it) }
        }
        every {
            variantCollection.all(any<Action<LibraryVariant>>())
        } answers {
            variants.forEach { firstArg<Action<LibraryVariant>>().execute(it) }
        }

        every { mockVariant.mergeAssetsProvider.hint(MergeSourceSetFolders::class).get() } returns
            mockk<MergeSourceSetFolders>(relaxed = true)
        
        val flutterTask = mockk<FlutterTask>(relaxed = true)
        val copySpec = mockk<org.gradle.api.file.CopySpec>(relaxed = true)
        every { flutterTask.assets } returns copySpec
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
        every { mockCopyTaskProvider.hint(Copy::class).get() } returns mockk<Copy>(relaxed = true)
        every {
            taskContainer.register(
                match { it.startsWith("copyFlutterAssets") },
                eq(Copy::class.java),
                any()
            )
        } answers {
            mockCopyTaskProvider
        }

        val flutterPlugin = FlutterPlugin()

        // This should succeed without ClassCastException even with split-per-abi enabled.
        flutterPlugin.apply(project)

        // Verify tasks were registered
        verify {
            taskContainer.register(
                match { it.contains("compileFlutterBuildDebug") },
                any<Class<FlutterTask>>(),
                any()
            )
        }
    }
}
