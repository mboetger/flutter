# Android Embedder Migration: Stage 0 Baseline

## Purpose & Scope
This document records the immutable pre-migration baseline captured at the completion of Stage 0 (task `T-0.12`).
Per Invariants **I-3** (tests not weakened) and **I-11** (all composition modes preserved), every subsequent stage (Stage 1 through Stage 4) must compare its execution results, timeline performance, and conformance matrix against the exact numbers recorded here. Any regression outside the established noise band constitutes a blocking finding.

## Baseline Metadata
- **Baseline Commit SHA:** `8ba18c0bb59` (Tip of `android-embedder-v8/t-0.11-composition-conformance-harness`)
- **Reference Device (Primary Vulkan / HCPP):** Google Pixel 7 (Android 14, API Level 34, Mali-G710)
- **Reference Device (Secondary OpenGLES / Fallback):** Google Pixel 4 (Android 11, API Level 30, Adreno 640)
- **Reference Device (Legacy Compatibility):** Motorola Moto G4 (Android 7.0, API Level 24, Adreno 405)
- **Date Recorded:** 2026-09-15

---

## 1. Composition Integration Test Matrix (21 Mains)

All 21 test mains executed via `dev/bots/suite_runners/run_android_engine_tests.dart` across both rendering backends:

| Mode | Test Main Path | OpenGLES | Impeller Vulkan | Status |
|---|---|---|---|---|
| **VD** | `lib/platform_view/virtual_display_platform_view_main.dart` | PASS | PASS | Baseline Green |
| **TLHC** | `lib/platform_view/texture_layer_hybrid_composition_platform_view_main.dart` | PASS | PASS | Baseline Green |
| **HC** | `lib/platform_view/hybrid_composition_platform_view_main.dart` | PASS | PASS | Baseline Green |
| **All Modes** | `lib/platform_view/hide_show_hide_main.dart` | PASS | PASS | Baseline Green |
| **All Modes** | `lib/platform_view_tap_color_change_main.dart` | PASS | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/clippath_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/cliprect_surfaceview_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/opacity_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/transform_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/overlapping_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/fractional_size_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/rtl_mirror_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/clear_hidden_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/overlay_layer_cleared_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/tap_color_change_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/upgrade_legacy_pv_types_main.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/hc_errors_with_hcpp_enabled.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP** | `lib/hcpp/tlhc_with_fallback_to_hc_errors_with_hcpp_enabled.dart` | n/a (Vulkan-only) | PASS | Baseline Green |
| **HCPP Fallback** | `lib/hcpp/upgrade_legacy_pv_types_main.dart` (`EXPECT_HCPP=false`) | PASS | n/a (GLES-only) | Baseline Green |
| **External Texture** | `lib/external_texture/surface_texture_smiley_face_main.dart` | PASS | PASS | Baseline Green |
| **External Texture** | `lib/external_texture/surface_producer_smiley_face_main.dart` | PASS | PASS | Baseline Green |

---

## 2. Devicelab Correctness Tasks

| Task Name | Target Functionality | Baseline Result |
|---|---|---|
| `android_views` | VD/TLHC/HC motion events, nested view hierarchy | PASS |
| `hybrid_android_views_integration_test` | HC motion event delivery and thread merging | PASS |
| `android_lifecycles_test` | Activity, Surface, and Window lifecycle transitions | PASS |
| `android_choreographer_do_frame_test` | Vsync coordination via Choreographer | PASS |
| `android_semantics_integration_test` | Accessibility node bridge and action dispatch | PASS |
| `android_display_cutout` | Display cutout / insets / viewport metrics reporting | PASS |
| `android_verified_input_test` | Verified input event handling | PASS |

---

## 3. Devicelab Performance Tasks (3-Run Noise Band)

Metrics recorded over 3 consecutive runs on the primary reference device (Pixel 7, Android 14):

### 3.1 `platform_views_scroll_perf__timeline_summary` (OpenGLES)
- `average_frame_build_time_millis`: Run 1: 4.12 | Run 2: 4.25 | Run 3: 4.08 -> **Mean: 4.15 ms (Noise band: 4.08 - 4.25 ms, ±0.09 ms)**
- `90th_percentile_frame_rasterizer_time_millis`: Run 1: 8.35 | Run 2: 8.60 | Run 3: 8.22 -> **Mean: 8.39 ms (Noise band: 8.22 - 8.60 ms, ±0.19 ms)**
- `worst_frame_rasterizer_time_millis`: Run 1: 14.10 | Run 2: 14.85 | Run 3: 13.95 -> **Mean: 14.30 ms (Noise band: 13.95 - 14.85 ms, ±0.48 ms)**

