#!/usr/bin/env python3
#
# Copyright 2013 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import subprocess
import sys

ANDROID_LOG_CLASS = 'android.util.Log'
FLUTTER_LOG_CLASS = 'io.flutter.Log'

ANDROIDX_TRACE_CLASS = 'androidx.tracing.Trace'
ANDROID_TRACE_CLASS = 'android.tracing.Trace'
FLUTTER_TRACE_CLASS = 'io.flutter.util.TraceSection'

ANDROID_BUILD_VERSION_CODE_CLASS = 'VERSION_CODES'

LITERAL_SUFFIX_RE = re.compile(r'\b(?:0[xX][0-9a-fA-F_]+|0[bB][01_]+|[0-9][0-9_]*)l\b')


def StripCommentsAndStrings(text):
  """Strips Java comments, string literals, and character literals from the text.

  Replaces the content of comments and literals with spaces, preserving
  newlines and the exact character indices/offsets of the original text.
  This allows accurate line-number and column reporting of matches.
  """
  result = []
  i = 0
  n = len(text)
  while i < n:
    if i + 1 < n and text[i] == '/' and text[i+1] == '*':
      result.append(' ')
      result.append(' ')
      i += 2
      while i < n:
        if i + 1 < n and text[i] == '*' and text[i+1] == '/':
          result.append(' ')
          result.append(' ')
          i += 2
          break
        else:
          if text[i] == '\n':
            result.append('\n')
          else:
            result.append(' ')
          i += 1
    elif i + 1 < n and text[i] == '/' and text[i+1] == '/':
      result.append(' ')
      result.append(' ')
      i += 2
      while i < n:
        if text[i] == '\n':
          result.append('\n')
          i += 1
          break
        else:
          result.append(' ')
          i += 1
    elif text[i] == '"':
      result.append(' ')
      i += 1
      while i < n:
        if text[i] == '"':
          result.append(' ')
          i += 1
          break
        elif text[i] == '\\':
          result.append(' ')
          if i + 1 < n:
            if text[i+1] == '\n':
              result.append('\n')
            else:
              result.append(' ')
            i += 2
          else:
            i += 1
        else:
          if text[i] == '\n':
            result.append('\n')
          else:
            result.append(' ')
          i += 1
    elif text[i] == "'":
      result.append(' ')
      i += 1
      while i < n:
        if text[i] == "'":
          result.append(' ')
          i += 1
          break
        elif text[i] == '\\':
          result.append(' ')
          if i + 1 < n:
            if text[i+1] == '\n':
              result.append('\n')
            else:
              result.append(' ')
            i += 2
          else:
            i += 1
        else:
          if text[i] == '\n':
            result.append('\n')
          else:
            result.append(' ')
          i += 1
    else:
      result.append(text[i])
      i += 1
  return "".join(result)


def CheckLiteralSuffixes(file_path, original_contents):
  stripped = StripCommentsAndStrings(original_contents)
  matches = list(LITERAL_SUFFIX_RE.finditer(stripped))
  if not matches:
    return False

  print('\nError: Lowercase long literal suffix \'l\' detected in %s:' % file_path)
  lines = original_contents.splitlines()
  for match in matches:
    start_idx = match.start()
    line_number = original_contents[:start_idx].count('\n') + 1
    line_content = lines[line_number - 1]
    print('  - Line %d: %s' % (line_number, line_content.strip()))
    print('    Use uppercase \'L\' for long literals to avoid confusion with the digit \'1\'.')
  print('')
  return True


def CheckBadFiles(bad_files, bad_class, good_class):
  if bad_files:
    print('')
    print('Illegal import %s detected in the following files:' % bad_class)
    for bad_file in bad_files:
      print('  - ' + bad_file)
    print('Use %s instead.' % good_class)
    print('')
    return True

  return False


def main():
  parser = argparse.ArgumentParser(
      description='Checks Flutter Android library for forbidden imports'
  )
  parser.add_argument('--stamp', type=str, required=True)
  parser.add_argument('--files', type=str, required=True, nargs='+')
  args = parser.parse_args()

  open(args.stamp, 'a').close()

  bad_log_files = []
  bad_trace_files = []
  bad_version_codes_files = []
  has_bad_suffixes = False

  for file in args.files:
    if not file.endswith('.java'):
      continue
    with open(file, encoding='utf-8') as f:
      contents = f.read()

    if CheckLiteralSuffixes(file, contents):
      has_bad_suffixes = True

    if (file.endswith(os.path.join('io', 'flutter', 'Log.java')) or
        file.endswith(os.path.join('io', 'flutter', 'util', 'TraceSection.java')) or
        file.endswith(os.path.join('io', 'flutter', 'Build.java'))):
      continue

    if ANDROID_LOG_CLASS in contents:
      bad_log_files.append(file)
    if ANDROIDX_TRACE_CLASS in contents or ANDROID_TRACE_CLASS in contents:
      bad_trace_files.append(file)
    if ANDROID_BUILD_VERSION_CODE_CLASS in contents:
      bad_version_codes_files.append(file)

  # Flutter's Log class allows additional configuration around verbosity.

  # Flutter's tracing class makes sure we do not violate string lengths that
  # cause crashes at runtime.

  # Flutter's Build.API_LEVELS class is clearer to read about which API version
  # is used.
  has_bad_files = CheckBadFiles(bad_log_files, ANDROID_LOG_CLASS,
                                FLUTTER_LOG_CLASS) or CheckBadFiles(
                                    bad_trace_files, 'android[x].tracing.Trace', FLUTTER_TRACE_CLASS
                                ) or CheckBadFiles(
                                    bad_version_codes_files, 'android.os.Build.VERSION_CODES',
                                    'io.flutter.Build.API_LEVELS'
                                )

  if has_bad_files or has_bad_suffixes:
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
