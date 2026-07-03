// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.integration.platformviews;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;

import java.util.HashMap;

import io.flutter.embedding.android.FlutterFragmentActivity;
import androidx.fragment.app.Fragment;
import android.widget.LinearLayout;
import android.widget.EditText;
import android.widget.Button;
import android.widget.PopupMenu;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.content.Context;
import io.flutter.plugin.platform.PlatformView;
import io.flutter.plugin.platform.PlatformViewFactory;
import io.flutter.plugin.common.StandardMessageCodec;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugins.GeneratedPluginRegistrant;

public class MainActivity extends FlutterFragmentActivity implements MethodChannel.MethodCallHandler {
    final static int STORAGE_PERMISSION_CODE = 1;

    MethodChannel mMethodChannel;

    // The method result to complete with the Android permission request result.
    // This is null when not waiting for the Android permission request;
    private MethodChannel.Result permissionResult;

    private PlatformViewFragment mFragment;

    public static class PlatformViewFragment extends Fragment {
        EditText editText;
        Button button;
        boolean popupShown = false;
        String popupError = null;

        @Override
        public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            Context context = getContext();
            LinearLayout layout = new LinearLayout(context);
            layout.setOrientation(LinearLayout.VERTICAL);

            editText = new EditText(context);
            editText.setId(View.generateViewId());
            editText.setHint("Enter text");
            editText.setTag("fragment_edit_text");
            layout.addView(editText);

            button = new Button(context);
            button.setId(View.generateViewId());
            button.setText("Show Popup");
            button.setTag("fragment_button");
            button.setOnClickListener(v -> {
                try {
                    PopupMenu popup = new PopupMenu(context, button);
                    popup.getMenu().add("Item 1");
                    popup.show();
                    popupShown = true;
                } catch (Exception e) {
                    popupError = e.toString();
                    popupShown = false;
                }
            });
            layout.addView(button);

            return layout;
        }
    }

    private View getFlutterView() {
      return findViewById(FLUTTER_VIEW_ID);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mFragment = new PlatformViewFragment();
        FrameLayout dummyContainer = new FrameLayout(this);
        dummyContainer.setVisibility(View.GONE);
        int containerId = View.generateViewId();
        dummyContainer.setId(containerId);
        ViewGroup rootView = findViewById(android.R.id.content);
        rootView.addView(dummyContainer);

        getSupportFragmentManager()
            .beginTransaction()
            .add(containerId, mFragment, "my_fragment")
            .commit();
        getSupportFragmentManager().executePendingTransactions();
    }

    @Override
    public void configureFlutterEngine(FlutterEngine flutterEngine) {
        DartExecutor executor = flutterEngine.getDartExecutor();
        flutterEngine
            .getPlatformViewsController()
            .getRegistry()
            .registerViewFactory("simple_view", new SimpleViewFactory(executor));

        flutterEngine
            .getPlatformViewsController()
            .getRegistry()
            .registerViewFactory("fragment_view", new PlatformViewFactory(StandardMessageCodec.INSTANCE) {
                @Override
                public PlatformView create(Context context, int id, Object args) {
                    return new PlatformView() {
                        @Override
                        public View getView() {
                            View view = mFragment.getView();
                            if (view != null && view.getParent() != null) {
                                ((ViewGroup) view.getParent()).removeView(view);
                            }
                            return view;
                        }

                        @Override
                        public void dispose() {}
                    };
                }
            });

        mMethodChannel = new MethodChannel(executor, "android_views_integration");
        mMethodChannel.setMethodCallHandler(this);
        GeneratedPluginRegistrant.registerWith(flutterEngine);
    }

    @Override
    public void onMethodCall(MethodCall methodCall, MethodChannel.Result result) {
        switch (methodCall.method) {
            case "pipeFlutterViewEvents":
                result.success(null);
                return;
            case "stopFlutterViewEvents":
                result.success(null);
                return;
            case "getStoragePermission":
                if (permissionResult != null) {
                    result.error("error", "already waiting for permissions", null);
                    return;
                }
                permissionResult = result;
                getExternalStoragePermissions();
                return;
            case "synthesizeEvent":
                synthesizeEvent(methodCall, result);
                return;
            case "getEditTextText":
                if (mFragment != null && mFragment.editText != null) {
                    result.success(mFragment.editText.getText().toString());
                } else {
                    result.error("error", "fragment or edittext not ready", null);
                }
                return;
            case "clickPopup":
                if (mFragment != null && mFragment.button != null) {
                    mFragment.button.performClick();
                    result.success(null);
                } else {
                    result.error("error", "fragment or button not ready", null);
                }
                return;
            case "getPopupResult":
                if (mFragment != null) {
                    java.util.HashMap<String, Object> map = new java.util.HashMap<>();
                    map.put("shown", mFragment.popupShown);
                    map.put("error", mFragment.popupError);
                    result.success(map);
                } else {
                    result.error("error", "fragment not ready", null);
                }
                return;
        }
        result.notImplemented();
    }

    @SuppressWarnings("unchecked")
    public void synthesizeEvent(MethodCall methodCall, MethodChannel.Result result) {
        MotionEvent event = MotionEventCodec.decode((HashMap<String, Object>) methodCall.arguments());
        getFlutterView().dispatchTouchEvent(event);
        // TODO(egarciad): Remove invokeMethod since it is not necessary.
        mMethodChannel.invokeMethod("onTouch", MotionEventCodec.encode(event));
        result.success(null);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode != STORAGE_PERMISSION_CODE || permissionResult == null)
            return;
        boolean permissionGranted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        sendPermissionResult(permissionGranted);
    }


    private void getExternalStoragePermissions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M)
            return;

        if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED) {
            sendPermissionResult(true);
            return;
        }

        requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, STORAGE_PERMISSION_CODE);
    }

    private void sendPermissionResult(boolean result) {
        if (permissionResult == null)
            return;
        permissionResult.success(result);
        permissionResult = null;
    }
}
