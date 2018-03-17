#!/usr/bin/env python
# -*- coding: utf-8 -*-
'''
Unit test file for ../inkex.py
Revision history:
  * 2012-01-27 (jazzynico): check errormsg function.

--
If you want to help, read the python unittest documentation:
http://docs.python.org/library/unittest.html
'''

import StringIO
import sys
import unittest

sys.path.append('..') # this line allows to import the extension code
import inkex


class ErrormsgTest(unittest.TestCase):
    """Tests for inkex.errormsg()."""

    def setUp(self):
        """Installs a fake stderr in the inkex module."""
        self.fake_stderr = StringIO.StringIO()
        self._real_stderr = inkex.sys.stderr
        inkex.sys.stderr = self.fake_stderr

    def tearDown(self):
        """Restores the real stderr in the inkex module."""
        self.fake_stderr.close()
        inkex.sys.stderr = self._real_stderr

    def assertOutput(self, expected_bytes):
        """Tests the bytes written to the fake stderr."""
        self.assertEqual(self.fake_stderr.getvalue(), expected_bytes)

    def test_ascii(self):
        # A string (not the 'unicode' type), containing only 7-bit-clean
        # ASCII characters.
        inkex.errormsg('ABCabc')
        self.assertOutput('ABCabc\n')

    def test_nonunicode_type(self):
        # A string (not the 'unicode' type), containing literal
        # non-ASCII Unicode characters.
        msg = 'Àûïàèé'
        # Demonstrate that this string contains UTF-8-encoded bytes,
        # because this text file is encoded as UTF-8.
        self.assertEqual(
                msg, '\xc3\x80\xc3\xbb\xc3\xaf\xc3\xa0\xc3\xa8\xc3\xa9')

        inkex.errormsg(msg)
        self.assertOutput('\xc3\x80\xc3\xbb\xc3\xaf\xc3\xa0\xc3\xa8\xc3\xa9\n')

    def test_unicode_type(self):
        # A 'unicode' string, containing literal non-ASCII Unicode
        # characters.
        msg = u'Àûïàèé'
        # Demonstrate that this string contains Unicode characters, not UTF-8.
        self.assertEqual(msg, u'\xc0\xfb\xef\xe0\xe8\xe9')

        inkex.errormsg(msg)
        self.assertOutput('\xc3\x80\xc3\xbb\xc3\xaf\xc3\xa0\xc3\xa8\xc3\xa9\n')


if __name__ == '__main__':
    unittest.main()
