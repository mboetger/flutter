---
name: flutter-testing
description: >-
  Guides on how to run, write, and configure tests in the Flutter (Framework and Engine) repositories.
  Similar to a testing cheatsheet for Flutter contributors.
  Use when needing to run unit tests, integration tests, or engine tests locally, or when adding a new test to the CI (LUCI) configuration.
  Don't use for general Dart package testing outside the Flutter repositories.
---

# Flutter Testing Guide (Monorepo)

This skill helps you run, write, and configure tests in the Flutter monorepo, which contains both the **Framework** and the **Engine** code.

---

## Repository Structure (Monorepo)

The `flutter/flutter` and `flutter/engine` repositories are merged into a single monorepo:
- **Repository Root**: The base directory of the cloned repository.
- **Framework Code**: Located at the root (e.g., `packages/flutter`, `examples/`).
- **Engine Code**: Located under the **`engine/src/flutter`** directory.
- **Engine Build Output**: Located under **`engine/src/out`**.

---

## 1. Framework Tests (`packages/flutter`, `examples/`)

### Unit Tests
Dart unit tests are located in the `test/` subdirectory of the package under test (written using the `flutter_test` package).

*   **Run all tests in a package**:
    Navigate to the package directory and run `flutter test`.
    ```bash
    cd examples/hello_world
    flutter test
    ```
*   **Run a specific test file**:
    ```bash
    flutter test lib/my_app_test.dart
    ```
*   **Simulate CI tests locally** (run from repository root):
    ```bash
    # Run all tests
    dart dev/bots/test.dart
    # Run static analysis
    dart --enable-asserts dev/bots/analyze.dart
    ```

### Golden File Tests
Golden file tests compare the rendered pixels of a widget against a master baseline image using **Skia Gold** (`flutter-gold.skia.org`).

*   **Writing a Golden Test**:
    1.  Add the `reduced-test-set` tag at the very top of your test file (so it runs on Mac/Windows CI pre-submit):
        ```dart
        @Tags(<String>['reduced-test-set'])
        ```
    2.  Wrap the widget subtree you want to capture in a **`RepaintBoundary`** (otherwise it captures the full 2400x1800 viewport):
        ```dart
        await tester.pumpWidget(
          const RepaintBoundary(
            child: MyWidget(),
          ),
        );
        ```
    3.  Assert using `matchesGoldenFile`. Use the format `test_filename.subtest.png`:
        ```dart
        await expectLater(
          find.byType(RepaintBoundary),
          matchesGoldenFile('my_widget_test.basic.png'),
        );
        ```
*   **Running/Updating Goldens Locally**:
    Navigate to the package (e.g., `packages/flutter`) and run with the `--update-goldens` flag:
    ```bash
    flutter test --update-goldens test/widgets/my_widget_test.dart
    ```
    This generates or updates the baseline images locally under `bin/cache/pkg/skia_goldens/packages/flutter/test/`.

*   **CI Configuration for Goldens**:
    Any task running golden tests in `.ci.yaml` must include the `goldctl` dependency:
    ```yaml
      properties:
        dependencies: >-
          [
            {"dependency": "goldctl", "version": "git_revision:2387d6fff449587eecbb7e45b2692ca0710b63b9"}
          ]
    ```
    *(Note: Copy the exact git revision from an existing task).*

### Integration Tests (`integration_test` package)
The `integration_test` package enables self-driving testing of Flutter code on devices and emulators.
*   **Run using `flutter drive`**:
    ```bash
    flutter drive \
      --driver=test_driver/integration_test.dart \
      --target=integration_test/foo_test.dart
    ```
*   **Run on Web**:
    ```bash
    flutter drive \
      --driver=test_driver/integration_test.dart \
      --target=integration_test/foo_test.dart \
      -d web-server
    ```
*   **Run on Android (Native Instrumentation)**:
    ```bash
    ./gradlew app:connectedAndroidTest -Ptarget=`pwd`/../integration_test/foo_test.dart
    ```
