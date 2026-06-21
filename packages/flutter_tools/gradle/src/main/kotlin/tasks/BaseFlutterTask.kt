// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.tasks

import org.gradle.api.DefaultTask
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.file.RegularFileProperty
import org.gradle.api.provider.ListProperty
import org.gradle.api.provider.Property
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.Internal
import org.gradle.api.tasks.Optional
import org.gradle.api.tasks.OutputFiles
import java.io.File

// IMPORTANT: Do not add logic to the methods in this class directly,
// instead add logic to [BaseFlutterTaskHelper].

/**
 * Base implementation of a Gradle task. Gradle tasks can not be instantiated for testing,
 * so this class delegates all logic to [BaseFlutterTaskHelper].
 */
abstract class BaseFlutterTask : DefaultTask() {
    @get:Internal
    abstract val flutterRoot: DirectoryProperty

    @get:Internal
    abstract val flutterExecutable: RegularFileProperty

    @get:Input
    abstract val buildMode: Property<String>

    @get:Input
    abstract val minSdkVersion: Property<Int>

    @get:Optional
    @get:Input
    abstract val localEngine: Property<String>

    @get:Optional
    @get:Input
    abstract val localEngineHost: Property<String>

    @get:Optional
    @get:Input
    abstract val localEngineSrcPath: Property<String>

    @get:Input
    abstract val targetPath: Property<String>

    @get:Optional
    @get:Input
    abstract val verbose: Property<Boolean>

    @get:Optional
    @get:Input
    abstract val fileSystemRoots: ListProperty<String>

    @get:Optional
    @get:Input
    abstract val fileSystemScheme: Property<String>

    @get:Input
    abstract val trackWidgetCreation: Property<Boolean>

    @get:Optional
    @get:Input
    abstract val targetPlatformValues: ListProperty<String>

    @get:Internal
    abstract val sourceDir: DirectoryProperty

    @get:Internal
    abstract val intermediateDir: DirectoryProperty

    @get:Optional
    @get:Input
    abstract val frontendServerStarterPath: Property<String>

    @get:Optional
    @get:Input
    abstract val extraFrontEndOptions: Property<String>

    @get:Optional
    @get:Input
    abstract val extraGenSnapshotOptions: Property<String>

    @get:Optional
    @get:Input
    abstract val splitDebugInfo: Property<String>

    @get:Optional
    @get:Input
    abstract val treeShakeIcons: Property<Boolean>

    @get:Optional
    @get:Input
    abstract val dartObfuscation: Property<Boolean>

    @get:Optional
    @get:Input
    abstract val dartDefines: Property<String>

    @get:Optional
    @get:Input
    abstract val bundleSkSLPath: Property<String>

    @get:Optional
    @get:Input
    abstract val codeSizeDirectory: Property<String>

    @get:Optional
    @get:Input
    abstract val performanceMeasurementFile: Property<String>

    @get:Optional
    @get:Input
    abstract val deferredComponents: Property<Boolean>

    @get:Optional
    @get:Input
    abstract val validateDeferredComponents: Property<Boolean>

    @get:Optional
    @get:Input
    abstract val skipDependencyChecks: Property<Boolean>

    @get:Optional
    @get:Input
    abstract val flavor: Property<String>

    /**
     * Gets the dependency file(s) by calling [com.flutter.gradle.tasks.BaseFlutterTaskHelper.getDependenciesFiles].
     *
     * @return the dependency file(s) based on the current intermediate directory path.
     */
    @OutputFiles
    fun getDependenciesFiles() = BaseFlutterTaskHelper.getDependenciesFiles(baseFlutterTask = this)

    /**
     * Builds a Flutter Android application bundle by verifying the Flutter source directory,
     * creating an intermediate build directory if necessary, and running flutter assemble by
     * configuring and executing with a set of build configurations.
     */
    fun buildBundle() = BaseFlutterTaskHelper.buildBundle(baseFlutterTask = this)
}
