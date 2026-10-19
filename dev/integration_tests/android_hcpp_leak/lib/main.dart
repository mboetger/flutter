import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';

_HcppLeakBodyState? hcppLeakBodyState;

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const MaterialApp(home: HcppLeakBody()));
}

class HcppLeakBody extends StatefulWidget {
  const HcppLeakBody({super.key});

  @override
  State<HcppLeakBody> createState() => _HcppLeakBodyState();
}

class _HcppLeakBodyState extends State<HcppLeakBody> {
  bool _scrim = false;

  @override
  void initState() {
    super.initState();
    hcppLeakBodyState = this;
  }

  void toggle() {
    setState(() {
      _scrim = !_scrim;
    });
  }

  @override
  Widget build(BuildContext context) {
    final video = PlatformViewLink(
      viewType: 'test_view',
      surfaceFactory: (context, controller) => AndroidViewSurface(
        controller: controller as AndroidViewController,
        hitTestBehavior: PlatformViewHitTestBehavior.transparent,
        gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
      ),
      onCreatePlatformView: (params) {
        final controller = PlatformViewsService.initHybridAndroidView(
                id: params.id,
                viewType: 'test_view',
                layoutDirection: TextDirection.ltr,
                creationParamsCodec: const StandardMessageCodec(),
              );
        controller
          ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
          ..create();
        return controller;
      },
    );

    return Scaffold(
      backgroundColor: Colors.black,
      body: Stack(
        fit: StackFit.expand,
        children: [
          Center(
            child: SizedBox.expand(child: video),
          ),
          if (_scrim)
            const ColoredBox(
              color: Color(0x99000000),
              child: Center(child: CircularProgressIndicator()),
            ),
        ],
      ),
    );
  }
}
