# Plan for adding Engine Spawning support to the Embedder API

## Objective
Remove internal engine dependencies from the Android embedder by adding a `FlutterEngineSpawn` API to `embedder.h`. This will allow the Android embedder to manage multiple engine instances (isolates) that share resources, fulfilling the requirements of `FlutterEngineGroup`.

## Background
The Android embedder currently uses `Shell::Spawn` to create additional engine instances that share the same Dart VM and potentially other resources. The public `embedder.h` API lacks a corresponding function, forcing the Android embedder to depend on internal engine headers (`shell/common/shell.h`).

## Proposed API Changes

### `embedder.h`

Add `FlutterEngineSpawn` to the public API:

```c
/**
 * Spawns a new engine instance from an existing one.
 * 
 * The new instance will share the same Dart VM and Isolate Group as the 
 * parent engine, resulting in faster startup and reduced memory footprint.
 * 
 * @param[in]  engine      An existing engine instance to spawn from.
 * @param[in]  config      The renderer configuration for the new engine.
 * @param[in]  args        The project arguments for the new engine.
 * @param[in]  user_data   The user data for the new engine.
 * @param[out] engine_out  The handle to the newly created engine instance.
 * 
 * @return kSuccess if the engine was successfully spawned.
 */
FLUTTER_EXPORT
FlutterEngineResult FlutterEngineSpawn(
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    const FlutterRendererConfig* config,
    const FlutterProjectArgs* args,
    void* user_data,
    FLUTTER_API_SYMBOL(FlutterEngine)* engine_out);
```

### `FlutterEngineProcTable`

Add the new function to the procedure table:

```c
typedef struct {
  // ... existing members ...
  FlutterEngineResult (*Spawn)(
      FLUTTER_API_SYMBOL(FlutterEngine) /* engine */,
      const FlutterRendererConfig* /* config */,
      const FlutterProjectArgs* /* args */,
      void* /* user_data */,
      FLUTTER_API_SYMBOL(FlutterEngine)* /* engine_out */);
} FlutterEngineProcTable;
```

## Implementation Details

### 1. `EmbedderEngine` Refactoring
The `EmbedderEngine` class (internal to the embedder platform) needs to support spawning.

-   **New Constructor/Method**: Add a method to `EmbedderEngine` that takes a `RunConfiguration` and callbacks, and calls `shell_->Spawn(...)`.
-   **TaskRunner Sharing**: Decide whether the spawned engine should share the `EmbedderThreadHost` or create a new one. Typically, `AndroidShellHolder` shares the `ThreadHost`. The API should allow the embedder to provide the same `custom_task_runners` to achieve this.

### 2. `embedder.cc` Implementation
Implement `FlutterEngineSpawn` by:
1.  Validating input arguments (similar to `FlutterEngineInitialize`).
2.  Resolving the parent `EmbedderEngine` from the provided handle.
3.  Inferring the platform view and rasterizer creation callbacks from the `FlutterRendererConfig`.
4.  Creating a `RunConfiguration` from the `FlutterProjectArgs`.
5.  Invoking the internal spawning logic which ultimately calls `Shell::Spawn`.
6.  Wrapping the new `Shell` in a new `EmbedderEngine` and returning it.

### 3. `AndroidShellHolder` Update
Once the API is available:
1.  Update `AndroidShellHolder` to hold a `FLUTTER_API_SYMBOL(FlutterEngine)` handle instead of a `std::unique_ptr<Shell>`.
2.  Use `FlutterEngineInitialize` and `FlutterEngineRunInitialized` in the constructor.
3.  Use `FlutterEngineSpawn` in the `Spawn` method.
4.  Remove `#include "flutter/shell/common/shell.h"`.

## Verification Plan
1.  **Unit Tests**: Add a test in `embedder_unittests.cc` that initializes an engine and then spawns a second one, verifying both can run simultaneously.
2.  **Android Integration**: Update `AndroidShellHolder` and ensure existing `FlutterEngineGroup` tests in the Android embedding still pass.
