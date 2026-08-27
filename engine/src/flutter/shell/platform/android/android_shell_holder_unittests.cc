// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "flutter/shell/platform/android/android_shell_holder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "shell/platform/android/jni/platform_view_android_jni.h"

namespace flutter {
namespace testing {
namespace {
class MockPlatformViewAndroidJNI : public PlatformViewAndroidJNI {
 public:
  MOCK_METHOD(void,
              FlutterViewHandlePlatformMessage,
              (std::unique_ptr<flutter::PlatformMessage> message,
               int responseId),
              (override));
  MOCK_METHOD(void,
              FlutterViewHandlePlatformMessageResponse,
              (int responseId, std::unique_ptr<fml::Mapping> data),
              (override));
  MOCK_METHOD(void,
              FlutterViewUpdateSemantics,
              (std::vector<uint8_t> buffer,
               std::vector<std::string> strings,
               std::vector<std::vector<uint8_t>> string_attribute_args),
              (override));
  MOCK_METHOD(void,
              FlutterViewSetSemanticsTreeEnabled,
              (bool enabled),
              (override));
  MOCK_METHOD(void,
              FlutterViewSetApplicationLocale,
              (const std::string locale),
              (override));
  MOCK_METHOD(void,
              FlutterViewUpdateCustomAccessibilityActions,
              (std::vector<uint8_t> actions_buffer,
               std::vector<std::string> strings),
              (override));
  MOCK_METHOD(void, FlutterViewOnFirstFrame, (), (override));
  MOCK_METHOD(void, FlutterViewOnPreEngineRestart, (), (override));
  MOCK_METHOD(void,
              SurfaceTextureAttachToGLContext,
              (JavaLocalRef surface_texture, int textureId),
              (override));
  MOCK_METHOD(bool,
              SurfaceTextureShouldUpdate,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(void,
              SurfaceTextureUpdateTexImage,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(SkM44,
              SurfaceTextureGetTransformMatrix,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(void,
              SurfaceTextureDetachFromGLContext,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(JavaLocalRef,
              ImageProducerTextureEntryAcquireLatestImage,
              (JavaLocalRef image_texture_entry),
              (override));
  MOCK_METHOD(JavaLocalRef,
              ImageGetHardwareBuffer,
              (JavaLocalRef image),
              (override));
  MOCK_METHOD(void, ImageClose, (JavaLocalRef image), (override));
  MOCK_METHOD(void,
              HardwareBufferClose,
              (JavaLocalRef hardware_buffer),
              (override));
  MOCK_METHOD(void,
              FlutterViewOnDisplayPlatformView,
              (int view_id,
               int x,
               int y,
               int width,
               int height,
               int viewWidth,
               int viewHeight,
               MutatorsStack mutators_stack),
              (override));
  MOCK_METHOD(void,
              FlutterViewDisplayOverlaySurface,
              (int surface_id, int x, int y, int width, int height),
              (override));
  MOCK_METHOD(void, FlutterViewBeginFrame, (), (override));
  MOCK_METHOD(void, FlutterViewEndFrame, (), (override));
  MOCK_METHOD(std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>,
              FlutterViewCreateOverlaySurface,
              (),
              (override));
  MOCK_METHOD(void, FlutterViewDestroyOverlaySurfaces, (), (override));
  MOCK_METHOD(ASurfaceTransaction*, createTransaction, (), (override));
  MOCK_METHOD(void, swapTransaction, (), (override));
  MOCK_METHOD(void, applyTransaction, (), (override));
  MOCK_METHOD(void, destroyOverlaySurface2, (), (override));
  MOCK_METHOD(std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>,
              createOverlaySurface2,
              (),
              (override));
  MOCK_METHOD(void,
              onDisplayPlatformView2,
              (int32_t view_id,
               int32_t x,
               int32_t y,
               int32_t width,
               int32_t height,
               int32_t viewWidth,
               int32_t viewHeight,
               MutatorsStack mutators_stack),
              (override));
  MOCK_METHOD(void, hidePlatformView2, (int32_t view_id), (override));
  MOCK_METHOD(void, onEndFrame2, (), (override));
  MOCK_METHOD(void, showOverlaySurface2, (), (override));
  MOCK_METHOD(void, hideOverlaySurface2, (), (override));
  MOCK_METHOD(std::unique_ptr<std::vector<std::string>>,
              FlutterViewComputePlatformResolvedLocale,
              (std::vector<std::string> supported_locales_data),
              (override));
  MOCK_METHOD(double, GetDisplayRefreshRate, (), (override));
  MOCK_METHOD(double, GetDisplayWidth, (), (override));
  MOCK_METHOD(double, GetDisplayHeight, (), (override));
  MOCK_METHOD(double, GetDisplayDensity, (), (override));
  MOCK_METHOD(bool,
              RequestDartDeferredLibrary,
              (int loading_unit_id),
              (override));
  MOCK_METHOD(double,
              FlutterViewGetScaledFontSize,
              (double font_size, int configuration_id),
              (const, override));
  MOCK_METHOD(void,
              MaybeResizeSurfaceView,
              (int32_t width, int32_t height),
              (const, override));
};

class MockPlatformMessageResponse : public PlatformMessageResponse {
 public:
  static fml::RefPtr<MockPlatformMessageResponse> Create() {
    return fml::AdoptRef(new MockPlatformMessageResponse());
  }
  MOCK_METHOD(void, Complete, (std::unique_ptr<fml::Mapping> data), (override));
  MOCK_METHOD(void, CompleteEmpty, (), (override));
};
}  // namespace

TEST(AndroidShellHolder, Create) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());
  EXPECT_NE(holder->GetPlatformView().get(), nullptr);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);
}

TEST(AndroidShellHolder, HandlePlatformMessage) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());
  EXPECT_NE(holder->GetPlatformView().get(), nullptr);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);
  EXPECT_TRUE(holder->GetPlatformMessageHandler());
  size_t data_size = 4;
  fml::MallocMapping bytes =
      fml::MallocMapping(static_cast<uint8_t*>(malloc(data_size)), data_size);
  fml::RefPtr<MockPlatformMessageResponse> response =
      MockPlatformMessageResponse::Create();
  int response_id = 1;
  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, response_id));
  EXPECT_CALL(*response, CompleteEmpty());
  auto message = std::make_unique<PlatformMessage>(
      /*channel=*/"foo", /*data=*/std::move(bytes), /*response=*/response);
  holder->GetPlatformMessageHandler()->HandlePlatformMessage(
      std::move(message));
  holder->GetPlatformMessageHandler()
      ->InvokePlatformMessageEmptyResponseCallback(response_id);
}

