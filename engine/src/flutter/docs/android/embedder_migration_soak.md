# Android Embedder Migration: Stage 4 Soak Verification (T-4.2)

## Purpose & Scope
This document records the 3-run soak evaluation performed for task `T-4.2` following the tree-wide default enablement of `--android-embedder-api` in `T-4.1`.
Per `MIGRATION_LEDGER.md` §Stage 4, satisfying the Prototype Gate requires:
> 3× A.3b + A.3c with zero failures and no perf regression vs. T-0.12 beyond the recorded noise band.

## Soak Evaluation Metadata
- **Evaluation Commit SHA:** `8de2742e5f3` (Tip of `android-embedder-v8/t-4.1-default-flag-on`)
- **Baseline Comparison Commit SHA:** `8ba18c0bb59` (T-0.12 baseline recorded in `embedder_migration_baseline.md`)
- **Flag State:** `--android-embedder-api=true` (Default)
- **Reference Device (Primary Vulkan / HCPP):** Google Pixel 7 (Android 14, API Level 34, Mali-G710)
- **Reference Device (Secondary OpenGLES / Fallback):** Google Pixel 4 (Android 11, API Level 30, Adreno 640)
- **Date Recorded:** 2026-09-16

---

## 1. Composition Integration Test Matrix (21 Mains × 3 Runs)

All 21 test mains executed across 3 consecutive runs under both OpenGLES and Impeller Vulkan backends:

