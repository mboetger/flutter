# Android Embedder CI Mapping and Test Wiring Contract

## 1. Executive Summary

This document establishes the authoritative CI contract for the Android embedder test suite across all stages of the Android Embedder API migration (`android-embedder-v8/*`).
The previous migration attempt (v7) failed because test executables were committed to `BUILD.gn` without being wired into CI runners, leading to false certifications of passes on unverified local environments.
In this migration:
- Every Android native test target is registered in `testing/run_tests.py` and actively exercised in CI.
- An orphan-target check mechanically fails `run_tests.py` if an `executable(...)` target in `shell/platform/android/` is not registered.
- The dependency ratchet (`tools/android_embedder_deps.py --check`) is enforced on test runs.
- AddressSanitizer (ASan) variants are configured for memory-safety characterization.

---

## 2. Test Target to CI Builder Mapping

| Test Target | Target Type | Scope & Coverage | CI Builder Config | Runner Invocation |
|---|---|---|---|---|
| `flutter_shell_native_unittests` | `executable` | Native C++ unit tests: surface lifecycle (T-0.2), threading (T-0.3), viewport metrics (T-0.4), platform message affinity (T-0.4), APK asset provider, LRU cache | `ci/builders/linux_android_emulator.json` (`ci/android_emulator_debug_x64`) | `python3 testing/run_tests.py --android-variant ci/android_emulator_debug_x64 --type android` |
| `flutter_shell_native_unittests` (ASan) | `executable` | Surface lifecycle teardown & GPU memory safety under AddressSanitizer | `ci/builders/linux_android_emulator.json` (`ci/android_emulator_debug_x64_asan`) | `python3 testing/run_tests.py --android-variant ci/android_emulator_debug_x64_asan --type android` |
| `android_external_view_embedder_unittests` | `executable` | Host-side external view embedder tests (HC, TLHC composition layers) | `ci/builders/linux_unopt.json`, `ci/builders/linux_host_engine_test.json` | `python3 testing/run_tests.py --type engine` |
| `jni_unittests` | `executable` | Host-side JNI wrapper and mock environment unit tests | `ci/builders/linux_unopt.json`, `ci/builders/linux_host_engine_test.json` | `python3 testing/run_tests.py --type engine` |
| `platform_view_android_delegate_unittests` | `executable` | Platform view Android delegate host tests | `ci/builders/linux_unopt.json`, `ci/builders/linux_host_engine_test.json` | `python3 testing/run_tests.py --type engine` |
| Robolectric Unit Tests | Gradle JUnit4 | Java-side embedding classes: `FlutterView`, `FlutterRenderer`, `PlatformViewsController`, `DartMessenger`, `FlutterJNI` | Linux Android Engine builders | `python3 testing/run_tests.py --type java` (or `./gradlew test` in `shell/platform/android`) |
| Composition Conformance Matrix | Integration tests | Full matrix (HC, TLHC, VD, HCPP, External Textures) across GLES and Vulkan | On-device CI shards (`android_engine_vulkan_tests`, `android_engine_opengles_tests`) | `SHARD=... bin/cache/dart-sdk/bin/dart dev/bots/test.dart` |
| Devicelab Suites | Hardware E2E | Frame rendering, vsync pacing, touch input, semantics, cutouts, lifecycle | LUCI Devicelab builders (`Linux_android ...`) | `dart dev/devicelab/bin/run.dart -t <suite>` |

---

## 3. Automated Guardrails

### 3.1 Orphan Executable Target Guard
Located in `testing/run_tests.py`: `check_android_embedder_contracts()`.
Whenever `run_tests.py` runs with `--type android` or `--type engine`:
1. It recursively scans `shell/platform/android/**/BUILD.gn` for `executable(...)` declarations.
2. It asserts that every declared executable is referenced in `run_tests.py`.
3. If an unregistered executable is found, execution immediately terminates with a `RuntimeError` naming the target and containing file.

### 3.2 Dependency Ratchet Guard
Located in `testing/run_tests.py`: `check_android_embedder_contracts()`.
Invokes `engine/src/flutter/tools/android_embedder_deps.py --check` on every test run.
Fails non-zero if internal-engine GN dependencies, internal `#include`s, or `// nogncheck` comments exceed the baseline.

---

## 4. Local Execution Commands

```bash
# Run Android emulator native unit tests
python3 engine/src/flutter/testing/run_tests.py --type android --android-variant ci/android_emulator_debug_x64

# Run host-side unit tests (including JNI, external view embedder, delegate unittests)
python3 engine/src/flutter/testing/run_tests.py --type engine

# Run Java Robolectric tests
python3 engine/src/flutter/testing/run_tests.py --type java

# Run dependency ratchet check
python3 engine/src/flutter/tools/android_embedder_deps.py --check
```
