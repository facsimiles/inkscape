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

import os
import StringIO
import sys
import tempfile
import unittest

sys.path.append('..') # this line allows to import the extension code
import inkex

# Used to construct simple SVG documents.
XML_HEADER = '<?xml version="1.0" encoding="UTF-8" standalone="no"?>'


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


class DimensionsTest(unittest.TestCase):
    """Tests for inkex.Effect.getDocument{Width,Height,Unit}."""

    @staticmethod
    def new_effect(svg_attrs):
        """Returns an inkex.Effect based on a simple SVG element.

        Args:
           svg_attrs: A string containing attributes to add to the
               root <svg> element of a minimal SVG document.
        Returns: An inkex.Effect object.
        """
        return new_test_effect(XML_HEADER + '<svg {}></svg>'.format(svg_attrs))

    #
    # Tests for inkex.Effect.getDocumentWidth()
    #

    def test_getDocumentWidth_no_dimensions(self):
        e = self.new_effect('')
        self.assertEqual(e.getDocumentWidth(), '0')

    def test_getDocumentWidth_empty_width(self):
        # An empty width value should be the same as a missing width.
        e = self.new_effect('width=""')
        self.assertEqual(e.getDocumentWidth(), '0')

    def test_getDocumentWidth_empty_viewBox(self):
        # An empty viewBox value should be the same as a missing viewBox.
        e = self.new_effect('viewBox=""')
        self.assertEqual(e.getDocumentWidth(), '0')

    def test_getDocumentWidth_empty_width_and_empty_viewBox(self):
        # Empty values for both should be the same as both missing.
        e = self.new_effect('width="" viewBox=""')
        self.assertEqual(e.getDocumentWidth(), '0')

    def test_getDocumentWidth_width_only(self):
        e = self.new_effect('width="120mm"')
        self.assertEqual(e.getDocumentWidth(), '120mm')

    def test_getDocumentWidth_width_and_viewBox(self):
        # If both are present, width overrides viewBox.
        e = self.new_effect('width="120mm" viewBox="0 0 22 99"')
        self.assertEqual(e.getDocumentWidth(), '120mm')

    def test_getDocumentWidth_viewBox_only(self):
        e = self.new_effect('viewBox="0 0 22 99"')
        self.assertEqual(e.getDocumentWidth(), '22')

    def test_getDocumentWidth_empty_width_and_valid_viewBox(self):
        # An empty width value should be the same as a missing width.
        e = self.new_effect('width="" viewBox="0 0 22 99"')
        self.assertEqual(e.getDocumentWidth(), '22')

    def test_getDocumentWidth_non_zero_viewBox_x_has_no_effect(self):
        # Demonstrate that a non-zero x value (viewbox[0]) does not affect the
        # width value.
        e = self.new_effect('width="" viewBox="5 7 22 99"')
        self.assertEqual(e.getDocumentWidth(), '22')

    #
    # Tests for inkex.Effect.getDocumentHeight()
    #

    def test_getDocumentHeight_no_dimensions(self):
        e = self.new_effect('')
        self.assertEqual(e.getDocumentHeight(), '0')

    def test_getDocumentHeight_empty_height(self):
        # An empty height value should be the same as a missing height.
        e = self.new_effect('height=""')
        self.assertEqual(e.getDocumentHeight(), '0')

    def test_getDocumentHeight_empty_viewBox(self):
        # An empty viewBox value should be the same as a missing viewBox.
        e = self.new_effect('viewBox=""')
        self.assertEqual(e.getDocumentHeight(), '0')

    def test_getDocumentHeight_empty_height_and_empty_viewBox(self):
        # Empty values for both should be the same as both missing.
        e = self.new_effect('height="" viewBox=""')
        self.assertEqual(e.getDocumentHeight(), '0')

    def test_getDocumentHeight_height_only(self):
        e = self.new_effect('height="330px"')
        self.assertEqual(e.getDocumentHeight(), '330px')

    def test_getDocumentHeight_height_and_viewBox(self):
        # If both are present, height overrides viewBox.
        e = self.new_effect('height="330px" viewBox="0 0 22 99"')
        self.assertEqual(e.getDocumentHeight(), '330px')

    def test_getDocumentHeight_viewBox_only(self):
        e = self.new_effect('viewBox="0 0 22 99"')
        self.assertEqual(e.getDocumentHeight(), '99')

    def test_getDocumentHeight_empty_height_and_valid_viewBox(self):
        # An empty height value should be the same as a missing height.
        e = self.new_effect('height="" viewBox="0 0 22 99"')
        self.assertEqual(e.getDocumentHeight(), '99')

    def test_getDocumentHeight_non_zero_viewBox_y_has_no_effect(self):
        # Demonstrate that a non-zero y value (viewbox[1]) does not affect the
        # height value.
        e = self.new_effect('height="" viewBox="5 7 22 99"')
        self.assertEqual(e.getDocumentHeight(), '99')

    #
    # Tests for inkex.Effect.getDocumentUnit()
    #

    def test_getDocumentUnit_no_dimensions(self):
        e = self.new_effect('')
        self.assertEqual(e.getDocumentUnit(), 'px')

    def test_getDocumentUnit_width_only(self):
        e = self.new_effect('width="100mm"')
        # TODO: Determine whether returning 'px' in this case is the
        #     intended behavior.
        self.assertEqual(e.getDocumentUnit(), 'px')

    def test_getDocumentUnit_height_only(self):
        e = self.new_effect('height="100mm"')
        # TODO: Determine whether returning 'px' in this case is the
        #     intended behavior.
        self.assertEqual(e.getDocumentUnit(), 'px')

    def test_getDocumentUnit_viewBox_only(self):
        e = self.new_effect('viewBox="0 0 377 565"')
        self.assertEqual(e.getDocumentUnit(), 'px')

    # Unit-ratio tests. Don't exhaustively test every unit conversion, just
    # demonstrate that the logic works.

    def test_getDocumentUnit_width_and_viewBox_px(self):
        # 100mm is ~377px, so unit should be 'px'.
        e = self.new_effect('width="100mm" viewBox="0 0 377 565"')
        self.assertEqual(e.getDocumentUnit(), 'px')

    def test_getDocumentUnit_width_and_viewBox_in(self):
        # 100mm is ~3.94in, so unit should be 'in'.
        e = self.new_effect('width="100mm" viewBox="0 0 3.94 5.90"')
        self.assertEqual(e.getDocumentUnit(), 'in')

    def test_getDocumentUnit_unitless_width_and_viewBox(self):
        # Unitless width should be treated as 'px'.
        # 3779px is ~1m, so unit should be 'm'.
        e = self.new_effect('width="3779" viewBox="0 0 1 1.5"')
        self.assertEqual(e.getDocumentUnit(), 'm')

    def test_getDocumentUnit_height_ignored_with_viewBox(self):
        # 150mm is ~5.90in, so unit should be 'in', but since height is ignored
        # the returned unit will be 'px'.
        # TODO: Determine whether returning 'px' in this case is the intended
        #     behavior.
        e = self.new_effect('height="150mm" viewBox="0 0 3.94 5.90"')
        self.assertEqual(e.getDocumentUnit(), 'px')

    def test_getDocumentUnit_height_width_and_viewBox(self):
        # 100mm is ~23.6pc, so unit should be 'pc'.
        e = self.new_effect(
                'width="100mm" height="150mm" viewBox="0 0 23.6 35.4"')
        self.assertEqual(e.getDocumentUnit(), 'pc')

    def test_getDocumentUnit_large_error_reverts_to_px(self):
        # 100mm is ~23.6pc; 24.1 is ~2% off from that, so unit should fall back
        # to 'px' instead of using the closest match 'pc'.
        e = self.new_effect('width="100mm" viewBox="0 0 24.1 35.4"')
        self.assertEqual(e.getDocumentUnit(), 'px')

    # Width-parsing tests.

    def test_getDocumentUnit_width_number_parsing(self):
        widths = (
            '100mm',
            '100  mm',
            '   100mm',
            # TODO: Allow whitespace after the units and add a test case.
            #     Currently, '100mm  ' would fail here, because the units will
            #     be treated as 'px'.
            '+100mm',
            '100.0mm',
            '100.0e0mm',
            '10.0e1mm',
            '10.0e+1mm',
            '1000.0e-1mm',
            '.1e+3mm',
            '+.1e+3mm',
        )
        for w in widths:
            # 100mm is ~3.94in, so unit should be 'in'.
            svg = 'width="{}" viewBox="0 0 3.94 5.90"'.format(w)
            e = self.new_effect(svg)
            self.assertEqual(e.getDocumentUnit(), 'in', msg=svg)

    # TODO: Demonstrate that unknown width units are treated as px while
    #     determining the ratio.

    # 100px fallback tests. Although that value is an arbitrary default, it's
    # possible for users of inkex.Effect to depend on this behavior.
    # NOTE: Do not treat the existence of these tests as a reason to preserve
    # the 100px fallback logic.

    def test_getDocumentUnit_bad_width_number_defaults_to_100(self):
        # First, demonstrate that 1in is 2.54cm, so unit should be 'cm'.
        e = self.new_effect('width="1in" viewBox="0 0 2.54 1"')
        self.assertEqual(e.getDocumentUnit(), 'cm')

        # Corrupt the width to contain an invalid number component; note that
        # the units change to 'm'. This is because the corrupt number part is
        # replaced with 100 while the unit is preserved, producing a width of
        # "100in"; 100in is 2.54m.
        e = self.new_effect('width="ABCDin" viewBox="0 0 2.54 1"')
        self.assertEqual(e.getDocumentUnit(), 'm')

    def test_getDocumentUnit_bad_viewbox_entry_defaults_to_100px(self):
        # First, demonstrate that 3779px is 1m, so unit should be 'm'.
        e = self.new_effect('width="3779px" viewBox="0 0 1 1"')
        self.assertEqual(e.getDocumentUnit(), 'm')

        # Corrupt the viewBox to include a non-float value; note that
        # the units change to 'cm'. This is because the corrupt viewBox
        # is effectively replaced with "0 0 100 100"; 3779/100 == 37.79,
        # and 37.79px is 1cm.
        # Also note that the corruption did not touch the width element
        # of the viewBox; this fallback happens if any element is corrupt.
        e = self.new_effect('width="3779px" viewBox="x 0 1 1"')
        self.assertEqual(e.getDocumentUnit(), 'cm')


def new_test_effect(raw_svg):
    """Returns a new inkex.Effect based on raw_svg, with all parsing done."""
    # inkex.Effect doesn't provide a provide a way to supply SVG data without
    # using a file, so use a temporary file. (We could get away with using
    # a monkey-patched sys.stdin if we didn't use inkex.Effect.affect(),
    # but it's important to use the real affect() for testing.)
    # TODO: Modify inkex.Effect to provide a way to inject SVG without using
    #     a file.
    # TODO: Reconcile inkex.Effect.affect() and parse() to simplify the
    #     svg_file/filename/stdin logic; as-is, only the svg_file path works.
    tmp_svg_path = ''
    try:
        # Write the SVG to a temp file that we can refer to by path.
        tf = tempfile.NamedTemporaryFile(
                prefix='inkscape-inkex-test-', delete=False)
        tmp_svg_path = tf.name
        tf.write(raw_svg)
        tf.close()

        # Return an inkex.Effect that has parsed the SVG.
        e = inkex.Effect()
        e.affect(args=[tmp_svg_path], output=False)
        return e
    finally:
        os.remove(tmp_svg_path)


if __name__ == '__main__':
    unittest.main()
