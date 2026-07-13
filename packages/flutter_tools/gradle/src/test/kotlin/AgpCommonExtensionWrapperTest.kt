// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import com.android.build.api.dsl.ApplicationExtension
import com.android.build.api.dsl.LibraryExtension
import com.android.build.api.dsl.Splits
import io.mockk.every
import io.mockk.mockk
import kotlin.test.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertSame

class AgpCommonExtensionWrapperTest {
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
    fun `addKeepDebugSymbols adds pattern to application extension`() {
        val mockKeepDebugSymbols = mockk<MutableSet<String>>(relaxed = true)
        val mockApplicationExtension = mockk<ApplicationExtension>(relaxed = true) {
            every { packaging.jniLibs.keepDebugSymbols } returns mockKeepDebugSymbols
        }
        val wrapper = AgpCommonExtensionWrapper(mockApplicationExtension)
        wrapper.addKeepDebugSymbols("**/libflutter.so")
        io.mockk.verify { mockKeepDebugSymbols.add("**/libflutter.so") }
    }

    @Test
    fun `addKeepDebugSymbols adds pattern to library extension`() {
        val mockKeepDebugSymbols = mockk<MutableSet<String>>(relaxed = true)
        val mockLibraryExtension = mockk<LibraryExtension>(relaxed = true) {
            every { packaging.jniLibs.keepDebugSymbols } returns mockKeepDebugSymbols
        }
        val wrapper = AgpCommonExtensionWrapper(mockLibraryExtension)
        wrapper.addKeepDebugSymbols("**/libflutter.so")
        io.mockk.verify { mockKeepDebugSymbols.add("**/libflutter.so") }
    }
}
