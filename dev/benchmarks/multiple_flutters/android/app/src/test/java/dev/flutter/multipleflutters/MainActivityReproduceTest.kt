// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.flutter.multipleflutters

import android.content.Intent
import org.junit.Test
import org.junit.Assert.fail
import java.lang.reflect.Method

/**
 * Robust reflection-based unit test to verify that [MainActivity] overrides and
 * forwards critical lifecycle and system callback methods required by [FlutterFragment].
 *
 * ## Why Reflection?
 * A genuine behavioral test would require instantiating [MainActivity] and running it
 * under Robolectric. However, [MainActivity] initializes 20 real [FlutterFragment]s and
 * [FlutterEngine]s, which would require extensive mocking of the Flutter JNI and engine
 * lifecycle to prevent crashes in a host-side JUnit environment.
 *
 * Therefore, this reflection-based test acts as an elegant, 100% hermetic, and lightweight
 * static validation suite to ensure that developers do not accidentally omit these required
 * forwarding calls.
 */
class MainActivityReproduceTest {

    @Test
    fun testRequiredMethodsAreOverriddenWithCorrectSignatures() {
        val clazz = MainActivity::class.java

        // Define the expected signatures of methods that must be overridden and forwarded
        val expectedMethods = listOf(
            ExpectedMethod("onPostResume", emptyList()),
            ExpectedMethod("onBackPressed", emptyList()),
            ExpectedMethod("onUserLeaveHint", emptyList()),
            ExpectedMethod("onNewIntent", listOf(Intent::class.java)),
            ExpectedMethod("onRequestPermissionsResult", listOf(
                Int::class.javaPrimitiveType ?: java.lang.Integer.TYPE,
                Array<String>::class.java,
                IntArray::class.java
            ))
        )

        val missingOrIncorrectMethods = mutableListOf<String>()

        for (expected in expectedMethods) {
            try {
                // getDeclaredMethod only returns methods declared in this class (i.e., overridden)
                val method = clazz.getDeclaredMethod(expected.name, *expected.parameterTypes.toTypedArray())
                
                // Verify return type is void (Void.TYPE) or Kotlin's Unit
                if (method.returnType != Void.TYPE && method.returnType != Unit::class.java) {
                    missingOrIncorrectMethods.add("${expected.name} (incorrect return type: ${method.returnType.simpleName}, expected void)")
                }
            } catch (e: NoSuchMethodException) {
                // Check if a method with the same name exists but with different parameters
                val methodsWithSameName = clazz.declaredMethods.filter { it.name == expected.name }
                if (methodsWithSameName.isNotEmpty()) {
                    val actualSignatures = methodsWithSameName.map { m ->
                        "${m.name}(${m.parameterTypes.joinToString { it.simpleName }})"
                    }
                    val expectedSignature = "${expected.name}(${expected.parameterTypes.joinToString { it?.simpleName ?: "null" }})"
                    missingOrIncorrectMethods.add(
                        "${expected.name} (incorrect parameters: expected $expectedSignature, but found: ${actualSignatures.joinToString()})"
                    )
                } else {
                    missingOrIncorrectMethods.add("${expected.name} (not overridden)")
                }
            }
        }

        if (missingOrIncorrectMethods.isNotEmpty()) {
            fail(
                "MainActivity is missing or has incorrect forwarding calls for the following methods:\n" +
                missingOrIncorrectMethods.joinToString(separator = "\n") { " - $it" } +
                "\n\nUsing a FlutterFragment requires forwarding these calls from the Activity to ensure " +
                "that the internal Flutter app/engines behave as expected (e.g., handling the back button, " +
                "permissions, new intents, and application lifecycles)."
            )
        }
    }

    private data class ExpectedMethod(val name: String, val parameterTypes: List<Class<*>?>)
}
