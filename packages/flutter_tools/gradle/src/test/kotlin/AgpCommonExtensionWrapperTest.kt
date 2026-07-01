// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import com.android.build.api.dsl.ApplicationExtension
import com.android.build.api.dsl.LibraryExtension
import com.android.build.api.dsl.DynamicFeatureExtension
import com.android.build.api.dsl.TestExtension
import com.android.build.api.dsl.Splits
import com.android.build.api.dsl.Ndk
import io.mockk.every
import io.mockk.mockk
import kotlin.test.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertSame
import kotlin.test.assertEquals

class AgpCommonExtensionWrapperTest {
    @Suppress("UNCHECKED_CAST")
    private fun <T> unsafeNull(): T = null as T

    @Test
    fun `splits delegates to the backing application extension`() {
        val mockSplits = mockk<Splits>(relaxed = true)
        val mockApplicationExtension =
            mockk<ApplicationExtension>(relaxed = true) {
                every { splits } returns mockSplits
            }

        val wrapper = AgpCommonExtensionWrapper(mockApplicationExtension)

        assertSame(mockSplits, wrapper.splits)
    }

    @Test
    fun `splits delegates to the backing library extension`() {
        val mockSplits = mockk<Splits>(relaxed = true)
        val mockLibraryExtension =
            mockk<LibraryExtension>(relaxed = true) {
                every { splits } returns mockSplits
            }

        val wrapper = AgpCommonExtensionWrapper(mockLibraryExtension)

        assertSame(mockSplits, wrapper.splits)
    }

    @Test
    fun `splits throws for an unsupported backing extension type`() {
        val wrapper = AgpCommonExtensionWrapper("not an android extension")

        assertFailsWith<IllegalArgumentException> { wrapper.splits }
    }

    @Test
    fun `abiFilters delegates to the backing application extension`() {
        val mockNdk = mockk<Ndk> {
            every { abiFilters } returns mutableSetOf("arm64-v8a")
        }
        val mockApplicationExtension = mockk<ApplicationExtension>(relaxed = true) {
            every { defaultConfig.ndk } returns mockNdk
        }
        val wrapper = AgpCommonExtensionWrapper(mockApplicationExtension)
        assertEquals(setOf("arm64-v8a"), wrapper.abiFilters)
    }

    @Test
    fun `abiFilters delegates to the backing library extension`() {
        val mockNdk = mockk<Ndk> {
            every { abiFilters } returns mutableSetOf("armeabi-v7a")
        }
        val mockLibraryExtension = mockk<LibraryExtension>(relaxed = true) {
            every { defaultConfig.ndk } returns mockNdk
        }
        val wrapper = AgpCommonExtensionWrapper(mockLibraryExtension)
        assertEquals(setOf("armeabi-v7a"), wrapper.abiFilters)
    }

    @Test
    fun `abiFilters handles null abiFilters by returning emptySet`() {
        val mockNdk = mockk<Ndk> {
            every { abiFilters } returns unsafeNull()
        }
        val mockApplicationExtension = mockk<ApplicationExtension>(relaxed = true) {
            every { defaultConfig.ndk } returns mockNdk
        }
        val wrapper = AgpCommonExtensionWrapper(mockApplicationExtension)
        assertEquals(emptySet(), wrapper.abiFilters)
    }

    @Test
    fun `abiFilters throws for an unsupported backing extension type`() {
        val wrapper = AgpCommonExtensionWrapper("not an android extension")
        assertFailsWith<IllegalArgumentException> { wrapper.abiFilters }
    }
}