*   **Run on iOS (Native XCTest)**:
    ```bash
    flutter build ios --config-only integration_test/foo_test.dart
    # Then run Product > Test in Xcode
    ```

### Device Lab (End-to-End) Tests
Device Lab tests are automated integration tests run on physical devices, simulators, or emulators, managed by the Flutter team's CI tooling.
- **Tooling**: `dev/devicelab`
- **Tests**: `dev/integration_tests`

*   **How to run locally**:
    1. Connect device/emulator.
    2. Set locale: `export LANG=en_US.UTF-8`.
    3. Navigate to `dev/devicelab`.
    4. Run: `../../bin/dart bin/run.dart -t [task_name]`.
*   **Legacy Driver Tests** (in `dev/integration_tests`):
    ```bash
    cd dev/integration_tests/flutter_gallery
    flutter drive -t lib/gallery/home.dart --driver test_driver/transitions_perf.dart
    ```

---

## 2. Where to Place New Integration Tests

When adding new integration tests (especially Android-focused tests that use `flutter drive`), you have two primary locations in the monorepo:

### Option A: Add to the existing `integration_ui` package (Preferred for general UI/Framework tests)
The **`dev/integration_tests/ui`** directory is a shared testbed designed for multiple integration tests. It avoids the overhead of creating a new Flutter project for every test.

1.  **Create the App UI**: Add a new Dart file in `dev/integration_tests/ui/lib/` (e.g., `lib/my_android_test.dart`) containing the `main()` entry point and the widgets you want to test.
2.  **Create the Driver**: Add a matching driver file in `dev/integration_tests/ui/test_driver/` (e.g., `test_driver/my_android_test_test.dart`) that uses `package:flutter_driver` to control the app.
3.  **Run it locally**:
    ```bash
    cd dev/integration_tests/ui
    flutter drive -t lib/my_android_test.dart --driver test_driver/my_android_test_test.dart
    ```

### Option B: Create a new integration test app (For complex or isolated scenarios)
If your test requires a highly specific Android configuration (e.g., custom Android Manifest, specific Gradle dependencies, or unique platform channels) that could conflict with other tests, you should create a new project directory under `dev/integration_tests/`.

1.  **Create a new directory** under `dev/integration_tests/` (e.g., `dev/integration_tests/my_custom_android_test`).
2.  **Set up a standard Flutter project** structure:
    ```text
    dev/integration_tests/my_custom_android_test/
      ├── android/              # Android specific host configuration
      ├── lib/
      │    └── main.dart        # The app to run
      ├── test_driver/
      │    └── main_test.dart   # The driver script
      └── pubspec.yaml
    ```
3.  **Run it locally**:
    ```bash
    cd dev/integration_tests/my_custom_android_test
    flutter drive -t lib/main.dart --driver test_driver/main_test.dart
    ```

### Option C: Using the modern `integration_test` package within `integration_ui`
If you prefer using the modern `integration_test` package (which runs `testWidgets` on the device instead of using a separate driver script):
1.  Add your test to `dev/integration_tests/ui/integration_test/my_test.dart`.
2.  Ensure `test_driver/integration_test.dart` exists (it should call `integrationDriver()`).
3.  Run it using `flutter drive`:
    ```bash
    cd dev/integration_tests/ui
    flutter drive \
      --driver=test_driver/integration_test.dart \
      --target=integration_test/my_test.dart
    ```

---

## 3. Enabling New Tests in Continuous Integration (CI)

To ensure a newly created integration test runs automatically on Flutter's CI (LUCI), you must register it as a **DeviceLab** or **Shard** task.

### Step 1: Create the CI Task File
For DeviceLab tests, you must create a task entry point that the runner script can execute.
1.  Create a file under `dev/devicelab/bin/tasks/<your_test_name>.dart`.
2.  Implement it to delegate to your actual test code (usually placed in `dev/devicelab/lib/tasks/`):
    ```dart
    import 'package:flutter_devicelab/framework/framework.dart';
    import 'package:flutter_devicelab/tasks/my_test_impl.dart'; // Your implementation

    Future<void> main() async {
      await task(myTestImpl());
    }
    ```

