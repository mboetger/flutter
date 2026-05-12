// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../base/context.dart';
import 'gradle.dart';

export 'gradle.dart' show AndroidBuilder;

/// The builder in the current context.
AndroidBuilder? get androidBuilder {
  return context.get<AndroidBuilder>();
}
