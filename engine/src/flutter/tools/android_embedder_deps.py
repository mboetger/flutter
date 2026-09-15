#!/usr/bin/env python3
#
# Copyright 2013 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Measures and enforces dependency boundaries for the Android embedder.

The ultimate architectural goal of the Android embedder migration is for
shell/platform/android to depend only on:
  - //flutter/fml
  - //flutter/assets
  - //flutter/common
  - embedder.h (//flutter/shell/platform/embedder:embedder_headers)

This script scans all BUILD.gn files in shell/platform/android and counts:
  1. Internal-engine GN dependencies (anything starting with //flutter/ outside
     the allowed list and outside shell/platform/android itself).
  2. Internal-engine #include headers in target sources.
  3. Occurrences of '// nogncheck'.

In --check mode, this script fails if any count increases relative to the
committed baseline at tools/android_embedder_deps_baseline.json.
"""

import argparse
import json
import os
import re
import sys

ALLOWED_GN_PREFIXES = (
    '//flutter/fml',
    '//flutter/assets',
    '//flutter/common',
    '//flutter/shell/platform/embedder',
    '//flutter/shell/platform/android',
    ':',
)

ALLOWED_HEADER_PREFIXES = (
    'flutter/fml/',
    'flutter/assets/',
    'flutter/common/',
    'flutter/shell/platform/embedder/embedder.h',
    'flutter/shell/platform/android/',
)


def get_engine_root():
  """Returns the engine root (engine/src/flutter)."""
  script_dir = os.path.dirname(os.path.abspath(__file__))
  return os.path.dirname(script_dir)


def extract_list_items(body, field_names):
  """Extracts string elements from lists like `deps = [...]` or `sources += [...]`."""
  items = []
  for field in field_names:
    pattern = re.compile(rf'{field}\s*\+?=\s*\[(.*?)\]', re.DOTALL)
    for m in pattern.finditer(body):
      list_content = m.group(1)
      for str_match in re.finditer(r'"([^"]+)"', list_content):
        items.append(str_match.group(1))
  return items


def parse_gn_file(filepath):
  """Parses target blocks and nogncheck comments from a GN file."""
  with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

  gn_nognchecks = len(re.findall(r'//\s*nogncheck', content))

  target_regex = re.compile(r'([a-zA-Z_0-9]+)\s*\(\s*"([^"]+)"\s*\)\s*\{', re.MULTILINE)

  targets = {}
  pos = 0
  while True:
    m = target_regex.search(content, pos)
    if not m:
      break
    target_type = m.group(1)
    target_name = m.group(2)
    start_brace = m.end() - 1

    depth = 0
    end_brace = -1
    in_string = False
    escape = False
    for i in range(start_brace, len(content)):
      ch = content[i]
      if escape:
        escape = False
        continue
      if ch == '\\':
        escape = True
        continue
      if ch == '"':
        in_string = not in_string
        continue
      if in_string:
        continue
      if ch == '{':
        depth += 1
      elif ch == '}':
        depth -= 1
        if depth == 0:
          end_brace = i
          break
    if end_brace == -1:
      break

    body = content[start_brace + 1:end_brace]
    targets[target_name] = {
        'type': target_type,
        'body': body,
        'gn_file': filepath,
    }
    pos = end_brace + 1

  return targets, gn_nognchecks


def scan_android_embedder(engine_root):
  """Scans all targets in shell/platform/android and computes dependency counts."""
  android_root = os.path.join(engine_root, 'shell', 'platform', 'android')
  gn_files = []
  for root, _, files in os.walk(android_root):
    for f in files:
      if f == 'BUILD.gn':
        gn_files.append(os.path.join(root, f))
  gn_files.sort()

  results = {
      'targets': {},
      'total_gn_nogncheck': 0,
  }

  for gf in gn_files:
    targets, gn_nognchecks = parse_gn_file(gf)
    results['total_gn_nogncheck'] += gn_nognchecks
    dir_path = os.path.dirname(gf)

    for tname, tinfo in sorted(targets.items()):
      if tinfo['type'] not in (
          'source_set',
          'executable',
          'shared_library',
          'static_library',
      ):
        continue

      target_key = f"//{os.path.relpath(dir_path, engine_root)}:{tname}"
      deps = extract_list_items(tinfo['body'], ['deps', 'public_deps'])
      sources = extract_list_items(tinfo['body'], ['sources'])

      internal_deps = []
      for d in deps:
        if d.startswith('//flutter/') and not any(d.startswith(p) for p in ALLOWED_GN_PREFIXES):
          internal_deps.append(d)

      internal_includes = []
      source_nognchecks = 0
      for s in sources:
        src_path = os.path.join(dir_path, s)
        if not os.path.exists(src_path):
          continue
        with open(src_path, 'r', encoding='utf-8', errors='ignore') as sf:
          for line in sf:
            if 'nogncheck' in line:
              source_nognchecks += 1
            m = re.match(r'^\s*#include\s+["<](flutter/[^">]+)[">]', line)
            if m:
              inc = m.group(1)
              if not any(inc.startswith(p) or inc == p for p in ALLOWED_HEADER_PREFIXES):
                internal_includes.append(f"{s}:{inc}")

      results['targets'][target_key] = {
          'type': tinfo['type'],
          'internal_deps_count': len(internal_deps),
          'internal_deps': sorted(list(set(internal_deps))),
          'internal_includes_count': len(internal_includes),
          'internal_includes': sorted(list(set(internal_includes))),
          'nogncheck_count': source_nognchecks,
      }

  return results


def format_table(results):
  """Formats a human-readable table of the results."""
  lines = []
  lines.append(f"{'Target':<65} | {'Deps':<5} | {'Includes':<8} | {'nogncheck':<9}")
  lines.append('-' * 95)
  total_deps = 0
  total_includes = 0
  total_nogncheck = results['total_gn_nogncheck']

  for tk, tv in sorted(results['targets'].items()):
    total_deps += tv['internal_deps_count']
    total_includes += tv['internal_includes_count']
    total_nogncheck += tv['nogncheck_count']
    lines.append(
        f"{tk:<65} | {tv['internal_deps_count']:<5} |"
        f" {tv['internal_includes_count']:<8} | {tv['nogncheck_count']:<9}"
    )

  lines.append('-' * 95)
  lines.append(f"{'TOTAL':<65} | {total_deps:<5} | {total_includes:<8} |"
               f' {total_nogncheck:<9}')
  return '\n'.join(lines)


def check_against_baseline(current, baseline_file):
  """Compares current results against the committed baseline."""
  if not os.path.exists(baseline_file):
    print(
        f'Error: Baseline file not found at {baseline_file}. Run with'
        ' --update-baseline first.',
        file=sys.stderr,
    )
    return False

  with open(baseline_file, 'r', encoding='utf-8') as f:
    baseline = json.load(f)

  violations = []
  base_targets = baseline.get('targets', {})
  curr_targets = current.get('targets', {})

  # Check total GN nogncheck
  if current['total_gn_nogncheck'] > baseline.get('total_gn_nogncheck', 0):
    violations.append(
        f"GN file nogncheck increased from {baseline.get('total_gn_nogncheck', 0)} to {current['total_gn_nogncheck']}"
    )

  for tk, cv in curr_targets.items():
    if tk not in base_targets:
      if (cv['internal_deps_count'] > 0 or cv['internal_includes_count'] > 0 or
          cv['nogncheck_count'] > 0):
        violations.append(
            f'New target {tk} introduced with non-zero counts: {cv["internal_deps_count"]} deps, {cv["internal_includes_count"]} includes, {cv["nogncheck_count"]} nogncheck'
        )
      continue

    bv = base_targets[tk]
    if cv['internal_deps_count'] > bv['internal_deps_count']:
      new_deps = set(cv['internal_deps']) - set(bv['internal_deps'])
      violations.append(
          f"{tk}: internal deps increased from {bv['internal_deps_count']} to {cv['internal_deps_count']} (added: {sorted(list(new_deps))})"
      )
    if cv['internal_includes_count'] > bv['internal_includes_count']:
      new_incs = set(cv['internal_includes']) - set(bv['internal_includes'])
      violations.append(
          f"{tk}: internal includes increased from {bv['internal_includes_count']} to {cv['internal_includes_count']} (added: {sorted(list(new_incs))})"
      )
    if cv['nogncheck_count'] > bv['nogncheck_count']:
      violations.append(
          f"{tk}: nogncheck count increased from {bv['nogncheck_count']} to {cv['nogncheck_count']}"
      )

  if violations:
    print('Dependency ratchet check FAILED:', file=sys.stderr)
    for v in violations:
      print(f'  - {v}', file=sys.stderr)
    return False

  return True


def main():
  parser = argparse.ArgumentParser(description='Android Embedder Dependency Ratchet')
  parser.add_argument(
      '--check',
      action='store_true',
      help='Compare counts against committed baseline and exit non-zero if increased',
  )
  parser.add_argument(
      '--update-baseline',
      action='store_true',
      help='Update baseline json with current counts',
  )
  parser.add_argument(
      '--baseline',
      type=str,
      default=None,
      help='Path to baseline json (default: tools/android_embedder_deps_baseline.json)',
  )
  args = parser.parse_args()

  engine_root = get_engine_root()
  baseline_path = args.baseline or os.path.join(
      engine_root, 'tools', 'android_embedder_deps_baseline.json'
  )

  results = scan_android_embedder(engine_root)

  if args.update_baseline:
    with open(baseline_path, 'w', encoding='utf-8') as f:
      json.dump(results, f, indent=2, sort_keys=True)
      f.write('\n')
    print(f'Baseline successfully updated at {baseline_path}')
    print(format_table(results))
    return 0

  print(format_table(results))

  if args.check:
    if not check_against_baseline(results, baseline_path):
      return 1
    print('Dependency ratchet check PASSED (all counts <= baseline).')

  return 0


if __name__ == '__main__':
  sys.exit(main())