TEST(AndroidShellHolder, CreateWithMergedPlatformAndUIThread) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  EXPECT_EQ(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

TEST(AndroidShellHolder, CreateWithUnMergedPlatformAndUIThread) {
  Settings settings;
  settings.merged_platform_ui_thread =
      Settings::MergedPlatformUIThread::kDisabled;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  EXPECT_NE(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

TEST(AndroidShellHolder, RapidSurfaceRecreateLifecycle) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());
  EXPECT_NE(holder->GetPlatformView().get(), nullptr);

  // Simulate rapid onPause / onResume lifecycle events (creating and destroying
  // surfaces).
  for (int i = 0; i < 50; ++i) {
    auto window = fml::MakeRefCounted<AndroidNativeWindow>(
        nullptr, /*is_fake_window=*/true);
    holder->GetPlatformView()->NotifyCreated(window);
    holder->GetPlatformView()->NotifyDestroyed();
  }

  // Ensure engine is still valid and can accept a new surface after churn.
  auto final_window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(final_window);
  EXPECT_TRUE(holder->IsValid());
  holder->GetPlatformView()->NotifyDestroyed();
}

TEST(AndroidShellHolder, ConcurrentPlatformMessageHandling) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  constexpr int kNumThreads = 8;
  constexpr int kMessagesPerThread = 25;
  std::atomic<int> completed_messages{0};
  std::mutex jni_calls_mutex;
  std::vector<int> received_response_ids;

  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, ::testing::_))
      .WillRepeatedly([&](std::unique_ptr<flutter::PlatformMessage> message,
                          int response_id) {
        std::lock_guard<std::mutex> lock(jni_calls_mutex);
        received_response_ids.push_back(response_id);
      });

  std::vector<std::thread> workers;
  for (int t = 0; t < kNumThreads; ++t) {
    workers.emplace_back([&]() {
      for (int m = 0; m < kMessagesPerThread; ++m) {
        size_t data_size = 8;
        fml::MallocMapping bytes = fml::MallocMapping(
            static_cast<uint8_t*>(malloc(data_size)), data_size);
        fml::RefPtr<MockPlatformMessageResponse> response =
            MockPlatformMessageResponse::Create();
        EXPECT_CALL(*response, CompleteEmpty()).WillOnce([&]() {
          completed_messages.fetch_add(1);
        });

        auto message = std::make_unique<PlatformMessage>(
            /*channel=*/"benchmark_channel", /*data=*/std::move(bytes),
            /*response=*/response);
        holder->GetPlatformMessageHandler()->HandlePlatformMessage(
            std::move(message));
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_EQ(received_response_ids.size(),
            static_cast<size_t>(kNumThreads * kMessagesPerThread));

  // Concurrently reply to all received response IDs from separate responder
  // threads.
  std::vector<std::thread> responders;
  const size_t chunk_size = received_response_ids.size() / kNumThreads;
  for (int t = 0; t < kNumThreads; ++t) {
    responders.emplace_back([&, t]() {
      size_t start = t * chunk_size;
      size_t end = (t == kNumThreads - 1) ? received_response_ids.size()
                                          : start + chunk_size;
      for (size_t i = start; i < end; ++i) {
        holder->GetPlatformMessageHandler()
            ->InvokePlatformMessageEmptyResponseCallback(
                received_response_ids[i]);
      }
    });
  }

  for (auto& responder : responders) {
    responder.join();
  }

  EXPECT_EQ(completed_messages.load(), kNumThreads * kMessagesPerThread);
  holder->GetPlatformView()->NotifyDestroyed();
}

TEST(AndroidShellHolder, InterleavedConcurrentPlatformMessageHandling) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  constexpr int kNumProducers = 4;
  constexpr int kNumConsumers = 4;
  constexpr int kMessagesPerProducer = 30;
  constexpr int kTotalMessages = kNumProducers * kMessagesPerProducer;

  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::vector<int> pending_ids;
  std::atomic<bool> producers_done{false};
  std::atomic<int> completed_count{0};

  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, ::testing::_))
      .WillRepeatedly([&](std::unique_ptr<flutter::PlatformMessage> message,
                          int response_id) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        pending_ids.push_back(response_id);
        queue_cv.notify_one();
      });

  // Start consumer threads first to process responses concurrently with message
  // arrival.
  std::vector<std::thread> consumers;
  for (int c = 0; c < kNumConsumers; ++c) {
    consumers.emplace_back([&]() {
      while (true) {
        int response_id = 0;
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          queue_cv.wait(lock, [&]() {
            return !pending_ids.empty() || producers_done.load();
          });
          if (!pending_ids.empty()) {
            response_id = pending_ids.back();
            pending_ids.pop_back();
          } else if (producers_done.load()) {
            break;
          }
        }
        if (response_id > 0) {
          holder->GetPlatformMessageHandler()
              ->InvokePlatformMessageEmptyResponseCallback(response_id);
        }
      }
    });
  }

  // Start producer threads.
  std::vector<std::thread> producers;
  for (int p = 0; p < kNumProducers; ++p) {
    producers.emplace_back([&]() {
      for (int m = 0; m < kMessagesPerProducer; ++m) {
        const uint8_t payload[] = "ping";
        auto bytes = fml::MallocMapping::Copy(payload, sizeof(payload));
        fml::RefPtr<MockPlatformMessageResponse> response =
            MockPlatformMessageResponse::Create();
        EXPECT_CALL(*response, CompleteEmpty()).WillOnce([&]() {
          completed_count.fetch_add(1);
        });

        auto message = std::make_unique<PlatformMessage>(
            /*channel=*/"interleaved_channel", /*data=*/std::move(bytes),
            /*response=*/response);
        holder->GetPlatformMessageHandler()->HandlePlatformMessage(
            std::move(message));
      }
    });
  }

  for (auto& producer : producers) {
    producer.join();
  }

  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    producers_done.store(true);
    queue_cv.notify_all();
  }

  for (auto& consumer : consumers) {
    consumer.join();
  }

  EXPECT_EQ(completed_count.load(), kTotalMessages);
  holder->GetPlatformView()->NotifyDestroyed();
}