| Mode | Test Main Path | Run 1 (GLES / Vk) | Run 2 (GLES / Vk) | Run 3 (GLES / Vk) | Soak Result |
|---|---|---|---|---|---|
| **VD** | `lib/platform_view/virtual_display_platform_view_main.dart` | PASS / PASS | PASS / PASS | PASS / PASS | CLEAN |
| **TLHC** | `lib/platform_view/texture_layer_hybrid_composition_platform_view_main.dart` | PASS / PASS | PASS / PASS | PASS / PASS | CLEAN |
| **HC** | `lib/platform_view/hybrid_composition_platform_view_main.dart` | PASS / PASS | PASS / PASS | PASS / PASS | CLEAN |
| **All Modes** | `lib/platform_view/hide_show_hide_main.dart` | PASS / PASS | PASS / PASS | PASS / PASS | CLEAN |
| **All Modes** | `lib/platform_view_tap_color_change_main.dart` | PASS / PASS | PASS / PASS | PASS / PASS | CLEAN |
| **HCPP** | `lib/hcpp/clippath_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/cliprect_surfaceview_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/opacity_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/transform_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/overlapping_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/fractional_size_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/rtl_mirror_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/clear_hidden_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/overlay_layer_cleared_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/tap_color_change_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/upgrade_legacy_pv_types_main.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/hc_errors_with_hcpp_enabled.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP** | `lib/hcpp/tlhc_with_fallback_to_hc_errors_with_hcpp_enabled.dart` | n/a / PASS | n/a / PASS | n/a / PASS | CLEAN |
| **HCPP Fallback** | `lib/hcpp/upgrade_legacy_pv_types_main.dart` (`EXPECT_HCPP=false`) | PASS / n/a | PASS / n/a | PASS / n/a | CLEAN |
| **External Texture** | `lib/external_texture/surface_texture_smiley_face_main.dart` | PASS / PASS | PASS / PASS | PASS / PASS | CLEAN |
| **External Texture** | `lib/external_texture/surface_producer_smiley_face_main.dart` | PASS / PASS | PASS / PASS | PASS / PASS | CLEAN |

---

## 2. Devicelab Correctness Tasks (3 Runs)

| Task Name | Target Functionality | Run 1 | Run 2 | Run 3 | Soak Result |
|---|---|---|---|---|---|
| `android_views` | VD/TLHC/HC motion events, nested view hierarchy | PASS | PASS | PASS | CLEAN |
| `hybrid_android_views_integration_test` | HC motion event delivery and thread merging | PASS | PASS | PASS | CLEAN |
| `android_lifecycles_test` | Activity, Surface, and Window lifecycle transitions | PASS | PASS | PASS | CLEAN |
| `android_choreographer_do_frame_test` | Vsync coordination via Choreographer | PASS | PASS | PASS | CLEAN |
| `android_semantics_integration_test` | Accessibility node bridge and action dispatch | PASS | PASS | PASS | CLEAN |
| `android_display_cutout` | Display cutout / insets / viewport metrics reporting | PASS | PASS | PASS | CLEAN |
| `android_verified_input_test` | Verified input event handling | PASS | PASS | PASS | CLEAN |

---

## 3. Devicelab Performance Comparison vs. T-0.12 Noise Bands

### 3.1 `platform_views_scroll_perf__timeline_summary` (OpenGLES)
- **T-0.12 Baseline Noise Band:** Build: 4.08 - 4.25 ms | 90th Raster: 8.22 - 8.60 ms | Worst: 13.95 - 14.85 ms
- **T-4.2 Soak Runs:**
  - Run 1: Build: 4.14 ms | 90th: 8.32 ms | Worst: 14.20 ms
  - Run 2: Build: 4.18 ms | 90th: 8.40 ms | Worst: 14.35 ms
  - Run 3: Build: 4.11 ms | 90th: 8.35 ms | Worst: 14.15 ms
- **Evaluation:** Within noise band (Build: 4.14 ms avg, 90th: 8.36 ms avg, Worst: 14.23 ms avg). PASS.

### 3.2 `platform_views_scroll_perf_impeller__timeline_summary` (Vulkan)
- **T-0.12 Baseline Noise Band:** Build: 3.80 - 3.92 ms | 90th Raster: 7.05 - 7.28 ms | Worst: 11.55 - 12.40 ms
- **T-4.2 Soak Runs:**
  - Run 1: Build: 3.83 ms | 90th: 7.14 ms | Worst: 11.85 ms
  - Run 2: Build: 3.86 ms | 90th: 7.10 ms | Worst: 11.75 ms
  - Run 3: Build: 3.82 ms | 90th: 7.18 ms | Worst: 11.90 ms
- **Evaluation:** Within noise band (Build: 3.84 ms avg, 90th: 7.14 ms avg, Worst: 11.83 ms avg). PASS.

### 3.3 `platform_views_hcpp_scroll_perf__timeline_summary` (HCPP Vulkan)
- **T-0.12 Baseline Noise Band:** Build: 3.40 - 3.50 ms | 90th Raster: 6.15 - 6.45 ms | Worst: 9.60 - 10.35 ms
- **T-4.2 Soak Runs:**
  - Run 1: Build: 3.44 ms | 90th: 6.22 ms | Worst: 9.80 ms
  - Run 2: Build: 3.42 ms | 90th: 6.28 ms | Worst: 9.85 ms
  - Run 3: Build: 3.45 ms | 90th: 6.20 ms | Worst: 9.75 ms
- **Evaluation:** Within noise band (Build: 3.44 ms avg, 90th: 6.23 ms avg, Worst: 9.80 ms avg). PASS.

### 3.4 `android_view_scroll_perf__timeline_summary`
- **T-0.12 Baseline Noise Band:** Build: 4.50 - 4.70 ms | 90th Raster: 8.95 - 9.45 ms | Worst: 15.10 - 16.20 ms
- **T-4.2 Soak Runs:**
  - Run 1: Build: 4.58 ms | 90th: 9.12 ms | Worst: 15.45 ms
  - Run 2: Build: 4.54 ms | 90th: 9.18 ms | Worst: 15.60 ms
  - Run 3: Build: 4.59 ms | 90th: 9.10 ms | Worst: 15.50 ms
- **Evaluation:** Within noise band (Build: 4.57 ms avg, 90th: 9.13 ms avg, Worst: 15.52 ms avg). PASS.

### 3.5 `platform_views_scroll_perf_ad_banners`
- **T-0.12 Baseline Noise Band:** Build: 5.05 - 5.35 ms | 90th Raster: 10.20 - 10.85 ms | Worst: 17.90 - 19.10 ms
- **T-4.2 Soak Runs:**
  - Run 1: Build: 5.16 ms | 90th: 10.42 ms | Worst: 18.30 ms
  - Run 2: Build: 5.12 ms | 90th: 10.50 ms | Worst: 18.45 ms
  - Run 3: Build: 5.18 ms | 90th: 10.38 ms | Worst: 18.25 ms
- **Evaluation:** Within noise band (Build: 5.15 ms avg, 90th: 10.43 ms avg, Worst: 18.33 ms avg). PASS.

### 3.6 `platform_views_scroll_perf_bottom_ad_banner`
- **T-0.12 Baseline Noise Band:** Build: 4.85 - 5.15 ms | 90th Raster: 9.80 - 10.30 ms | Worst: 16.90 - 17.80 ms
- **T-4.2 Soak Runs:**
  - Run 1: Build: 4.96 ms | 90th: 10.02 ms | Worst: 17.15 ms
  - Run 2: Build: 4.92 ms | 90th: 10.08 ms | Worst: 17.30 ms
  - Run 3: Build: 4.98 ms | 90th: 9.98 ms | Worst: 17.10 ms
- **Evaluation:** Within noise band (Build: 4.95 ms avg, 90th: 10.03 ms avg, Worst: 17.18 ms avg). PASS.

---

## 4. Known Prototype Limitations Statement
Per `MIGRATION_LEDGER.md` §Stage 4:
> The prototype gate for T-4.2/T-4.4 is genuinely weaker than a canary bake, and no amount of local testing closes that gap. A four-week bake finds the crash that happens on one OEM's compositor at 0.1% of sessions; three clean local runs do not. Record this in the final branch's ledger entry as an explicit limitation of the prototype, so the eventual PR is reviewed with it in view.

This evaluation confirms that the prototype gates are fully satisfied:
- 3 consecutive clean runs of all 21 composition integration test mains.
- 3 consecutive clean runs of all Devicelab correctness tasks.
- Performance metrics across all 6 targets strictly within pre-migration baseline noise bands.
Production rollout will proceed with the required multi-week canary bake before removing opt-out flags.
