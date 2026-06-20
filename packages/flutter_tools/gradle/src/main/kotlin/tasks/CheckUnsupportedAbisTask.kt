// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.tasks

import com.flutter.gradle.FlutterPluginConstants.PLATFORM_ABI_LIST
import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.file.ConfigurableFileCollection
import org.gradle.api.provider.Property
import org.gradle.api.provider.SetProperty
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.InputFiles
import org.gradle.api.tasks.PathSensitive
import org.gradle.api.tasks.PathSensitivity
import org.gradle.api.tasks.TaskAction
import java.io.File

abstract class CheckUnsupportedAbisTask : DefaultTask() {
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val mergedNativeLibsFiles: ConfigurableFileCollection

    @get:Input
    abstract val buildMode: Property<String>

    @get:Input
    abstract val abiFilters: SetProperty<String>

    @TaskAction
    fun run() {
        performAbiCheck(
            mergedNativeLibsFiles = mergedNativeLibsFiles.files,
            supportedAbis = PLATFORM_ABI_LIST,
            buildMode = buildMode.get(),
            abiFilters = abiFilters.get()
        )
    }

    companion object {
        internal fun performAbiCheck(
            mergedNativeLibsFiles: Set<File>,
            supportedAbis: List<String>,
            buildMode: String,
            abiFilters: Set<String>
        ) {
            if (abiFilters.isNotEmpty()) {
                val unsupportedInFilters = abiFilters.filter { it !in supportedAbis }
                if (unsupportedInFilters.isNotEmpty()) {
                    throwUnsupportedAbisException(unsupportedInFilters.toSet(), supportedAbis)
                }
                return
            }

            val unsupportedAbis = mutableSetOf<String>()
            
            mergedNativeLibsFiles.forEach { file ->
                if (file.isDirectory) {
                    file.walkTopDown().forEach { child ->
                        if (child.isFile && child.extension == "so") {
                            checkFile(child, supportedAbis, unsupportedAbis)
                        }
                    }
                } else if (file.isFile && file.extension == "so") {
                    checkFile(file, supportedAbis, unsupportedAbis)
                }
            }

            if (unsupportedAbis.isNotEmpty()) {
                throwUnsupportedAbisException(unsupportedAbis, supportedAbis)
            }
        }

        private fun checkFile(file: File, supportedAbis: List<String>, unsupportedAbis: MutableSet<String>) {
            val parentDir = file.parentFile
            val parentParentDir = parentDir?.parentFile
            if (parentParentDir?.name == "lib") {
                val abi = parentDir.name
                if (abi !in supportedAbis) {
                    unsupportedAbis.add(abi)
                }
            }
        }

        private fun throwUnsupportedAbisException(unsupportedAbis: Set<String>, supportedAbis: List<String>) {
            throw GradleException(
                "Build failed due to unsupported ABIs: ${unsupportedAbis.joinToString(", ")}.\n" +
                "Flutter does not support these ABIs in release or profile builds. " +
                "Packaging them without Flutter's native libraries (libflutter.so and libapp.so) " +
                "will cause the app to crash on devices of these ABIs.\n\n" +
                "To resolve this issue:\n" +
                "  1. Re-enable Flutter's ABI filtering (do not pass '-Pdisable-abi-filtering=true' " +
                "unless you have a custom, non-standard setup).\n" +
                "  2. Configure 'abiFilters' explicitly in your app's 'build.gradle' file " +
                "within the 'defaultConfig' or 'ndk' block to only include supported ABIs: " +
                "${supportedAbis.joinToString(", ")}.\n" +
                "  3. Remove or update dependencies that introduce these unsupported native libraries."
            )
        }
    }
}
