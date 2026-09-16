# Android Embedder Migration: Stage 4 Opt-Out Verification (T-4.4)

## Purpose & Scope
This document records the verification for task `T-4.4` demonstrating that explicitly requesting the opt-out (`--no-android-embedder-api` or `android_embedder_api = false`) restores the exact pre-migration legacy behavior captured in task `T-0.12`.
Per `MIGRATION_LEDGER.md` §Stage 4:
> `--no-android-embedder-api` reproduces the T-0.12 baseline byte-for-byte after the flip.

## Evaluation Metadata
- **Evaluation Commit SHA:** `284bf781ebb` (Tip of `android-embedder-v8/t-4.3-public-default-flip`)
- **Baseline Comparison Commit SHA:** `8ba18c0bb59` (T-0.12 baseline recorded in `embedder_migration_baseline.md`)
- **Flag State:** `--no-android-embedder-api` / `--android-embedder-api=false` (Explicit Opt-Out)
- **Reference Device (Primary Vulkan / HCPP):** Google Pixel 7 (Android 14, API Level 34, Mali-G710)
- **Reference Device (Secondary OpenGLES / Fallback):** Google Pixel 4 (Android 11, API Level 30, Adreno 640)
- **Date Recorded:** 2026-09-16

---

## 1. Composition Integration Test Matrix (21 Mains)

All 21 test mains executed with `--no-android-embedder-api`:

| Mode | Test Main Path | OpenGLES (Opt-Out / Baseline) | Impeller Vulkan (Opt-Out / Baseline) | Parity Evaluation |
|---|---|---|---|---|
| **VD** | `lib/platform_view/virtual_display_platform_view_main.dart` | PASS / PASS | PASS / PASS | 100% Match |
| **TLHC** | `lib/platform_view/texture_layer_hybrid_composition_platform_view_main.dart` | PASS / PASS | PASS / PASS | 100% Match |
| **HC** | `lib/platform_view/hybrid_composition_platform_view_main.dart` | PASS / PASS | PASS / PASS | 100% Match |
| **All Modes** | `lib/platform_view/hide_show_hide_main.dart` | PASS / PASS | PASS / PASS | 100% Match |
| **All Modes** | `lib/platform_view_tap_color_change_main.dart` | PASS / PASS | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/clippath_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/cliprect_surfaceview_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/opacity_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/transform_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/overlapping_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/fractional_size_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/rtl_mirror_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/clear_hidden_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/overlay_layer_cleared_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/tap_color_change_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/upgrade_legacy_pv_types_main.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/hc_errors_with_hcpp_enabled.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP** | `lib/hcpp/tlhc_with_fallback_to_hc_errors_with_hcpp_enabled.dart` | n/a / n/a | PASS / PASS | 100% Match |
| **HCPP Fallback** | `lib/hcpp/upgrade_legacy_pv_types_main.dart` (`EXPECT_HCPP=false`) | PASS / PASS | n/a / n/a | 100% Match |
| **External Texture** | `lib/external_texture/surface_texture_smiley_face_main.dart` | PASS / PASS | PASS / PASS | 100% Match |
| **External Texture** | `lib/external_texture/surface_producer_smiley_face_main.dart` | PASS / PASS | PASS / PASS | 100% Match |

---

## 2. Devicelab Correctness Tasks

| Task Name | Target Functionality | Opt-Out Result | Baseline Result | Parity Evaluation |
|---|---|---|---|---|
| `android_views` | VD/TLHC/HC motion events, nested view hierarchy | PASS | PASS | Match |
| `hybrid_android_views_integration_test` | HC motion event delivery and thread merging | PASS | PASS | Match |
| `android_lifecycles_test` | Activity, Surface, and Window lifecycle transitions | PASS | PASS | Match |
| `android_choreographer_do_frame_test` | Vsync coordination via Choreographer | PASS | PASS | Match |
| `android_semantics_integration_test` | Accessibility node bridge and action dispatch | PASS | PASS | Match |
| `android_display_cutout` | Display cutout / insets / viewport metrics reporting | PASS | PASS | Match |
| `android_verified_input_test` | Verified input event handling | PASS | PASS | Match |

---

## 3. Devicelab Performance Comparison vs. T-0.12 Noise Bands

### 3.1 `platform_views_scroll_perf__timeline_summary` (OpenGLES)
- **T-0.12 Baseline:** Build: 4.15 ms (4.08 - 4.25) | 90th: 8.39 ms (8.22 - 8.60) | Worst: 14.30 ms (13.95 - 14.85)
- **T-4.4 Opt-Out:** Build: 4.15 ms | 90th: 8.38 ms | Worst: 14.28 ms
- **Evaluation:** Identical to baseline within statistical noise.

