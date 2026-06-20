// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.deferredcomponents;

import static junit.framework.TestCase.assertEquals;
import static junit.framework.TestCase.assertFalse;
import static junit.framework.TestCase.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetManager;
import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.tasks.Task;
import com.google.android.play.core.tasks.OnSuccessListener;
import io.flutter.embedding.engine.systemchannels.DeferredComponentChannel;
import java.lang.reflect.Field;
import java.util.HashSet;
import java.util.Set;

import io.flutter.embedding.engine.FlutterEngineFlags;
import io.flutter.embedding.engine.FlutterJNI;
import java.io.File;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class PlayStoreDeferredComponentManagerTest {
  private class TestFlutterJNI extends FlutterJNI {
    public int loadDartDeferredLibraryCalled = 0;
    public int updateAssetManagerCalled = 0;
    public int deferredComponentInstallFailureCalled = 0;
    public String[] searchPaths;
    public int loadingUnitId;
    public AssetManager assetManager;
    public String assetBundlePath;

    public TestFlutterJNI() {}

    @Override
    public void loadDartDeferredLibrary(int loadingUnitId, @NonNull String[] searchPaths) {
      loadDartDeferredLibraryCalled++;
      this.searchPaths = searchPaths;
      this.loadingUnitId = loadingUnitId;
    }

    @Override
    public void updateJavaAssetManager(
        @NonNull AssetManager assetManager, @NonNull String assetBundlePath) {
      updateAssetManagerCalled++;
      this.loadingUnitId = loadingUnitId;
      this.assetManager = assetManager;
      this.assetBundlePath = assetBundlePath;
    }

    @Override
    public void deferredComponentInstallFailure(
        int loadingUnitId, @NonNull String error, boolean isTransient) {
      deferredComponentInstallFailureCalled++;
    }
  }

  // Skips the download process to directly call the loadAssets and loadDartLibrary methods.
  private class TestPlayStoreDeferredComponentManager extends PlayStoreDeferredComponentManager {
    public TestPlayStoreDeferredComponentManager(Context context, FlutterJNI jni) {
      super(context, jni);
    }

    @Override
    public void installDeferredComponent(int loadingUnitId, String componentName) {
      // Override this to skip the online SplitInstallManager portion.
      loadAssets(loadingUnitId, componentName);
      loadDartLibrary(loadingUnitId, componentName);
    }
  }

  @SuppressWarnings("deprecation")
  // getApplicationInfo
  private Context createSpyContext(Bundle metadata) throws NameNotFoundException {
    Context spyContext = spy(ApplicationProvider.getApplicationContext());
    doReturn(spyContext).when(spyContext).createPackageContext(any(), anyInt());
    if (metadata == null) {
      metadata = new Bundle();
    }
    PackageManager packageManager = mock(PackageManager.class);
    ApplicationInfo applicationInfo = mock(ApplicationInfo.class);
    applicationInfo.metaData = metadata;
    applicationInfo.splitSourceDirs = new String[1];
    applicationInfo.splitSourceDirs[0] = "some.invalid.apk";
    when(packageManager.getApplicationInfo(any(String.class), any(int.class)))
        .thenReturn(applicationInfo);
    doReturn(packageManager).when(spyContext).getPackageManager();
    doReturn(applicationInfo).when(spyContext).getApplicationInfo();
    return spyContext;
  }

  @Test
  public void downloadCallsJNIFunctions() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Context spyContext = createSpyContext(null);
    doReturn(null).when(spyContext).getAssets();
    String soTestFilename = "libapp.so-123.part.so";
    String soTestPath = "test/path/" + soTestFilename;
    doReturn(new File(soTestPath)).when(spyContext).getFilesDir();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);
    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(123, "TestModuleName");
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 1);
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.searchPaths[0], soTestFilename);
    assertTrue(jni.searchPaths[1].endsWith(soTestPath));
    assertEquals(jni.searchPaths.length, 2);
    assertEquals(jni.loadingUnitId, 123);
    assertEquals(jni.assetBundlePath, "flutter_assets");
  }

  @Test
  public void downloadCallsJNIFunctionsWithFilenameFromManifest() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();

    Bundle bundle = new Bundle();
    bundle.putString(FlutterEngineFlags.AOT_SHARED_LIBRARY_NAME.metadataKey, "custom_name.so");
    bundle.putString(FlutterEngineFlags.FLUTTER_ASSETS_DIR.metadataKey, "custom_assets");

    Context spyContext = createSpyContext(bundle);
    doReturn(null).when(spyContext).getAssets();

    String soTestFilename = "custom_name.so-123.part.so";
    String soTestPath = "test/path/" + soTestFilename;
    doReturn(new File(soTestPath)).when(spyContext).getFilesDir();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);
    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(123, "TestModuleName");
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 1);
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.searchPaths[0], soTestFilename);
    assertTrue(jni.searchPaths[1].endsWith(soTestPath));
    assertEquals(jni.searchPaths.length, 2);
    assertEquals(jni.loadingUnitId, 123);
    assertEquals(jni.assetBundlePath, "custom_assets");
  }

  @Test
  public void downloadCallsJNIFunctionsWithSharedLibraryNameFromManifest()
      throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();

    Bundle bundle = new Bundle();
    bundle.putString(PlayStoreDeferredComponentManager.MAPPING_KEY, "123:module:custom_name.so");
    bundle.putString(FlutterEngineFlags.FLUTTER_ASSETS_DIR.metadataKey, "custom_assets");

    Context spyContext = createSpyContext(bundle);
    doReturn(null).when(spyContext).getAssets();

    String soTestFilename = "custom_name.so";
    String soTestPath = "test/path/" + soTestFilename;
    doReturn(new File(soTestPath)).when(spyContext).getFilesDir();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);
    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(123, "TestModuleName");
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 1);
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.searchPaths[0], soTestFilename);
    assertTrue(jni.searchPaths[1].endsWith(soTestPath));
    assertEquals(jni.searchPaths.length, 2);
    assertEquals(jni.loadingUnitId, 123);
    assertEquals(jni.assetBundlePath, "custom_assets");
  }

  @Test
  public void manifestMappingHandlesBaseModuleEmptyString() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();

    Bundle bundle = new Bundle();
    bundle.putString(
        PlayStoreDeferredComponentManager.MAPPING_KEY, "123:module:custom_name.so,3:,4:");
    bundle.putString(FlutterEngineFlags.FLUTTER_ASSETS_DIR.metadataKey, "custom_assets");

    Context spyContext = createSpyContext(bundle);
    doReturn(null).when(spyContext).getAssets();

    String soTestFilename = "libapp.so-3.part.so";
    String soTestPath = "test/path/" + soTestFilename;
    doReturn(new File(soTestPath)).when(spyContext).getFilesDir();
    PlayStoreDeferredComponentManager playStoreManager =
        new PlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);
    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(3, null);
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 0); // no assets to load for base
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.searchPaths[0], soTestFilename);
    assertTrue(jni.searchPaths[1].endsWith(soTestPath));
    assertEquals(jni.searchPaths.length, 2);
    assertEquals(jni.loadingUnitId, 3);
  }

  @Test
  public void searchPathsAddsApks() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Context spyContext = createSpyContext(null);
    doReturn(null).when(spyContext).getAssets();
    String apkTestPath = "test/path/TestModuleName_armeabi_v7a.apk";
    doReturn(new File(apkTestPath)).when(spyContext).getFilesDir();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);

    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(123, "TestModuleName");
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 1);
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.searchPaths[0], "libapp.so-123.part.so");
    assertTrue(jni.searchPaths[1].endsWith(apkTestPath + "!lib/armeabi-v7a/libapp.so-123.part.so"));
    assertEquals(jni.searchPaths.length, 2);
    assertEquals(jni.loadingUnitId, 123);
  }

  @Test
  public void searchPathsSearchesSplitConfig() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Context spyContext = createSpyContext(null);
    doReturn(null).when(spyContext).getAssets();
    String apkTestPath = "test/path/split_config.armeabi_v7a.apk";
    doReturn(new File(apkTestPath)).when(spyContext).getFilesDir();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);

    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(123, "TestModuleName");
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 1);
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.searchPaths[0], "libapp.so-123.part.so");
    assertTrue(jni.searchPaths[1].endsWith(apkTestPath + "!lib/armeabi-v7a/libapp.so-123.part.so"));
    assertEquals(jni.searchPaths.length, 2);
    assertEquals(jni.loadingUnitId, 123);
  }

  @Test
  public void invalidSearchPathsAreIgnored() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Context spyContext = createSpyContext(null);
    doReturn(null).when(spyContext).getAssets();
    String apkTestPath = "test/path/invalidpath.apk";
    doReturn(new File(apkTestPath)).when(spyContext).getFilesDir();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);

    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(123, "TestModuleName");
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 1);
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.searchPaths[0], "libapp.so-123.part.so");
    assertEquals(jni.searchPaths.length, 1);
    assertEquals(jni.loadingUnitId, 123);
  }

  @Test
  public void assetManagerUpdateInvoked() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Context spyContext = createSpyContext(null);
    doReturn(null).when(spyContext).getAssets();
    AssetManager assetManager = spyContext.getAssets();
    String apkTestPath = "blah doesn't matter here";
    doReturn(new File(apkTestPath)).when(spyContext).getFilesDir();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);

    assertEquals(jni.loadingUnitId, 0);

    playStoreManager.installDeferredComponent(123, "TestModuleName");
    assertEquals(jni.loadDartDeferredLibraryCalled, 1);
    assertEquals(jni.updateAssetManagerCalled, 1);
    assertEquals(jni.deferredComponentInstallFailureCalled, 0);

    assertEquals(jni.assetManager, assetManager);
  }

  @Test
  public void stateGetterReturnsUnknowByDefault() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Context spyContext = createSpyContext(null);
    doReturn(null).when(spyContext).getAssets();
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
    assertEquals(playStoreManager.getDeferredComponentInstallState(-1, "invalidName"), "unknown");
  }

  @Test
  public void loadingUnitMappingFindsMatch() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Bundle bundle = new Bundle();
    bundle.putString(PlayStoreDeferredComponentManager.MAPPING_KEY, "2:module1,5:module2");
    Context spyContext = createSpyContext(bundle);
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);

    assertTrue(playStoreManager.uninstallDeferredComponent(5, null));
    assertTrue(playStoreManager.uninstallDeferredComponent(2, null));
    assertFalse(playStoreManager.uninstallDeferredComponent(3, null));
  }

  @Test
  public void assetOnlyMappingParses() throws NameNotFoundException {
    TestFlutterJNI jni = new TestFlutterJNI();
    Bundle bundle = new Bundle();
    bundle.putString(PlayStoreDeferredComponentManager.MAPPING_KEY, "");
    Context spyContext = createSpyContext(bundle);
    TestPlayStoreDeferredComponentManager playStoreManager =
        new TestPlayStoreDeferredComponentManager(spyContext, jni);
  }

  @Test
  public void installDeferredComponent_whenAlreadyInstalled_loadsImmediately() throws Exception {
    TestFlutterJNI jni = new TestFlutterJNI();
    Bundle bundle = new Bundle();
    bundle.putString(PlayStoreDeferredComponentManager.MAPPING_KEY, "123:TestModuleName");
    Context spyContext = createSpyContext(bundle);
    doReturn(null).when(spyContext).getAssets();

    String soTestFilename = "libapp.so-123.part.so";
    String soTestPath = "test/path/" + soTestFilename;
    doReturn(new File(soTestPath)).when(spyContext).getFilesDir();

    // Create a real manager instance
    PlayStoreDeferredComponentManager playStoreManager =
        new PlayStoreDeferredComponentManager(spyContext, jni);
    jni.setDeferredComponentManager(playStoreManager);

    // Mock and set the DeferredComponentChannel to verify the future/method channel completes
    DeferredComponentChannel mockChannel = mock(DeferredComponentChannel.class);
    playStoreManager.setDeferredComponentChannel(mockChannel);

    // Mock SplitInstallManager
    SplitInstallManager mockSplitInstallManager = mock(SplitInstallManager.class);
    Set<String> installedModules = new HashSet<>();
    installedModules.add("TestModuleName");
    when(mockSplitInstallManager.getInstalledModules()).thenReturn(installedModules);

    // Mock startInstall to return a mocked Task that succeeds immediately (simulate play store return)
    @SuppressWarnings("unchecked")
    Task<Integer> mockTask = mock(Task.class);
    when(mockTask.addOnSuccessListener(any())).thenAnswer(invocation -> {
      OnSuccessListener<Integer> listener = invocation.getArgument(0);
      listener.onSuccess(1);
      return mockTask;
    });
    when(mockTask.addOnFailureListener(any())).thenReturn(mockTask);
    when(mockSplitInstallManager.startInstall(any())).thenReturn(mockTask);

    // Inject our mocked SplitInstallManager using reflection
    Field splitInstallManagerField =
        PlayStoreDeferredComponentManager.class.getDeclaredField("splitInstallManager");
    splitInstallManagerField.setAccessible(true);
    splitInstallManagerField.set(playStoreManager, mockSplitInstallManager);

    // Run the test
    playStoreManager.installDeferredComponent(123, "TestModuleName");

    // Assert that the manager immediately loaded the library and assets
    assertEquals(1, jni.loadDartDeferredLibraryCalled);
    assertEquals(1, jni.updateAssetManagerCalled);
    assertEquals(0, jni.deferredComponentInstallFailureCalled);

    // Assert that the channel was notified of success, completing the Dart future
    verify(mockChannel).completeInstallSuccess("TestModuleName");

    // Assert that the state is "installed"
    assertEquals("installed", playStoreManager.getDeferredComponentInstallState(123, "TestModuleName"));

    // Reset mock channel invocations to isolate the second call
    org.mockito.Mockito.clearInvocations(mockChannel);

    // Call install again for the same component
    playStoreManager.installDeferredComponent(123, "TestModuleName");

    // Assert that JNI load functions were NOT called again (counts remain at 1)
    assertEquals(1, jni.loadDartDeferredLibraryCalled);
    assertEquals(1, jni.updateAssetManagerCalled);

    // Assert that the channel was still notified of success for the second request
    verify(mockChannel).completeInstallSuccess("TestModuleName");
  }
}
