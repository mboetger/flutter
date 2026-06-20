// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.tasks

import org.gradle.api.GradleException
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.io.TempDir
import java.nio.file.Path
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class CheckUnsupportedAbisTaskTest {
    private val supportedAbis = listOf("armeabi-v7a", "arm64-v8a", "x86_64")

    @Test
    fun testPerformAbiCheck_noFiles_passes() {
        CheckUnsupportedAbisTask.performAbiCheck(
            mergedNativeLibsFiles = emptySet(),
            supportedAbis = supportedAbis,
            buildMode = "release",
            abiFilters = emptySet()
        )
    }

    @Test
    fun testPerformAbiCheck_onlySupportedAbis_passes(@TempDir tempDir: Path) {
        val rootDir = tempDir.resolve("out").toFile().apply { mkdirs() }
        val libDir = rootDir.resolve("lib").apply { mkdir() }
        
        val arm64Dir = libDir.resolve("arm64-v8a").apply { mkdir() }
        arm64Dir.resolve("libfoo.so").createNewFile()

        val x64Dir = libDir.resolve("x86_64").apply { mkdir() }
        x64Dir.resolve("libbar.so").createNewFile()

        CheckUnsupportedAbisTask.performAbiCheck(
            mergedNativeLibsFiles = setOf(rootDir),
            supportedAbis = supportedAbis,
            buildMode = "release",
            abiFilters = emptySet()
        )
    }

    @Test
    fun testPerformAbiCheck_unsupportedAbis_throws(@TempDir tempDir: Path) {
        val rootDir = tempDir.resolve("out").toFile().apply { mkdirs() }
        val libDir = rootDir.resolve("lib").apply { mkdir() }

        // Supported ABI
        val arm64Dir = libDir.resolve("arm64-v8a").apply { mkdir() }
        arm64Dir.resolve("libfoo.so").createNewFile()

        // Unsupported ABI (x86)
        val x86Dir = libDir.resolve("x86").apply { mkdir() }
        x86Dir.resolve("libbad.so").createNewFile()

        // Unsupported ABI (armeabi)
        val armeabiDir = libDir.resolve("armeabi").apply { mkdir() }
        armeabiDir.resolve("libolder.so").createNewFile()

        val exception = assertFailsWith<GradleException> {
            CheckUnsupportedAbisTask.performAbiCheck(
                mergedNativeLibsFiles = setOf(rootDir),
                supportedAbis = supportedAbis,
                buildMode = "release",
                abiFilters = emptySet()
            )
        }
        val message = exception.message ?: ""
        assertTrue(message.contains("unsupported ABIs"), "Expected message to contain 'unsupported ABIs', but was: '$message'")
        assertTrue(message.contains("x86"), "Expected message to contain 'x86', but was: '$message'")
        assertTrue(message.contains("armeabi"), "Expected message to contain 'armeabi', but was: '$message'")
        val firstLine = message.substringBefore("\n")
        assertTrue(!firstLine.contains("arm64-v8a"), "Expected first line of message NOT to contain 'arm64-v8a', but was: '$firstLine'")
    }

    @Test
    fun testPerformAbiCheck_nonSoFiles_ignored(@TempDir tempDir: Path) {
        val rootDir = tempDir.resolve("out").toFile().apply { mkdirs() }
        val libDir = rootDir.resolve("lib").apply { mkdir() }
        val x86Dir = libDir.resolve("x86").apply { mkdir() }
        
        // Non-.so file in an unsupported ABI directory should be ignored
        x86Dir.resolve("readme.txt").createNewFile()

        CheckUnsupportedAbisTask.performAbiCheck(
            mergedNativeLibsFiles = setOf(rootDir),
            supportedAbis = supportedAbis,
            buildMode = "release",
            abiFilters = emptySet()
        )
    }

    @Test
    fun testPerformAbiCheck_unsupportedAbis_withSupportedFilters_passes(@TempDir tempDir: Path) {
        val rootDir = tempDir.resolve("out").toFile().apply { mkdirs() }
        val libDir = rootDir.resolve("lib").apply { mkdir() }

        // Unsupported ABI (x86) is present in merged native libs
        val x86Dir = libDir.resolve("x86").apply { mkdir() }
        x86Dir.resolve("libbad.so").createNewFile()

        // But active filters only include supported ABI (arm64-v8a)
        CheckUnsupportedAbisTask.performAbiCheck(
            mergedNativeLibsFiles = setOf(rootDir),
            supportedAbis = supportedAbis,
            buildMode = "release",
            abiFilters = setOf("arm64-v8a")
        )
    }

    @Test
    fun testPerformAbiCheck_unsupportedFilters_throws() {
        // If active filters contain an unsupported ABI (x86), the build should throw
        // even if the merged native libs directory is empty.
        val exception = assertFailsWith<GradleException> {
            CheckUnsupportedAbisTask.performAbiCheck(
                mergedNativeLibsFiles = emptySet(),
                supportedAbis = supportedAbis,
                buildMode = "release",
                abiFilters = setOf("arm64-v8a", "x86")
            )
        }
        val message = exception.message ?: ""
        assertTrue(message.contains("unsupported ABIs"), "Expected message to contain 'unsupported ABIs', but was: '$message'")
        assertTrue(message.contains("x86"), "Expected message to contain 'x86', but was: '$message'")
        val firstLine = message.substringBefore("\n")
        assertTrue(!firstLine.contains("arm64-v8a"), "Expected first line of message NOT to contain 'arm64-v8a', but was: '$firstLine'")
    }
}
