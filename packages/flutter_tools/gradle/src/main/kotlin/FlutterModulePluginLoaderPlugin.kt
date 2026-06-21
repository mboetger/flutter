// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import org.gradle.api.Action
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.initialization.Settings
import org.gradle.api.invocation.Gradle
import org.gradle.kotlin.dsl.*
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import java.io.File
import java.nio.file.Paths

private const val FLUTTER_SDK_PATH = "flutterSdkPath"

/**
 * This plugin applies the native plugin loader plugin (../scripts/native_plugin_loader.gradle.kts)
 * and then configures the module project to `include` each of the loaded flutter plugins.
 */
@Suppress("unused") // This class is used by packages/flutter_tools/gradle/build.gradle.kts.
class FlutterModulePluginLoaderPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) {
        val flutterProject = settings.findProject(":flutter") ?: throw IllegalStateException(
            "The project ':flutter' must be included before applying 'dev.flutter.flutter-module-plugin-loader' plugin."
        )

        val flutterProjectRoot: File = flutterProject.projectDir.parentFile.parentFile
        val androidDir = flutterProject.projectDir.parentFile

        var flutterSdkPath = if (settings.extraProperties.has(FLUTTER_SDK_PATH)) {
            settings.extraProperties.get(FLUTTER_SDK_PATH) as? String
        } else {
            null
        }

        if (flutterSdkPath == null) {
            val localPropertiesFile = File(androidDir, "local.properties")
            check(localPropertiesFile.exists()) {
                "local.properties file not found at ${localPropertiesFile.absolutePath}. " +
                "You must run `flutter pub get` in ${flutterProjectRoot.absolutePath}."
            }
            val properties = FlutterPluginUtils.readPropertiesIfExist(localPropertiesFile)
            val flutterSdk = properties.getProperty("flutter.sdk")
            checkNotNull(flutterSdk) {
                "flutter.sdk not set in local.properties at ${localPropertiesFile.absolutePath}. " +
                "You must run `flutter pub get` in ${flutterProjectRoot.absolutePath} to regenerate it."
            }
            settings.extraProperties.set(FLUTTER_SDK_PATH, flutterSdk)
            flutterSdkPath = flutterSdk
        }

        settings.apply {
            from(
                Paths.get(
                    flutterSdkPath,
                    "packages",
                    "flutter_tools",
                    "gradle",
                    "src",
                    "main",
                    "scripts",
                    "native_plugin_loader.gradle.kts"
                )
            )
        }

        val nativePlugins = NativePluginLoaderReflectionBridge.getPlugins(settings.extraProperties, flutterProjectRoot)
        nativePlugins.forEach { androidPlugin ->
            val pluginDirectory = File(androidPlugin["path"] as String, "android")
            check(pluginDirectory.exists()) { "Plugin directory does not exist: ${pluginDirectory.absolutePath}" }
            val pluginName = androidPlugin["name"] as String
            settings.include(":$pluginName")
            settings.project(":$pluginName").projectDir = pluginDirectory
        }

        val nativePluginNames = nativePlugins.map { it["name"] as String }.toSet()
        val flutterModulePath = androidDir.absolutePath

        settings.gradle.projectsLoaded(Action<Gradle> {
            rootProject.beforeEvaluate(Action<Project> {
                subprojects(Action<Project> {
                    if (nativePluginNames.contains(name)) {
                        val androidPluginBuildOutputDir = File(
                            File(flutterModulePath, "plugins_build_output"),
                            name
                        )
                        if (!androidPluginBuildOutputDir.exists()) {
                            androidPluginBuildOutputDir.mkdirs()
                        }
                        layout.buildDirectory.fileValue(androidPluginBuildOutputDir)
                    }
                })
                if (settings.extraProperties.has("mainModuleName")) {
                    val mainModuleName = settings.extraProperties.get("mainModuleName") as? String
                    if (!mainModuleName.isNullOrEmpty()) {
                        extensions.extraProperties.set("mainModuleName", mainModuleName)
                    }
                }
            })
            rootProject.afterEvaluate(Action<Project> {
                subprojects(Action<Project> {
                    if (name != "flutter") {
                        evaluationDependsOn(":flutter")
                    }
                })
            })
        })
    }
}