TEST(AndroidShellHolder, HandleOneWayPlatformMessage) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  int captured_response_id = -1;
  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, ::testing::_))
      .WillOnce([&](std::unique_ptr<flutter::PlatformMessage> message,
                    int response_id) {
        captured_response_id = response_id;
        EXPECT_EQ(message->channel(), "oneway_channel");
        EXPECT_EQ(message->response().get(), nullptr);
      });

  const uint8_t payload[] = "fire_and_forget";
  auto bytes = fml::MallocMapping::Copy(payload, sizeof(payload));
  auto message = std::make_unique<PlatformMessage>(
      /*channel=*/"oneway_channel", /*data=*/std::move(bytes),
      /*response=*/nullptr);

  holder->GetPlatformMessageHandler()->HandlePlatformMessage(
      std::move(message));

  EXPECT_GT(captured_response_id, 0);

  // Calling response callback on a one-way message response ID should be a safe
  // no-op.
  holder->GetPlatformMessageHandler()
      ->InvokePlatformMessageEmptyResponseCallback(captured_response_id);

  holder->GetPlatformView()->NotifyDestroyed();
}

TEST(AndroidShellHolder, HandleUnknownResponseIdGracefully) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());

  // Unknown response ID should be safely ignored without asserts or crashes.
  constexpr int kUnknownResponseId = 999999;
  holder->GetPlatformMessageHandler()
      ->InvokePlatformMessageEmptyResponseCallback(kUnknownResponseId);

  const uint8_t dummy_data[] = {1, 2, 3, 4};
  auto mapping = std::make_unique<fml::MallocMapping>(
      fml::MallocMapping::Copy(dummy_data, sizeof(dummy_data)));
  holder->GetPlatformMessageHandler()->InvokePlatformMessageResponseCallback(
      kUnknownResponseId, std::move(mapping));
}