### Step 2: Configure the Target in `.ci.yaml`
You must register the task in the top-level `.ci.yaml` file at the repository root.

1.  Open `.ci.yaml` and add a new target entry under `targets:`.
2.  Set **`bringup: true`** initially. This ensures the test runs in a "non-blocking" staging pool.
3.  Define the platform, recipe, and properties:
    ```yaml
    - name: Linux_android_emu my_new_test        # Platform + Test Name
      recipe: devicelab/devicelab_drone          # DeviceLab runner recipe
      bringup: true                              # Set to true for initial release
      timeout: 60
      properties:
        tags: >
          ["framework", "hostonly", "linux"]
        task_name: my_new_test                   # Must match the task filename (without .dart)
        dependencies: >-
          [
            {"dependency": "android_sdk", "version": "version:36v9unmodified"},
            {"dependency": "open_jdk", "version": "version:21"}
          ]
    ```

### Step 3: Assign Ownership in `TESTOWNERS`
Every test in the repository must have designated owners to triage failures.
1.  Open `TESTOWNERS` at the repository root.
2.  Add a line mapping your task file to GitHub handles of the owners and the responsible team:
    ```text
    /dev/devicelab/bin/tasks/my_new_test.dart @your_github_handle @flutter/android
    ```

### Step 4: Graduate the Test to "Blocking"
1.  Monitor the test on the Flutter Build Dashboard after landing the PR.
2.  Once the test has run successfully in post-submit CI multiple times and proven to be stable (non-flaky), submit a follow-up PR to **remove `bringup: true`** from `.ci.yaml`.

---

## 4. Engine Tests (`engine/src/flutter`)

All engine-specific tests must be run from the engine source directory: **`engine/src/flutter`**.

### C++ Core Engine Tests
These tests are co-located with their source files (e.g., `*_unittest.cc`).

*   **Run via Python runner** (from `engine/src/flutter`):
    ```bash
    cd engine/src/flutter
    testing/run_tests.py --type=engine
    ```
    *To use a different variant (e.g., arm64 Mac):*
    ```bash
    testing/run_tests.py --type=engine --variant=host_debug_unopt_arm64
    ```
*   **Run GTest executables directly** (from `engine/src/flutter`):
    ```bash
    cd engine/src/flutter
    ../out/host_debug_unopt/shell_unittests
    ```
    *Filter specific tests:*
    ```bash
    ../out/host_debug_unopt/shell_unittests --gtest_filter="ShellTest.WaitForFirstFrame"
    ```

### Android Java Embedder Tests (JUnit / Robolectric)
Located at `engine/src/flutter/shell/platform/android/test`.

*   **Run Android JUnit/Robolectric tests** (from `engine/src/flutter`):
    ```bash
    cd engine/src/flutter
    testing/run_tests.py --type=java
    ```
    *Note: Requires `$JAVA_HOME` to be set to JDK v8.*

### iOS Objective-C Embedder Tests (XCTest)
Unit tests are co-located in `engine/src/flutter/shell/platform/darwin/ios`.

*   **Run iOS XCTests** (from `engine/src/flutter`):
    ```bash
    cd engine/src/flutter
    testing/run_tests.py --type=objc
    ```

### Dart `dart:ui` Tests
Located at `engine/src/flutter/testing/dart`.

*   **Run dart:ui tests** (from `engine/src/flutter`):
    ```bash
    cd engine/src/flutter
    testing/run_tests.py --type=dart
    ```

### Running Framework Tests with Local Engine Build
To run Framework tests against your locally compiled engine within the monorepo:
1.  Compile the engine (e.g., to `engine/src/out/host_debug_unopt`).
2.  Run the following command from the **monorepo root**:
    ```bash
    flutter test \
      --local-engine-src-path=engine/src \
      --local-engine=host_debug_unopt \
      --local-engine-host=host_debug_unopt \
      packages/flutter
    ```

### Web Engine Tests
Web tests are run via the `felt` command.
- Refer to `engine/src/flutter/lib/web_ui/README.md` for more details.
