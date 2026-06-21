// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.tasks

import androidx.annotation.VisibleForTesting
import org.gradle.api.Action
import org.gradle.api.GradleException
import org.gradle.api.file.FileCollection
import org.gradle.api.logging.LogLevel
import org.gradle.api.tasks.OutputFiles
import org.gradle.kotlin.dsl.support.serviceOf
import org.gradle.process.ExecOperations
import org.gradle.process.ExecSpec
import java.io.File

/**
 * Stateless object to contain the logic used in [BaseFlutterTask]. Any required state should be stored
 * on [BaseFlutterTask] instead, while any logic needed by [BaseFlutterTask] should be added here.
 */
object BaseFlutterTaskHelper {
    @VisibleForTesting
    internal fun getGradleErrorMessage(baseFlutterTask: BaseFlutterTask): String =
        "Invalid Flutter source directory: ${baseFlutterTask.sourceDir.orNull?.asFile}"

    /**
     * Gets the dependency file(s) that tracks the dependencies or input files used for a specific
     * Flutter build step based on the current intermediate directory.
     *
     * @return the dependency file(s) based on the current intermediate directory.
     */
    @OutputFiles
    @VisibleForTesting
    internal fun getDependenciesFiles(baseFlutterTask: BaseFlutterTask): FileCollection {
        var depfiles: FileCollection = baseFlutterTask.project.files()

        // TODO(jesswon): During cleanup determine if .../flutter_build.d is ever a directory and refactor accordingly
        // Includes all sources used in the flutter compilation.
        depfiles += baseFlutterTask.project.files(baseFlutterTask.intermediateDir.file("flutter_build.d"))
        return depfiles
    }

    /**
     * Checks precondition to ensures sourceDir is not null and is a directory. Also checks
     * if intermediateDir is valid valid and creates it (and parent directories if needed) if invalid.
     *
     * @throws GradleException if sourceDir is null or is not a directory
     */
    @VisibleForTesting
    internal fun checkPreConditions(baseFlutterTask: BaseFlutterTask) {
        val sourceDir = baseFlutterTask.sourceDir.orNull?.asFile
        if (sourceDir == null || !sourceDir.isDirectory) {
            throw GradleException(getGradleErrorMessage(baseFlutterTask))
        }
        baseFlutterTask.intermediateDir.get().asFile.mkdirs()
    }

    /**
     * Computes the rule names for flutter assemble. To speed up builds that contain
     * multiple ABIs, the target name is used to communicate which ones are required
     * rather than the TargetPlatform. This allows multiple builds to share the same
     * cache.
     *
     * @param baseFlutterTask is a BaseFlutterTask to access its properties
     * @return the list of rule names for flutter assemble.
     */
    @VisibleForTesting
    internal fun generateRuleNames(baseFlutterTask: BaseFlutterTask): List<String> {
        val buildMode = baseFlutterTask.buildMode.orNull
        val deferredComponents = baseFlutterTask.deferredComponents.getOrElse(false)
        val targetPlatformValues = baseFlutterTask.targetPlatformValues.orNull ?: emptyList()
        val ruleNames: List<String> =
            when {
                buildMode == "debug" -> listOf("debug_android_application")
                deferredComponents ->
                    targetPlatformValues
                        .map {
                            "android_aot_deferred_components_bundle_${buildMode}_$it"
                        }

                else -> targetPlatformValues.map { "android_aot_bundle_${buildMode}_$it" }
            }
        return ruleNames
    }

    /**
     * Creates and configures the build processes of an Android Flutter application to be executed.
     * The configuration includes setting the executable to the Flutter command-line tool (Flutter CLI),
     * setting the working directory to the Flutter project's source directory, adding command-line arguments and build rules
     * to configure various build options.
     *
     * @return an Action<ExecSpec> of build processes and options to be executed.
     */
    internal fun createExecSpecActionFromTask(baseFlutterTask: BaseFlutterTask): Action<ExecSpec> =
        Action<ExecSpec> {
            executable(baseFlutterTask.flutterExecutable.get().asFile.absolutePath)
            workingDir(baseFlutterTask.sourceDir.orNull?.asFile)
            baseFlutterTask.localEngine.orNull?.let {
                args("--local-engine", it)
                args("--local-engine-src-path", baseFlutterTask.localEngineSrcPath.orNull)
            }
            baseFlutterTask.localEngineHost.orNull?.let {
                args("--local-engine-host", it)
            }
            if (baseFlutterTask.verbose.getOrElse(false)) {
                args("--verbose")
            } else {
                args("--quiet")
            }
            args("assemble")
            args("--no-version-check")
            args("--depfile", "${baseFlutterTask.intermediateDir.get().asFile}/flutter_build.d")
            args("--output", "${baseFlutterTask.intermediateDir.get().asFile}")
            baseFlutterTask.performanceMeasurementFile.orNull?.let {
                args("--performance-measurement-file=$it")
            }
            args("-dTargetFile=${baseFlutterTask.targetPath.orNull}")
            args("-dTargetPlatform=android")
            args("-dBuildMode=${baseFlutterTask.buildMode.orNull}")
            baseFlutterTask.trackWidgetCreation.orNull?.let {
                args("-dTrackWidgetCreation=$it")
            }
            baseFlutterTask.splitDebugInfo.orNull?.let {
                args("-dSplitDebugInfo=$it")
            }
            if (baseFlutterTask.treeShakeIcons.getOrElse(false)) {
                args("-dTreeShakeIcons=true")
            }
            if (baseFlutterTask.dartObfuscation.getOrElse(false)) {
                args("-dDartObfuscation=true")
            }
            baseFlutterTask.dartDefines.orNull?.let {
                args("--DartDefines=$it")
            }
            baseFlutterTask.bundleSkSLPath.orNull?.let {
                args("-dBundleSkSLPath=$it")
            }
            baseFlutterTask.codeSizeDirectory.orNull?.let {
                args("-dCodeSizeDirectory=$it")
            }
            baseFlutterTask.flavor.orNull?.let {
                args("-dFlavor=$it")
            }
            baseFlutterTask.extraGenSnapshotOptions.orNull?.let {
                args("--ExtraGenSnapshotOptions=$it")
            }
            baseFlutterTask.frontendServerStarterPath.orNull?.let {
                args("-dFrontendServerStarterPath=$it")
            }
            baseFlutterTask.extraFrontEndOptions.orNull?.let {
                args("--ExtraFrontEndOptions=$it")
            }

            args("-dAndroidArchs=${baseFlutterTask.targetPlatformValues.orNull?.joinToString(" ")}")
            args("-dMinSdkVersion=${baseFlutterTask.minSdkVersion.orNull}")
            args(generateRuleNames(baseFlutterTask))
        }

    fun buildBundle(baseFlutterTask: BaseFlutterTask) {
        checkPreConditions(baseFlutterTask)
        baseFlutterTask.logging.captureStandardError(LogLevel.ERROR)
        val execOps = baseFlutterTask.project.serviceOf<ExecOperations>()
        execOps.exec(createExecSpecActionFromTask(baseFlutterTask = baseFlutterTask))
    }
}
