// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.flutter.gradle

/**
 * The compile SDK configured on a project's Android extension: either a numeric API level
 * (`compileSdk = 36`, optionally with `compileSdkMinor = 2`) or a preview codename
 * (`compileSdkPreview = "Baklava"`). All are null when the DSL has not been configured (yet).
 */
internal data class CompileSdkVersion(
    val apiLevel: Int?,
    val minorApiLevel: Int? = null,
    val previewCodename: String? = null
) {
    init {
        require(apiLevel == null || previewCodename == null) {
            "CompileSdkVersion cannot have both apiLevel ($apiLevel) and previewCodename ($previewCodename) set."
        }
        require(minorApiLevel == null || apiLevel != null) {
            "CompileSdkVersion cannot have minorApiLevel ($minorApiLevel) without apiLevel set."
        }
    }

    /**
     * Whether this compile SDK is known to be higher than [other].
     *
     * - numeric vs numeric: numeric comparison of [apiLevel], breaking ties with [minorApiLevel]
     *   (treating null as 0).
     * - preview vs numeric: a preview codename targets an unreleased SDK, so it is
     *   considered higher than any numeric API level.
     * - preview vs preview: codenames stopped being alphabetically ordered when the
     *   alphabet reset at "Baklava", so distinct codenames are incomparable and this
     *   returns false rather than guessing.
     * - if either side is unset, returns false.
     */
    fun isHigherThan(other: CompileSdkVersion): Boolean {
        if (other.previewCodename != null) {
            return false
        }
        if (previewCodename != null && other.apiLevel != null) {
            return true
        }
        if (apiLevel != null && other.apiLevel != null) {
            if (apiLevel != other.apiLevel) {
                return apiLevel > other.apiLevel
            }
            return (minorApiLevel ?: 0) > (other.minorApiLevel ?: 0)
        }
        return false
    }

    /** The human-readable form used in log messages, e.g. "35", "37.2", or "Baklava". */
    override fun toString(): String =
        previewCodename
            ?: apiLevel?.let { if (minorApiLevel != null) "$it.$minorApiLevel" else it.toString() }
            ?: "unknown"
}
