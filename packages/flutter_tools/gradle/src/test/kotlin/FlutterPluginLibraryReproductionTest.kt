package com.flutter.gradle

import com.android.build.api.dsl.ApplicationExtension
import com.android.build.gradle.LibraryExtension
import com.android.build.api.variant.AndroidComponentsExtension
import com.android.build.gradle.AbstractAppExtension
import com.android.build.gradle.BaseExtension
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.unmockkAll
import com.flutter.gradle.tasks.FlutterTask
import org.gradle.api.Action
import org.gradle.api.Project
import org.gradle.api.Task
import org.gradle.api.tasks.Copy
import org.gradle.api.tasks.Sync
import org.gradle.api.tasks.TaskContainer
import org.gradle.api.tasks.TaskProvider
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.nio.file.Path
import kotlin.io.path.writeText
import kotlin.test.Test

class FlutterPluginLibraryReproductionTest {

    @AfterEach
    fun tearDown() {
        // Prevent mocked Kotlin objects (like NativePluginLoaderReflectionBridge)
        // from leaking to other tests in the same JVM process.
        unmockkAll()
    }

    @Test
    fun `applying FlutterPlugin to a module with an Android Library host project succeeds`(
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
        fakeEngineStampFile.writeText("901b0f1afe77c3555abee7b86a26aaa37f131379")
        val fakeEngineRealmFile = fakeCacheDir.resolve("engine.realm")
        fakeEngineRealmFile.writeText("made_up_realm")

        // 1. Mock the project that the plugin is applied to (the Flutter module project)
        val project = mockk<Project>(relaxed = true)
        every { project.projectDir } returns projectDir.toFile()
        every { project.findProperty("flutter.sdk") } returns fakeFlutterSdkDir.toString()
        every { project.state.failure } returns null

        val mockCopySpec = mockk<org.gradle.api.file.CopySpec>(relaxed = true)
        val mockFlutterTask = mockk<FlutterTask>(relaxed = true)
        every { mockFlutterTask.assets } returns mockCopySpec
        val mockFlutterTaskProvider = mockk<TaskProvider<FlutterTask>>(relaxed = true)
        every { mockFlutterTaskProvider.hint(FlutterTask::class).get() } returns mockFlutterTask
        every {
            project.tasks.register(any<String>(), eq(FlutterTask::class.java), any())
        } answers {
            val action = thirdArg<Action<FlutterTask>>()
            action.execute(mockFlutterTask)
            mockFlutterTaskProvider
        }

        val mockSyncTask = mockk<Sync>(relaxed = true)
        val mockSyncTaskProvider = mockk<TaskProvider<Sync>>(relaxed = true)
        every { mockSyncTaskProvider.hint(Sync::class).get() } returns mockSyncTask
        every {
            project.tasks.register(any<String>(), eq(Sync::class.java), any())
        } answers {
            val action = thirdArg<Action<Sync>>()
            action.execute(mockSyncTask)
            mockSyncTaskProvider
        }

        val mockCopyTask = mockk<Copy>(relaxed = true)
        val mockCopyTaskProvider = mockk<TaskProvider<Copy>>(relaxed = true)
        every { mockCopyTaskProvider.hint(Copy::class).get() } returns mockCopyTask
        every {
            project.tasks.register(any<String>(), eq(Copy::class.java), any())
        } answers {
            val action = thirdArg<Action<Copy>>()
            action.execute(mockCopyTask)
            mockCopyTaskProvider
        }
        every { project.file(any<Any>()) } answers {
            val path = firstArg<Any>()
            val result = if (path is File) path else File(path.toString())
            println("=== project.file called with: $path (type: ${path.javaClass.name}), returning: $result (type: ${result.javaClass.name})")
            result
        }

        val flutterExtension = FlutterExtension()
        every { project.extensions.create("flutter", any<Class<*>>()) } returns flutterExtension
        every { project.extensions.findByType(FlutterExtension::class.java) } returns flutterExtension

        // Since it's a module, it is NOT a Flutter App project (ApplicationExtension is null)
        every { project.extensions.findByType(ApplicationExtension::class.java) } returns null

        // It is an Android Library project, so it has a LibraryExtension
        val mockModuleLibraryExtension = mockk<LibraryExtension>(relaxed = true)
        every { project.extensions.findByType(LibraryExtension::class.java) } returns mockModuleLibraryExtension
        every { project.extensions.getByType(LibraryExtension::class.java) } returns mockModuleLibraryExtension
        every { project.extensions.findByName("android") } returns mockModuleLibraryExtension

        val mockDebugBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>(relaxed = true)
        val mockReleaseBuildType = mockk<com.android.build.gradle.internal.dsl.BuildType>(relaxed = true)
        every { mockModuleLibraryExtension.buildTypes.getByName("debug") } returns mockDebugBuildType
        every { mockModuleLibraryExtension.buildTypes.getByName("release") } returns mockReleaseBuildType

        // Retrieve the relaxed library variants collection mock directly and stub its .all {} call
        val libraryVariants = mockModuleLibraryExtension.libraryVariants
        val mockLibraryVariant = mockk<com.android.build.gradle.api.LibraryVariant>(relaxed = true)
        val mockMergeAssetsTask = mockk<com.android.build.gradle.tasks.MergeSourceSetFolders>(relaxed = true)
        val mockMergeAssetsTaskProvider = mockk<TaskProvider<com.android.build.gradle.tasks.MergeSourceSetFolders>>(relaxed = true)
        every { mockMergeAssetsTaskProvider.hint(com.android.build.gradle.tasks.MergeSourceSetFolders::class).get() } returns mockMergeAssetsTask
        every { mockLibraryVariant.mergeAssetsProvider } returns mockMergeAssetsTaskProvider

        val variantOutput = mockk<com.android.build.gradle.api.BaseVariantOutput>(relaxed = true)
        val processResourcesProvider = mockk<TaskProvider<com.android.build.gradle.tasks.ProcessAndroidResources>>(relaxed = true)
        val mockProcessAndroidResources = mockk<com.android.build.gradle.tasks.ProcessAndroidResources>(relaxed = true)
        every { processResourcesProvider.hint(com.android.build.gradle.tasks.ProcessAndroidResources::class).get() } returns mockProcessAndroidResources
        every { variantOutput.processResourcesProvider } returns processResourcesProvider

        val outputsIterator = mockk<MutableIterator<com.android.build.gradle.api.BaseVariantOutput>>()
        every { outputsIterator.hasNext() } returns true andThen false
        every { outputsIterator.next() } returns variantOutput
        val variantOutputCollection = mockk<org.gradle.api.DomainObjectCollection<com.android.build.gradle.api.BaseVariantOutput>>()
        every { variantOutputCollection.iterator() } returns outputsIterator
        every { mockLibraryVariant.outputs } returns variantOutputCollection

        every { libraryVariants.all(any<Action<com.android.build.gradle.api.LibraryVariant>>()) } answers {
            firstArg<Action<com.android.build.gradle.api.LibraryVariant>>().execute(mockLibraryVariant)
        }

        // Mock additional extensions needed during initialization
        val mockBaseExtension = mockk<BaseExtension>(relaxed = true)
        every { project.extensions.findByType(BaseExtension::class.java) } returns mockBaseExtension
        val mockAndroidComponentsExtension = mockk<AndroidComponentsExtension<*, *, *>>(relaxed = true)
        every { project.extensions.getByType(AndroidComponentsExtension::class.java) } returns mockAndroidComponentsExtension
        every { mockAndroidComponentsExtension.selector() } returns mockk {
            every { all() } returns mockk()
        }

        // mock return of NativePluginLoaderReflectionBridge.getPlugins
        mockkObject(NativePluginLoaderReflectionBridge)
        every { NativePluginLoaderReflectionBridge.getPlugins(any(), any()) } returns listOf()
        every { project.extraProperties } returns mockk()

        // 2. Mock the host app project ("appProject")
        val appProject = mockk<Project>(relaxed = true)
        every { project.rootProject } returns project
        every { project.findProject(":app") } returns appProject

        // Mock afterEvaluate on the host app project to execute the action immediately
        every { appProject.afterEvaluate(any<Action<in Project>>()) } answers {
            println("=== appProject.afterEvaluate STUB TRIGGERED!")
            firstArg<Action<in Project>>().execute(appProject)
        }

        // Crucial part: mock the "android" extension on the host app project to be a LibraryExtension,
        // which represents an Android Library project instead of an Android Application project.
        val mockHostLibraryExtension = mockk<LibraryExtension>(relaxed = true)
        every { appProject.extensions.findByName("android") } returns mockHostLibraryExtension

        // Mock library variants on the host's library extension as well to be ready for the fixed path
        val hostLibraryVariants = mockHostLibraryExtension.libraryVariants
        val mockHostLibraryVariant = mockk<com.android.build.gradle.api.LibraryVariant>(relaxed = true)
        val mockAssembleTask = mockk<org.gradle.api.Task>(relaxed = true)
        val mockAssembleTaskProvider = mockk<org.gradle.api.tasks.TaskProvider<org.gradle.api.Task>>(relaxed = true)
        every { mockAssembleTaskProvider.get() } returns mockAssembleTask
        every { mockHostLibraryVariant.assembleProvider } returns mockAssembleTaskProvider
        every { hostLibraryVariants.all(any<Action<com.android.build.gradle.api.LibraryVariant>>()) } answers {
            firstArg<Action<com.android.build.gradle.api.LibraryVariant>>().execute(mockHostLibraryVariant)
        }
        val mockTaskProvider = mockk<TaskProvider<Task>>(relaxed = true)
        every { mockTaskProvider.hint(Task::class).get() } returns mockk<Task>(relaxed = true)
        every {
            project.tasks.named(any<String>())
        } returns mockTaskProvider

        val flutterPlugin = FlutterPlugin()

        // 3. Apply the plugin. This should succeed under a correct implementation,
        // but will throw an uncaught exception on the current buggy implementation,
        // thus failing the test and confirming the bug.
        flutterPlugin.apply(project)
    }
}
