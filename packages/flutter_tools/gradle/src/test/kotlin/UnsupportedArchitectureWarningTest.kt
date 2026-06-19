package com.flutter.gradle

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.gradle.api.Project
import org.gradle.api.logging.Logger
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.io.TempDir
import java.io.File
import java.io.FileOutputStream
import java.nio.file.Path
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class UnsupportedArchitectureWarningTest {
    private fun createMockZip(
        file: File,
        entries: List<String>
    ) {
        ZipOutputStream(FileOutputStream(file)).use { zos ->
            for (entryName in entries) {
                zos.putNextEntry(ZipEntry(entryName))
                zos.write("fake content".toByteArray())
                zos.closeEntry()
            }
        }
    }

    @Test
    fun `checkUnsupportedAbis logs warning when APK contains unsupported x86 architecture`(
        @TempDir tempDir: Path
    ) {
        val mockProject = mockk<Project>(relaxed = true)
        val mockLogger = mockk<Logger>(relaxed = true)
        every { mockProject.logger } returns mockLogger

        val apkFile = tempDir.resolve("app-release.apk").toFile()
        // APK structure: native libs are under lib/<ABI>/.
        // We include a supported one (arm64-v8a) and an unsupported one (x86).
        createMockZip(
            apkFile,
            listOf(
                "lib/arm64-v8a/libflutter.so",
                "lib/arm64-v8a/libapp.so",
                "lib/x86/libsome_native_lib.so"
            )
        )

        FlutterPluginUtils.checkUnsupportedAbis(mockProject, apkFile, isBundle = false)

        // Verify that a warning was logged mentioning the unsupported architecture 'x86'
        verify(exactly = 1) {
            mockLogger.warn(
                match { message ->
                    message.contains("APK contains native libraries for the following unsupported architectures") &&
                        message.contains("x86") &&
                        !message.substringAfter("unsupported architectures:").substringBefore(".").contains("arm64-v8a")
                }
            )
        }
    }

    @Test
    fun `checkUnsupportedAbis logs warning when AAB contains unsupported x86 architecture`(
        @TempDir tempDir: Path
    ) {
        val mockProject = mockk<Project>(relaxed = true)
        val mockLogger = mockk<Logger>(relaxed = true)
        every { mockProject.logger } returns mockLogger

        val aabFile = tempDir.resolve("app-release.aab").toFile()
        // AAB structure: native libs are under base/lib/<ABI>/ or feature/lib/<ABI>/.
        createMockZip(
            aabFile,
            listOf(
                "base/lib/arm64-v8a/libflutter.so",
                "base/lib/arm64-v8a/libapp.so",
                "base/lib/x86/libsome_native_lib.so"
            )
        )

        FlutterPluginUtils.checkUnsupportedAbis(mockProject, aabFile, isBundle = true)

        // Verify that a warning was logged mentioning the unsupported architecture 'x86'
        verify(exactly = 1) {
            mockLogger.warn(
                match { message ->
                    message.contains("AAB contains native libraries for the following unsupported architectures") &&
                        message.contains("x86")
                }
            )
        }
    }

    @Test
    fun `checkUnsupportedAbis does not log warning when APK only contains supported architectures`(
        @TempDir tempDir: Path
    ) {
        val mockProject = mockk<Project>(relaxed = true)
        val mockLogger = mockk<Logger>(relaxed = true)
        every { mockProject.logger } returns mockLogger

        val apkFile = tempDir.resolve("app-release.apk").toFile()
        createMockZip(
            apkFile,
            listOf(
                "lib/armeabi-v7a/libflutter.so",
                "lib/arm64-v8a/libflutter.so",
                "lib/x86_64/libflutter.so"
            )
        )

        FlutterPluginUtils.checkUnsupportedAbis(mockProject, apkFile, isBundle = false)

        // Verify that NO warnings were logged
        verify(exactly = 0) {
            mockLogger.warn(any<String>())
        }
    }

    @Test
    fun `checkUnsupportedAbis logs warning with all unsupported architectures when multiple are present`(
        @TempDir tempDir: Path
    ) {
        val mockProject = mockk<Project>(relaxed = true)
        val mockLogger = mockk<Logger>(relaxed = true)
        every { mockProject.logger } returns mockLogger

        val apkFile = tempDir.resolve("app-release.apk").toFile()
        createMockZip(
            apkFile,
            listOf(
                "lib/arm64-v8a/libflutter.so",
                "lib/x86/libsome_native_lib.so",
                "lib/mips/libother_native_lib.so"
            )
        )

        FlutterPluginUtils.checkUnsupportedAbis(mockProject, apkFile, isBundle = false)

        verify(exactly = 1) {
            mockLogger.warn(
                match { message ->
                    message.contains("APK contains native libraries for the following unsupported architectures") &&
                        message.contains("x86") &&
                        message.contains("mips") &&
                        !message.substringAfter("unsupported architectures:").substringBefore(".").contains("arm64-v8a")
                }
            )
        }
    }

    @Test
    fun `checkUnsupportedAbis does not crash when the file is invalid or corrupt`(
        @TempDir tempDir: Path
    ) {
        val mockProject = mockk<Project>(relaxed = true)
        val mockLogger = mockk<Logger>(relaxed = true)
        every { mockProject.logger } returns mockLogger

        val corruptFile = tempDir.resolve("corrupt-release.apk").toFile()
        corruptFile.writeText("This is not a zip file at all!")

        // This call should not throw any exceptions
        FlutterPluginUtils.checkUnsupportedAbis(mockProject, corruptFile, isBundle = false)

        // It shouldn't log any warning about unsupported architectures, and it shouldn't crash
        verify(exactly = 0) {
            mockLogger.warn(any())
        }
    }

    @Test
    fun `checkUnsupportedAbis ignores directory entries and non-so files`(
        @TempDir tempDir: Path
    ) {
        val mockProject = mockk<Project>(relaxed = true)
        val mockLogger = mockk<Logger>(relaxed = true)
        every { mockProject.logger } returns mockLogger

        val apkFile = tempDir.resolve("app-release.apk").toFile()
        createMockZip(
            apkFile,
            listOf(
                "lib/x86/", // Directory entry
                "lib/x86/README.txt", // Non-so file
                "lib/arm64-v8a/libflutter.so"
            )
        )

        FlutterPluginUtils.checkUnsupportedAbis(mockProject, apkFile, isBundle = false)

        // Since no actual .so files are in the unsupported x86 directory, it should not log a warning.
        verify(exactly = 0) {
            mockLogger.warn(any())
        }
    }
}
