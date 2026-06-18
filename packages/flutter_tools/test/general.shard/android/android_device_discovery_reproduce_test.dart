// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter_tools/src/android/android_device.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:flutter_tools/src/device_vm_service_discovery_for_attach.dart';
import 'package:flutter_tools/src/mdns_discovery.dart';
import 'package:test/fake.dart';

import '../../src/common.dart';
import '../../src/context.dart';
import 'android_device_test.dart';

void main() {
  testUsingContext(
    'AndroidDevice VM Service discovery supports mDNS fallback',
    () async {
      final AndroidDevice device = setUpAndroidDevice();

      final VMServiceDiscoveryForAttach discovery = device.getVMServiceDiscoveryForAttach(
        ipv6: false,
        logger: BufferLogger.test(),
      );

      // Assert that the returned discovery is a DelegateVMServiceDiscoveryForAttach.
      expect(discovery, isA<DelegateVMServiceDiscoveryForAttach>());

      // We expect that the uris stream emits the URI discovered via mDNS.
      final List<Uri> emittedUris = await discovery.uris.toList();
      expect(emittedUris, contains(Uri.parse('http://127.0.0.1:12345/')));
    },
    overrides: <Type, Generator>{
      MDnsVmServiceDiscovery: () =>
          FakeMDnsVmServiceDiscovery(Uri.parse('http://127.0.0.1:12345/')),
    },
  );
}

class FakeMDnsVmServiceDiscovery extends Fake implements MDnsVmServiceDiscovery {
  FakeMDnsVmServiceDiscovery(this.uri);

  final Uri uri;

  @override
  Future<Uri?> getVMServiceUriForAttach(
    String? applicationId,
    Device device, {
    bool usesIpv6 = false,
    int? hostVmservicePort,
    int? deviceVmservicePort,
    bool useDeviceIPAsHost = false,
    Duration timeout = const Duration(minutes: 10),
  }) async {
    return uri;
  }
}
