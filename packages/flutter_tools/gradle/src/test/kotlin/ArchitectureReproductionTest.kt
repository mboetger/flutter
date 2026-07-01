// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

import com.android.build.api.dsl.ApplicationExtension
import com.android.build.api.dsl.Ndk
import com.android.build.api.dsl.ApplicationDefaultConfig
import io.mockk.every
import io.mockk.mockk
import org.gradle.api.Project
import org.gradle.api.plugins.ExtensionContainer
import org.gradle.api.UnknownDomainObjectException
import kotlin.test.Test
import kotlin.test.assertEquals

class ArchitectureReproductionTest {

    @Suppress("UNCHECKED_CAST")
    private fun <T> unsafeNull(): T = null as T

    private fun mockProjectWithAbiFilters(
        abiFilters: Set<String>?,
        hasAndroidExtension: Boolean = true,
        hasAppProject: Boolean = true,
        isAppProject: Boolean = false
    ): Project {
        val project = mockk<Project>()
        val rootProject = mockk<Project>()
        val pluginContainer = mockk<org.gradle.api.plugins.PluginContainer>()
        
        every { project.plugins } returns pluginContainer
        every { pluginContainer.hasPlugin("com.android.application") } returns isAppProject
        
        every { project.hasProperty(FlutterPluginUtils.PROP_TARGET_PLATFORM) } returns false
        every { project.rootProject } returns rootProject
        every { rootProject.hasProperty("flutter.hostAppProjectName") } returns false
        
        if (isAppProject) {
            val extensions = mockk<ExtensionContainer>()
            every { project.extensions } returns extensions
            
            if (hasAndroidExtension) {
                val androidExtension = mockk<ApplicationExtension>()
                val defaultConfig = mockk<ApplicationDefaultConfig>()
                val ndk = mockk<Ndk>()
                
                every { extensions.findByName("android") } returns androidExtension
                every { extensions.findByType(ApplicationExtension::class.java) } returns androidExtension
                every { extensions.getByType(ApplicationExtension::class.java) } returns androidExtension
                
                every { androidExtension.defaultConfig } returns defaultConfig
                every { defaultConfig.ndk } returns ndk
                every { ndk.abiFilters } returns (abiFilters?.toMutableSet() ?: unsafeNull())
            } else {
                every { extensions.findByName("android") } returns null
                every { extensions.findByType(any<Class<*>>()) } returns null
                every { extensions.getByType(any<Class<*>>()) } throws org.gradle.api.UnknownDomainObjectException("not found")
            }
            return project
        }
        
        if (!hasAppProject) {
            every { rootProject.findProject(":app") } returns null
            return project
        }

        val appProject = mockk<Project>()
        every { rootProject.findProject(":app") } returns appProject

        val extensions = mockk<ExtensionContainer>()
        every { appProject.extensions } returns extensions

        if (!hasAndroidExtension) {
            every { extensions.findByName("android") } returns null
            every { extensions.findByType(any<Class<*>>()) } returns null
            every { extensions.getByType(any<Class<*>>()) } throws org.gradle.api.UnknownDomainObjectException("not found")
            return project
        }

        val androidExtension = mockk<ApplicationExtension>()
        val defaultConfig = mockk<ApplicationDefaultConfig>()
        val ndk = mockk<Ndk>()

        // Mock both findByName and getByType/findByType to be robust
        every { extensions.findByName("android") } returns androidExtension
        every { extensions.findByType(ApplicationExtension::class.java) } returns androidExtension
        every { extensions.getByType(ApplicationExtension::class.java) } returns androidExtension

        every { androidExtension.defaultConfig } returns defaultConfig
        every { defaultConfig.ndk } returns ndk
        every { ndk.abiFilters } returns (abiFilters?.toMutableSet() ?: unsafeNull())

        return project
    }

    @Test
    fun `getTargetPlatforms should respect ndk abiFilters of the app project`() {
        val project = mockProjectWithAbiFilters(setOf("arm64-v8a"))
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        assertEquals(listOf("android-arm64"), result)
    }

    @Test
    fun `getTargetPlatforms should return default platforms when ndk abiFilters is empty`() {
        val project = mockProjectWithAbiFilters(emptySet())
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        assertEquals(listOf("android-arm", "android-arm64", "android-x64"), result)
    }

    @Test
    fun `getTargetPlatforms should return default platforms when ndk abiFilters is null`() {
        val project = mockProjectWithAbiFilters(null)
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        assertEquals(listOf("android-arm", "android-arm64", "android-x64"), result)
    }

    @Test
    fun `getTargetPlatforms should filter out unsupported ABIs`() {
        val project = mockProjectWithAbiFilters(setOf("arm64-v8a", "x86"))
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        assertEquals(listOf("android-arm64"), result)
    }

    @Test
    fun `getTargetPlatforms should handle multiple supported ABIs`() {
        val project = mockProjectWithAbiFilters(setOf("arm64-v8a", "armeabi-v7a"))
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        // Order should match the default platforms or the order of supported ABIs.
        // We expect both to be present.
        assertEquals(listOf("android-arm", "android-arm64"), result.sorted())
    }

    @Test
    fun `getTargetPlatforms should fallback to default platforms when app project is missing`() {
        val project = mockProjectWithAbiFilters(setOf("arm64-v8a"), hasAppProject = false)
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        assertEquals(listOf("android-arm", "android-arm64", "android-x64"), result)
    }

    @Test
    fun `getTargetPlatforms should fallback to default platforms when android extension is missing`() {
        val project = mockProjectWithAbiFilters(setOf("arm64-v8a"), hasAndroidExtension = false)
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        assertEquals(listOf("android-arm", "android-arm64", "android-x64"), result)
    }

    @Test
    fun `getTargetPlatforms should respect ndk abiFilters of the project itself when it is the app project`() {
        val project = mockProjectWithAbiFilters(setOf("arm64-v8a"), isAppProject = true)
        val result = FlutterPluginUtils.getTargetPlatforms(project)
        assertEquals(listOf("android-arm64"), result)
    }
}
