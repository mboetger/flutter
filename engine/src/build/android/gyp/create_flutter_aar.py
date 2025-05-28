#!/usr/bin/env python3
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create a AAR incorporating all the components required to build a Flutter application"""

import optparse
import os
import sys
import zipfile

from util import build_utils

def main(args):
  args = build_utils.ExpandFileArgs(args)
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--output', help='Path to output jar.')
  parser.add_option('--output_native_jar', help='Path to output native library jar.')
  parser.add_option('--dist_jar', help='Flutter shell Java code jar.')
  parser.add_option('--native_lib', action='append', help='Native code library.')
  parser.add_option('--android_abi', help='Native code ABI.')
  parser.add_option('--asset_dir', help='Path to assets.')
  options, _ = parser.parse_args(args)
  build_utils.CheckOptions(options, parser, [
    'output', 'dist_jar', 'native_lib', 'android_abi'
  ])

  classes_jar = "classes.jar"

  with zipfile.ZipFile(options.output, 'w', zipfile.ZIP_DEFLATED) as out_zip:
    with zipfile.ZipFile(classes_jar, 'w', zipfile.ZIP_DEFLATED) as classes_zip:
      with zipfile.ZipFile(options.dist_jar, 'r') as dist_zip:
        for dist_file in dist_zip.infolist():
          if dist_file.filename.endswith('.class'):
            classes_zip.writestr(dist_file.filename, dist_zip.read(dist_file.filename))
    with zipfile.ZipFile(classes_jar, 'r', zipfile.ZIP_DEFLATED) as classes_zip:
      out_zip.write(classes_jar)

    for native_lib in options.native_lib:
      out_zip.write(native_lib,
                    'libs/%s/%s' % (options.android_abi, os.path.basename(native_lib)))


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