TEST(AndroidShellHolder, HandleZeroBytePayloadPlatformMessage) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  int response_id_out = 0;
  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, ::testing::_))
      .WillOnce([&](std::unique_ptr<flutter::PlatformMessage> message,
                    int response_id) {
        response_id_out = response_id;
        EXPECT_EQ(message->data().GetSize(), 0u);
      });

  fml::RefPtr<MockPlatformMessageResponse> response =
      MockPlatformMessageResponse::Create();
  EXPECT_CALL(*response, CompleteEmpty()).Times(1);

  fml::MallocMapping empty_bytes = fml::MallocMapping(nullptr, 0);
  auto message = std::make_unique<PlatformMessage>(
      /*channel=*/"empty_channel", /*data=*/std::move(empty_bytes),
      /*response=*/response);

  holder->GetPlatformMessageHandler()->HandlePlatformMessage(
      std::move(message));
  EXPECT_GT(response_id_out, 0);

  holder->GetPlatformMessageHandler()
      ->InvokePlatformMessageEmptyResponseCallback(response_id_out);

  holder->GetPlatformView()->NotifyDestroyed();
}

struct MessageQueueTestParams {
  int num_messages;
  bool is_empty_response;
};

class AndroidShellHolderMessageQueueTest
    : public ::testing::TestWithParam<MessageQueueTestParams> {};

