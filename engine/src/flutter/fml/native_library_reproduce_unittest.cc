// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <iostream>

#include "flutter/fml/native_library.h"
#include "flutter/testing/testing.h"

#if defined(FML_OS_LINUX) || defined(FML_OS_ANDROID)
extern "C" void* __real_dlsym(void* handle, const char* symbol);

static bool g_intercept_dlsym = false;
static bool g_rtld_default_used = false;

extern "C" void* __wrap_dlsym(void* handle, const char* symbol) {
  if (g_intercept_dlsym && handle == RTLD_DEFAULT) {
    g_rtld_default_used = true;
    std::cerr << "[REPRODUCE] Intercepted dlsym(RTLD_DEFAULT, " << symbol << ")." << std::endl;
  }
  return __real_dlsym(handle, symbol);
}
#endif

namespace fml {
namespace testing {

TEST(NativeLibraryReproduce, CreateForCurrentProcessDoesNotUseRtldDefault) {
#if defined(FML_OS_LINUX) || defined(FML_OS_ANDROID)
  // Enable interception of dlsym(RTLD_DEFAULT)
  g_intercept_dlsym = true;
  g_rtld_default_used = false;

  std::cout << "[REPRODUCE] Calling CreateForCurrentProcess..." << std::endl;
  auto loaded_process = fml::NativeLibrary::CreateForCurrentProcess();
  ASSERT_TRUE(loaded_process);

  std::cout << "[REPRODUCE] Resolving symbol..." << std::endl;
  // Trigger symbol resolution which calls dlsym under the hood.
  // ResolveSymbol is public in fml::NativeLibrary.
  const uint8_t* symbol = loaded_process->ResolveSymbol("malloc");
  EXPECT_NE(symbol, nullptr) << "Failed to resolve 'malloc' in current process.";

  g_intercept_dlsym = false;

#if defined(FML_OS_ANDROID)
  // On Android, we must NOT use RTLD_DEFAULT to avoid Houdini crashes.
  EXPECT_FALSE(g_rtld_default_used)
      << "dlsym was called with RTLD_DEFAULT, which causes crashes on Houdini.";
#elif defined(FML_OS_LINUX)
  // On Linux, we should continue to use RTLD_DEFAULT.
  EXPECT_TRUE(g_rtld_default_used)
      << "Expected dlsym to be called with RTLD_DEFAULT on Linux.";
#endif
#else
  GTEST_SKIP() << "Reproduction test is only supported on Linux/Android due to linker wrapping.";
#endif
}

}  // namespace testing
}  // namespace fml
