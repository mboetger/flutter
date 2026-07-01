// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import org.gradle.api.Plugin
import org.gradle.api.initialization.Settings
import org.jetbrains.kotlin.gradle.plugin.extraProperties
import java.io.File
import java.nio.file.Paths
import java.util.Properties

private const val FLUTTER_SDK_PATH = "flutterSdkPath"

// Integration tests that cover this class include
// - packages/flutter_tools/test/integration.shard/android_gradle_daemon_cache_test.dart
// - packages/flutter_tools/test/integration.shard/android_plugin_compilesdkversion_mismatch_test.dart
// And can be run by following the README in  packages/flutter_tools/.

/**
 * This plugin applies the native plugin loader plugin (../scripts/native_plugin_loader.gradle.kts)
 * and then configures the main project to `include` each of the loaded flutter plugins.
 */
@Suppress("unused") // This class is used by packages/flutter_tools/gradle/build.gradle.kts.
class FlutterAppPluginLoaderPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) {
        val flutterProjectRoot: File = settings.settingsDir.parentFile

        if (!settings.extraProperties.has(FLUTTER_SDK_PATH)) {
            val properties = Properties()
            val localPropertiesFile = File(settings.rootProject.projectDir, "local.properties")
            localPropertiesFile.inputStream().use { properties.load(it) }
            settings.extraProperties.set(FLUTTER_SDK_PATH, properties.getProperty("flutter.sdk"))
            assert(
                settings.extraProperties.get(FLUTTER_SDK_PATH) != null
            ) { "flutter.sdk not set in local.properties" }
        }

        settings.apply {
            from(
                Paths.get(
                    settings.extraProperties.get(FLUTTER_SDK_PATH) as String,
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

        NativePluginLoaderReflectionBridge
            .getPlugins(settings.extraProperties, flutterProjectRoot)
            .forEach { androidPlugin ->
                val pluginDirectory = File(androidPlugin["path"] as String, "android")
                check(
                    pluginDirectory.exists()
                ) { "Plugin directory does not exist: ${pluginDirectory.absolutePath}" }
                val pluginName = androidPlugin["name"] as String
                settings.include(":$pluginName")
                settings.project(":$pluginName").projectDir = pluginDirectory

                // Also include subprojects of the plugin.
                val settingsGradle = File(pluginDirectory, "settings.gradle")
                val settingsGradleKts = File(pluginDirectory, "settings.gradle.kts")
                val subprojects = findSubprojects(settingsGradle) + findSubprojects(settingsGradleKts)
                subprojects.distinct().forEach { subproject ->
                    val relativePath = subproject.removePrefix(":")
                    val subprojectDirName = relativePath.replace(":", File.separator)
                    val subprojectDirectory = File(pluginDirectory, subprojectDirName)
                    if (!subprojectDirectory.exists()) {
                        throw org.gradle.api.GradleException(
                            "Plugin '$pluginName' declares subproject '$subproject', " +
                            "but the directory '${subprojectDirectory.absolutePath}' does not exist. " +
                            "Note that custom 'projectDir' assignments in the plugin's settings.gradle are not supported."
                        )
                    }

                    val existingProject = settings.findProject(subproject)
                    if (existingProject != null) {
                        val existingProjectDir = existingProject.projectDir.canonicalFile
                        val newProjectDir = subprojectDirectory.canonicalFile
                        if (existingProjectDir != newProjectDir) {
                            throw org.gradle.api.GradleException(
                                "Plugin '$pluginName' is trying to include Gradle subproject '$subproject' mapped to '$newProjectDir', " +
                                "but it is already defined by another plugin or the app mapped to '$existingProjectDir'. " +
                                "To resolve this, ensure all Gradle subprojects have unique names across all plugins."
                            )
                        }
                    } else {
                        settings.include(subproject)
                        settings.project(subproject).projectDir = subprojectDirectory
                    }
                }
            }
    }

    /**
     * Finds all subprojects defined in a plugin's settings.gradle or settings.gradle.kts.
     *
     * Note: This parser does not support custom `projectDir` assignments for subprojects.
     * It assumes the subprojects follow the default Gradle directory layout (i.e., the
     * directory path matches the colon-separated project path relative to the settings file).
     */
    private fun findSubprojects(gradleSettingsFile: File): List<String> {
        if (!gradleSettingsFile.exists()) {
            return emptyList()
        }
        val subprojects = mutableListOf<String>()
        val content = gradleSettingsFile.readText().replace(Regex("//.*|/\\*(?s).*?\\*/"), "")

        // 1. Match parenthesized includes: include("a", "b") - can be multi-line
        val includeWithParens = """\binclude\s*\(\s*([^)]+)\)""".toRegex(RegexOption.DOT_MATCHES_ALL)
        includeWithParens.findAll(content).forEach { match ->
            val arguments = match.groupValues[1]
            val stringRegex = """['"]([^'"]+)['"]""".toRegex()
            stringRegex.findAll(arguments).forEach { stringMatch ->
                val projectPath = stringMatch.groupValues[1]
                subprojects.add(if (projectPath.startsWith(":")) projectPath else ":$projectPath")
            }
        }

        // 2. Match non-parenthesized includes: include "a", "b" - single line or terminated by semicolon
        val includeWithoutParens = """\binclude\s+([^;\n]+)""".toRegex()
        includeWithoutParens.findAll(content).forEach { match ->
            val arguments = match.groupValues[1]
            val stringRegex = """['"]([^'"]+)['"]""".toRegex()
            stringRegex.findAll(arguments).forEach { stringMatch ->
                val projectPath = stringMatch.groupValues[1]
                subprojects.add(if (projectPath.startsWith(":")) projectPath else ":$projectPath")
            }
        }

        return subprojects.distinct()
    }
}