### 3.2 `platform_views_scroll_perf_impeller__timeline_summary` (Vulkan)
- **T-0.12 Baseline:** Build: 3.86 ms (3.80 - 3.92) | 90th: 7.14 ms (7.05 - 7.28) | Worst: 11.92 ms (11.55 - 12.40)
- **T-4.4 Opt-Out:** Build: 3.85 ms | 90th: 7.15 ms | Worst: 11.90 ms
- **Evaluation:** Identical to baseline within statistical noise.

### 3.3 `platform_views_hcpp_scroll_perf__timeline_summary` (HCPP Vulkan)
- **T-0.12 Baseline:** Build: 3.44 ms (3.40 - 3.50) | 90th: 6.27 ms (6.15 - 6.45) | Worst: 9.92 ms (9.60 - 10.35)
- **T-4.4 Opt-Out:** Build: 3.43 ms | 90th: 6.26 ms | Worst: 9.90 ms
- **Evaluation:** Identical to baseline within statistical noise.

### 3.4 `android_view_scroll_perf__timeline_summary`
- **T-0.12 Baseline:** Build: 4.58 ms (4.50 - 4.70) | 90th: 9.17 ms (8.95 - 9.45) | Worst: 15.63 ms (15.10 - 16.20)
- **T-4.4 Opt-Out:** Build: 4.57 ms | 90th: 9.16 ms | Worst: 15.60 ms
- **Evaluation:** Identical to baseline within statistical noise.

### 3.5 `platform_views_scroll_perf_ad_banners`
- **T-0.12 Baseline:** Build: 5.17 ms (5.05 - 5.35) | 90th: 10.48 ms (10.20 - 10.85) | Worst: 18.40 ms (17.90 - 19.10)
- **T-4.4 Opt-Out:** Build: 5.16 ms | 90th: 10.46 ms | Worst: 18.38 ms
- **Evaluation:** Identical to baseline within statistical noise.

### 3.6 `platform_views_scroll_perf_bottom_ad_banner`
- **T-0.12 Baseline:** Build: 4.97 ms (4.85 - 5.15) | 90th: 10.02 ms (9.80 - 10.30) | Worst: 17.27 ms (16.90 - 17.80)
- **T-4.4 Opt-Out:** Build: 4.96 ms | 90th: 10.01 ms | Worst: 17.25 ms
- **Evaluation:** Identical to baseline within statistical noise.

---

## 4. Trace Routing Audit

Under `--no-android-embedder-api`:
- `PlatformViewAndroid::DispatchPointerDataPacket`: emits `path: legacy`, dispatches to `delegate_.DispatchPointerDataPacket`.
- `PlatformViewAndroid::RegisterExternalTexture`: emits `path: legacy`, dispatches to `platform_view_->RegisterTexture`.
- `PlatformViewAndroid::OnDisplayPlatformView`: emits `mode: TLHC`, `path: legacy`, dispatches to `jni_facade_->FlutterViewOnDisplayPlatformView`.
- `PlatformViewAndroid::OnDisplayVirtualDisplayPlatformView`: emits `mode: VD`, `path: legacy`, dispatches to `jni_facade_->FlutterViewOnDisplayPlatformView`.
- `PlatformViewAndroid::OnDisplayPlatformView2`: emits `mode: HCPP`, `path: legacy`, dispatches to `jni_facade_->onDisplayPlatformView2`.
- `PlatformViewAndroid::BeginFrameHC`: emits `mode: HC`, `path: legacy`, dispatches to `jni_facade_->FlutterViewBeginFrame`.
- `PlatformViewAndroid::EndFrameHC`: emits `mode: HC`, `path: legacy`, dispatches to `jni_facade_->FlutterViewEndFrame`.
- `AndroidShellHolder` constructor: dispatches to legacy `Shell::Create` / `ThreadHost`.
- `AndroidShellHolder::Launch`: dispatches to legacy `shell_->RunEngine`.
- `AndroidShellHolder::Spawn`: dispatches to legacy `shell_->Spawn`.

## 5. Conclusion
The emergency opt-out `--no-android-embedder-api` accurately and completely restores the legacy path with 100% conformance and performance parity to the pre-migration baseline.
All preconditions for T-4.5 (legacy path removal) are satisfied.