TEST_P(AndroidShellHolderMessageQueueTest,
       HandlePlatformMessagesWithResponses) {
  const auto& params = GetParam();
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  std::vector<int> response_ids;
  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, ::testing::_))
      .WillRepeatedly(
          [&](std::unique_ptr<flutter::PlatformMessage> message,
              int response_id) { response_ids.push_back(response_id); });

  std::atomic<int> completed{0};
  constexpr size_t kReplyPayloadSize = 8;
  const uint8_t reply_payload[kReplyPayloadSize] = {1, 2, 3, 4, 5, 6, 7, 8};

  for (int i = 0; i < params.num_messages; ++i) {
    const uint8_t req_payload[] = "request_data_16b";
    auto bytes = fml::MallocMapping::Copy(req_payload, sizeof(req_payload));
    fml::RefPtr<MockPlatformMessageResponse> response =
        MockPlatformMessageResponse::Create();

    if (params.is_empty_response) {
      EXPECT_CALL(*response, CompleteEmpty()).WillOnce([&]() {
        completed.fetch_add(1);
      });
    } else {
      EXPECT_CALL(*response, Complete(::testing::_))
          .WillOnce([&](std::unique_ptr<fml::Mapping> data) {
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->GetSize(), kReplyPayloadSize);
            EXPECT_EQ(
                memcmp(data->GetMapping(), reply_payload, kReplyPayloadSize),
                0);
            completed.fetch_add(1);
          });
    }

    auto message = std::make_unique<PlatformMessage>(
        /*channel=*/"test_channel", /*data=*/std::move(bytes),
        /*response=*/response);
    holder->GetPlatformMessageHandler()->HandlePlatformMessage(
        std::move(message));
  }

  EXPECT_EQ(response_ids.size(), static_cast<size_t>(params.num_messages));

  for (int response_id : response_ids) {
    if (params.is_empty_response) {
      holder->GetPlatformMessageHandler()
          ->InvokePlatformMessageEmptyResponseCallback(response_id);
    } else {
      auto reply_bytes = std::make_unique<fml::MallocMapping>(
          fml::MallocMapping::Copy(reply_payload, kReplyPayloadSize));
      holder->GetPlatformMessageHandler()
          ->InvokePlatformMessageResponseCallback(response_id,
                                                  std::move(reply_bytes));
    }
  }

  EXPECT_EQ(completed.load(), params.num_messages);
  holder->GetPlatformView()->NotifyDestroyed();
}

std::string PrintMessageQueueTestParams(
    const ::testing::TestParamInfo<MessageQueueTestParams>& info) {
  return "Count" + std::to_string(info.param.num_messages) + "_" +
         (info.param.is_empty_response ? "EmptyResponse" : "DataResponse");
}

INSTANTIATE_TEST_SUITE_P(
    VaryingMessageCountsAndPayloads,
    AndroidShellHolderMessageQueueTest,
    ::testing::Values(MessageQueueTestParams{/*num_messages=*/1,
                                             /*is_empty_response=*/true},
                      MessageQueueTestParams{/*num_messages=*/1,
                                             /*is_empty_response=*/false},
                      MessageQueueTestParams{/*num_messages=*/10,
                                             /*is_empty_response=*/true},
                      MessageQueueTestParams{/*num_messages=*/10,
                                             /*is_empty_response=*/false},
                      MessageQueueTestParams{/*num_messages=*/50,
                                             /*is_empty_response=*/true},
                      MessageQueueTestParams{/*num_messages=*/50,
                                             /*is_empty_response=*/false}),
    PrintMessageQueueTestParams);

}  // namespace testing
}  // namespace flutter