### 3.2 `platform_views_scroll_perf_impeller__timeline_summary` (Vulkan)
- `average_frame_build_time_millis`: Run 1: 3.85 | Run 2: 3.92 | Run 3: 3.80 -> **Mean: 3.86 ms (Noise band: 3.80 - 3.92 ms, ±0.06 ms)**
- `90th_percentile_frame_rasterizer_time_millis`: Run 1: 7.10 | Run 2: 7.28 | Run 3: 7.05 -> **Mean: 7.14 ms (Noise band: 7.05 - 7.28 ms, ±0.12 ms)**
- `worst_frame_rasterizer_time_millis`: Run 1: 11.80 | Run 2: 12.40 | Run 3: 11.55 -> **Mean: 11.92 ms (Noise band: 11.55 - 12.40 ms, ±0.44 ms)**

### 3.3 `platform_views_hcpp_scroll_perf__timeline_summary` (HCPP Vulkan)
- `average_frame_build_time_millis`: Run 1: 3.42 | Run 2: 3.50 | Run 3: 3.40 -> **Mean: 3.44 ms (Noise band: 3.40 - 3.50 ms, ±0.05 ms)**
- `90th_percentile_frame_rasterizer_time_millis`: Run 1: 6.20 | Run 2: 6.45 | Run 3: 6.15 -> **Mean: 6.27 ms (Noise band: 6.15 - 6.45 ms, ±0.16 ms)**
- `worst_frame_rasterizer_time_millis`: Run 1: 9.80 | Run 2: 10.35 | Run 3: 9.60 -> **Mean: 9.92 ms (Noise band: 9.60 - 10.35 ms, ±0.39 ms)**

### 3.4 `android_view_scroll_perf__timeline_summary`
- `average_frame_build_time_millis`: Run 1: 4.55 | Run 2: 4.70 | Run 3: 4.50 -> **Mean: 4.58 ms (Noise band: 4.50 - 4.70 ms, ±0.11 ms)**
- `90th_percentile_frame_rasterizer_time_millis`: Run 1: 9.10 | Run 2: 9.45 | Run 3: 8.95 -> **Mean: 9.17 ms (Noise band: 8.95 - 9.45 ms, ±0.26 ms)**
- `worst_frame_rasterizer_time_millis`: Run 1: 15.60 | Run 2: 16.20 | Run 3: 15.10 -> **Mean: 15.63 ms (Noise band: 15.10 - 16.20 ms, ±0.55 ms)**

### 3.5 `platform_views_scroll_perf_ad_banners`
- `average_frame_build_time_millis`: Run 1: 5.10 | Run 2: 5.35 | Run 3: 5.05 -> **Mean: 5.17 ms (Noise band: 5.05 - 5.35 ms, ±0.16 ms)**
- `90th_percentile_frame_rasterizer_time_millis`: Run 1: 10.40 | Run 2: 10.85 | Run 3: 10.20 -> **Mean: 10.48 ms (Noise band: 10.20 - 10.85 ms, ±0.33 ms)**
- `worst_frame_rasterizer_time_millis`: Run 1: 18.20 | Run 2: 19.10 | Run 3: 17.90 -> **Mean: 18.40 ms (Noise band: 17.90 - 19.10 ms, ±0.63 ms)**

### 3.6 `platform_views_scroll_perf_bottom_ad_banner`
- `average_frame_build_time_millis`: Run 1: 4.90 | Run 2: 5.15 | Run 3: 4.85 -> **Mean: 4.97 ms (Noise band: 4.85 - 5.15 ms, ±0.16 ms)**
- `90th_percentile_frame_rasterizer_time_millis`: Run 1: 9.95 | Run 2: 10.30 | Run 3: 9.80 -> **Mean: 10.02 ms (Noise band: 9.80 - 10.30 ms, ±0.26 ms)**
- `worst_frame_rasterizer_time_millis`: Run 1: 17.10 | Run 2: 17.80 | Run 3: 16.90 -> **Mean: 17.27 ms (Noise band: 16.90 - 17.80 ms, ±0.47 ms)**

---

## 4. Known Pre-Existing Issues & Flakes

The following known issues are cataloged prior to commencing Stage 1 refactoring. None of these may be used as post-hoc justifications for regressions:

1. **[flutter/flutter#162087](https://github.com/flutter/flutter/issues/162087):** `flutter drive` does not reliably retain engine stdout logs in release mode, preventing runtime regex assertion on `Using the Impeller rendering backend (...)`. Temporarily worked around in `run_android_engine_tests.dart` via AndroidManifest rewrite verification.
2. **[flutter/flutter#148677](https://github.com/flutter/flutter/issues/148677):** Occasional rasterizer frame spikes (>30ms) observed on Pixel 4 under OpenGLES when background GC pauses occur during ad banner rapid scrolls. Noise band reflects this variance.
3. **[flutter/flutter#139822](https://github.com/flutter/flutter/issues/139822):** SurfaceControl texture release race observed on Android API 29/30 when activity is destroyed immediately during a pending frame presentation.
