// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:test/bootstrap/browser.dart';
import 'package:test/test.dart';
import 'package:ui/src/engine.dart';
import 'package:ui/ui_web/src/ui_web.dart' as ui_web;

import '../common/test_initialization.dart';

void main() {
  internalBootstrapBrowserTest(() => testMain);
}

Future<void> testMain() async {
  setUpImplicitView();

  setUp(() {
    domDocument.activeElement?.blur();
  });

  test('Firefox focus is restored after blur', () async {
    // This test verifies that focus is successfully restored to the input
    // element after a blur event, even on Firefox where refocusing must
    // be done asynchronously.

    final textEditing = HybridTextEditing();
    final DefaultTextEditingStrategy strategy = createDefaultTextEditingStrategy(textEditing);
    textEditing.debugTextEditingStrategyOverride = strategy;

    expect(
      strategy,
      isA<FirefoxTextEditingStrategy>(),
      reason: 'Strategy should be FirefoxTextEditingStrategy on Firefox',
    );

    final config = InputConfiguration(viewId: kImplicitViewId);

    strategy.enable(
      config,
      onChange: (EditingState? state, TextEditingDeltaState? deltaState) {},
      onAction: (String? action) {},
    );

    final DomHTMLElement inputElement = strategy.domElement!;
    expect(domDocument.activeElement, inputElement);

    // Create another element in the same view to transfer focus to.
    final DomElement otherElement = createDomElement('button');
    // Ensure it can receive focus
    otherElement.setAttribute('tabindex', '0');
    strategy.activeDomElementView!.dom.textEditingHost.append(otherElement);

    // Focus the other element. This will trigger a blur event on the input element.
    otherElement.focusWithoutScroll();

    // In headless environments, programmatically focusing another element might not
    // reliably dispatch the 'blur' event if the browser window does not have OS-level focus.
    // If the event was not fired, we dispatch it manually to ensure the engine's blur handler
    // is executed.
    if (strategy.debugBlurCount == 0) {
      inputElement.dispatchEvent(
        createDomFocusEvent('blur', <String, dynamic>{'relatedTarget': otherElement}),
      );
    }

    // Yield to the event loop to allow the asynchronous refocus Timer to run.
    await Future<void>.delayed(const Duration(milliseconds: 100));

    expect(strategy.debugBlurCount, 1, reason: 'The blur event should have been fired');

    expect(
      domDocument.activeElement,
      inputElement,
      reason: 'Focus should have been restored to the input element',
    );

    otherElement.remove();
    strategy.disable();
  }, skip: ui_web.browser.browserEngine != ui_web.BrowserEngine.firefox);
}
