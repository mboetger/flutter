# Copyright 2013 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib.machinery
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest

SKY_TOOLS = os.path.dirname(os.path.abspath(__file__))
loader = importlib.machinery.SourceFileLoader('gn', os.path.join(SKY_TOOLS, 'gn'))
spec = importlib.util.spec_from_file_location('gn', os.path.join(SKY_TOOLS, 'gn'), loader=loader)
gn = importlib.util.module_from_spec(spec)
sys.modules['gn'] = gn
spec.loader.exec_module(gn)


class GNTestCase(unittest.TestCase):

  def _expect_build_dir(self, arg_list, expected_build_dir):
    args = gn.parse_args(['gn'] + arg_list)
    self.assertEqual(gn.get_out_dir(args), expected_build_dir)

  def test_get_out_dir(self):
    self._expect_build_dir(['--runtime-mode', 'debug'], os.path.join('out', 'host_debug'))
    self._expect_build_dir(['--runtime-mode', 'release'], os.path.join('out', 'host_release'))
    self._expect_build_dir(['--ios'], os.path.join('out', 'ios_debug'))
    self._expect_build_dir(['--ios', '--darwin-extension-safe'],
                           os.path.join('out', 'ios_debug_extension_safe'))
    self._expect_build_dir(['--ios', '--runtime-mode', 'release'],
                           os.path.join('out', 'ios_release'))
    self._expect_build_dir(['--ios', '--darwin-extension-safe', '--runtime-mode', 'release'],
                           os.path.join('out', 'ios_release_extension_safe'))
    self._expect_build_dir(['--android'], os.path.join('out', 'android_debug'))
    self._expect_build_dir(['--android', '--runtime-mode', 'release'],
                           os.path.join('out', 'android_release'))

  def _gn_args(self, arg_list):
    args = gn.parse_args(['gn'] + arg_list)
    return gn.to_gn_args(args)

  def test_to_gn_args(self):
    # This would not necessarily be true on a 32-bit machine?
    if sys.platform == 'darwin':
      self.assertEqual(
          self._gn_args(['--ios', '--simulator', '--simulator-cpu', 'x64'])['target_cpu'], 'x64'
      )
      self.assertEqual(self._gn_args(['--ios'])['target_cpu'], 'arm64')

  def test_cannot_use_android_and_enable_unittests(self):
    with self.assertRaises(Exception):
      self._gn_args(['--android', '--enable-unittests'])

  def test_cannot_use_ios_and_enable_unittests(self):
    with self.assertRaises(Exception):
      self._gn_args(['--ios', '--enable-unittests'])

  def test_parse_size(self):
    self.assertEqual(gn.parse_size('5B'), 5)
    self.assertEqual(gn.parse_size('5KB'), 5 * 2**10)
    self.assertEqual(gn.parse_size('5MB'), 5 * 2**20)
    self.assertEqual(gn.parse_size('5GB'), 5 * 2**30)

  def test_no_asserts_in_release_unopt(self):
    # Use a temporary directory to keep the test hermetic
    with tempfile.TemporaryDirectory() as temp_dir:
      arg_list = [
          '--unopt',
          '--runtime-mode', 'release',
          '--no-lto',
          '--out-dir', temp_dir,
      ]
      parsed_args = gn.parse_args(['gn'] + arg_list)
      out_dir = gn.get_out_dir(parsed_args)
      
      gn_wrapper = os.path.join(SKY_TOOLS, 'gn')
      cmd = [sys.executable, gn_wrapper] + arg_list
      
      # Run GN
      subprocess.check_call(cmd, cwd=gn.SRC_ROOT)
      
      # Path to the generated toolchain.ninja inside the temp directory
      toolchain_ninja = os.path.join(out_dir, 'toolchain.ninja')
      
      self.assertTrue(os.path.exists(toolchain_ninja), f"{toolchain_ninja} does not exist")
      
      with open(toolchain_ninja, 'r', encoding='utf-8') as f:
        content = f.read()
        
      rule_trigger = 'rule __flutter_lib_snapshot_generate_snapshot_bin'
      
      found_rule = False
      in_rule_block = False
      command_line = None
      
      for line in content.splitlines():
        if line.startswith(rule_trigger):
          found_rule = True
          in_rule_block = True
          continue
        if in_rule_block:
          if line.strip() == '':
            continue
          if not line.startswith(' '):
            # End of rule block
            in_rule_block = False
            continue
          # We are in the rule block and the line is indented
          stripped = line.strip()
          if stripped.startswith('command ='):
            command_line = stripped
            break
            
      self.assertTrue(found_rule, "Could not find generate_snapshot_bin rule in toolchain.ninja")
      self.assertIsNotNone(command_line, "Could not find command in generate_snapshot_bin rule")
      self.assertNotIn('--enable_asserts', command_line, 
                       "Found --enable_asserts in generate_snapshot_bin rule for release mode!")


if __name__ == '__main__':
  unittest.main()
