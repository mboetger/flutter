// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:boolean_selector/boolean_selector.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_test/src/test_compat.dart' as test_compat;
import 'package:test_api/src/backend/group.dart'; // ignore: implementation_imports
import 'package:test_api/src/backend/group_entry.dart'; // ignore: implementation_imports
import 'package:test_api/src/backend/invoker.dart'; // ignore: implementation_imports
import 'package:test_api/src/backend/metadata.dart'; // ignore: implementation_imports

void main() {
  tearDown(() {
    // Reset to default after each test
    test_compat.includeTags = null;
    test_compat.excludeTags = null;
  });

  test('test_compat tag matching logic behaves correctly', () {
    test_compat.includeTags = BooleanSelector.parse('foo && bar');
    test_compat.excludeTags = BooleanSelector.parse('baz');

    // Should match if both foo and bar are present, and baz is absent
    expect(test_compat.matchesTags(<String>{'foo', 'bar'}), isTrue);
    expect(test_compat.matchesTags(<String>{'foo', 'bar', 'qux'}), isTrue);

    // Should NOT match if one of the include tags is missing
    expect(test_compat.matchesTags(<String>{'foo'}), isFalse);
    expect(test_compat.matchesTags(<String>{'bar'}), isFalse);

    // Should NOT match if the exclude tag is present
    expect(test_compat.matchesTags(<String>{'foo', 'bar', 'baz'}), isFalse);
  });

  test('test_compat tag matching logic handles null/empty filters', () {
    // When no filters are set, it should match everything
    expect(test_compat.matchesTags(<String>{}), isTrue);
    expect(test_compat.matchesTags(<String>{'foo'}), isTrue);

    // When only include filter is set
    test_compat.includeTags = BooleanSelector.parse('foo');
    expect(test_compat.matchesTags(<String>{'foo'}), isTrue);
    expect(test_compat.matchesTags(<String>{'bar'}), isFalse);

    // When only exclude filter is set
    test_compat.includeTags = null;
    test_compat.excludeTags = BooleanSelector.parse('bar');
    expect(test_compat.matchesTags(<String>{'foo'}), isTrue);
    expect(test_compat.matchesTags(<String>{'bar'}), isFalse);
  });

  test('test_compat hasMatchingTests correctly identifies groups with matching tags', () {
    test_compat.includeTags = BooleanSelector.parse('foo');

    // Case 1: Empty group
    final emptyGroup = Group('', <GroupEntry>[]);
    expect(test_compat.hasMatchingTests(emptyGroup), isFalse);

    // Case 2: Group with no matching tests
    final noMatchGroup = Group('', <GroupEntry>[
      LocalTest('test1', Metadata(tags: <String>{'bar'}), () {}),
    ]);
    expect(test_compat.hasMatchingTests(noMatchGroup), isFalse);

    // Case 3: Group with a matching test
    final matchGroup = Group('', <GroupEntry>[
      LocalTest('test2', Metadata(tags: <String>{'foo'}), () {}),
    ]);
    expect(test_compat.hasMatchingTests(matchGroup), isTrue);

    // Case 4: Nested group containing a matching test
    final nestedGroup = Group('', <GroupEntry>[
      Group('sub', <GroupEntry>[
        LocalTest('test3', Metadata(tags: <String>{'foo'}), () {}),
      ]),
    ]);
    expect(test_compat.hasMatchingTests(nestedGroup), isTrue);
  });
}
