// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle.tasks

import org.gradle.api.Project
import org.gradle.api.provider.Property
import org.gradle.api.provider.ListProperty
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.file.RegularFileProperty
import org.gradle.testfixtures.ProjectBuilder
import org.junit.jupiter.api.Test
import kotlin.test.assertTrue

class BaseFlutterTaskLazyPropertiesReproduceTest {

    @Test
    fun `properties should use Gradle lazy configuration types`() {
        // 1. Sanity Check: Verify that Gradle can successfully instantiate the task.
        // This ensures that refactoring (e.g., making the class abstract) does not break Gradle task creation.
        val project = ProjectBuilder.builder().build()
        val task = project.tasks.create("baseFlutterTask", BaseFlutterTask::class.java)
        assertTrue(task is BaseFlutterTask, "Task should be an instance of BaseFlutterTask")

        // 2. Structural Check: Verify all properties use lazy types.
        val lazyPropertyFields = listOf(
            "flutterRoot" to DirectoryProperty::class.java,
            "flutterExecutable" to RegularFileProperty::class.java,
            "buildMode" to Property::class.java,
            "minSdkVersion" to Property::class.java,
            "localEngine" to Property::class.java,
            "localEngineHost" to Property::class.java,
            "localEngineSrcPath" to Property::class.java,
            "targetPath" to Property::class.java,
            "verbose" to Property::class.java,
            "fileSystemRoots" to ListProperty::class.java,
            "fileSystemScheme" to Property::class.java,
            "trackWidgetCreation" to Property::class.java,
            "targetPlatformValues" to ListProperty::class.java,
            "sourceDir" to DirectoryProperty::class.java,
            "intermediateDir" to DirectoryProperty::class.java,
            "frontendServerStarterPath" to Property::class.java,
            "extraFrontEndOptions" to Property::class.java,
            "extraGenSnapshotOptions" to Property::class.java,
            "splitDebugInfo" to Property::class.java,
            "treeShakeIcons" to Property::class.java,
            "dartObfuscation" to Property::class.java,
            "dartDefines" to Property::class.java,
            "bundleSkSLPath" to Property::class.java,
            "codeSizeDirectory" to Property::class.java,
            "performanceMeasurementFile" to Property::class.java,
            "deferredComponents" to Property::class.java,
            "validateDeferredComponents" to Property::class.java,
            "skipDependencyChecks" to Property::class.java,
            "flavor" to Property::class.java
        )

        val errors = mutableListOf<String>()

        for ((fieldName, expectedType) in lazyPropertyFields) {
            try {
                val getterName = "get" + fieldName.replaceFirstChar { it.uppercase() }
                val method = try {
                    BaseFlutterTask::class.java.getMethod(getterName)
                } catch (e: NoSuchMethodException) {
                    null
                }
                
                val actualType = method?.returnType ?: try {
                    val field = BaseFlutterTask::class.java.getDeclaredField(fieldName)
                    field.type
                } catch (e: NoSuchFieldException) {
                    null
                }

                if (actualType == null) {
                    errors.add("Property '$fieldName' not found in BaseFlutterTask")
                } else if (!expectedType.isAssignableFrom(actualType)) {
                    errors.add("Property '$fieldName' is of type '${actualType.name}', but expected a lazy type assignable to '${expectedType.name}'")
                }
            } catch (e: Exception) {
                errors.add("Failed to inspect property '$fieldName': ${e.message}")
            }
        }

        assertTrue(errors.isEmpty(), "The following properties are not using lazy configuration types:\n" + errors.joinToString("\n"))
    }
}
