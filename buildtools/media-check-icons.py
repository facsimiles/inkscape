#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Icon checker: test that icon themes contain all needed icons
# Author: Martin Owens <doctormo@geek-2.com>
# Licensed under GPL version 2 or any later version, read the file "COPYING" for more information.

import fnmatch
import os
import sys

from collections import defaultdict

THEME_PATH = os.path.join('.', 'share', 'icons')
IGNORE_THEMES = [
    'application',
    'Tango',
]
IGNORE_ICONS = [
    # These are hard coded as symbolic in the gtk source code
    'list-add-symbolic.svg',
    'list-add.svg',
    'list-remove-symbolic.svg',
    'list-remove.svg',
    'accessories-character-map.svg',
    'accessories-character-map-symbolic.svg',
    'applications-graphics.svg',
    'applications-graphics-symbolic.svg',
    'dialog-information.svg',
    'dialog-information-symbolic.svg',
    'edit-find.svg',
    'edit-find-symbolic.svg',
    'folder-open.svg',
    'folder-open-symbolic.svg',
    'tools-check-spelling.svg',
    'tools-check-spelling-symbolic.svg',
    'dialog-warning.svg',
    'dialog-warning-symbolic.svg',
    'format-justify-center.svg',
    'format-justify-center-symbolic.svg',
    'format-justify-fill.svg',
    'format-justify-fill-symbolic.svg',
    'format-justify-left.svg',
    'format-justify-left-symbolic.svg',
    'format-justify-right.svg',
    'format-justify-right-symbolic.svg',
    'dialog-warning-symbolic.svg',
    'edit-clear.svg',
    'edit-clear-symbolic.svg',
    'view-refresh-symbolic.svg',
    'view-refresh.svg',
    # Those are illustrations rather than icons
    'feBlend-icon-symbolic.svg',
    'feColorMatrix-icon-symbolic.svg',
    'feComponentTransfer-icon-symbolic.svg',
    'feComposite-icon-symbolic.svg',
    'feConvolveMatrix-icon-symbolic.svg',
    'feDiffuseLighting-icon-symbolic.svg',
    'feDisplacementMap-icon-symbolic.svg',
    'feFlood-icon-symbolic.svg',
    'feGaussianBlur-icon-symbolic.svg',
    'feImage-icon-symbolic.svg',
    'feMerge-icon-symbolic.svg',
    'feMorphology-icon-symbolic.svg',
    'feOffset-icon-symbolic.svg',
    'feSpecularLighting-icon-symbolic.svg',
    'feTile-icon-symbolic.svg',
    'feTurbulence-icon-symbolic.svg',
    'feBlend-icon.svg',
    'feColorMatrix-icon.svg',
    'feComponentTransfer-icon.svg',
    'feComposite-icon.svg',
    'feConvolveMatrix-icon.svg',
    'feDiffuseLighting-icon.svg',
    'feDisplacementMap-icon.svg',
    'feFlood-icon.svg',
    'feGaussianBlur-icon.svg',
    'feImage-icon.svg',
    'feMerge-icon.svg',
    'feMorphology-icon.svg',
    'feOffset-icon.svg',
    'feSpecularLighting-icon.svg',
    'feTile-icon.svg',
    'feTurbulence-icon.svg',
    # Those are UI elements in form of icons; themes may define them, but they shouldn't have to
    'resizing-handle-horizontal-symbolic.svg',
    'resizing-handle-vertical-symbolic.svg',
]

NO_PROBLEM,\
BAD_SYMBOLIC_NAME,\
BAD_SCALABLE_NAME,\
MISSING_FROM,\
ONLY_FOUND_IN = range(5)

def icon_themes():
    for name in os.listdir(THEME_PATH):
        filename = os.path.join(THEME_PATH, name)
        if name in IGNORE_THEMES or not os.path.isdir(filename):
            continue
        yield name, filename

def find_errors_in(themes):
    data = defaultdict(set)
    bad_symbolic = []
    bad_scalable = []
    names = set()

    for name, path in themes:
        for root, dirs, files in os.walk(path):
            orig = root
            root = root[len(path)+1:]
            if '/' not in root:
                continue
            (kind, root) = root.split('/', 1)
            if kind not in ("symbolic", "scalable"):
                continue # Not testing cursors, maybe later.

            theme_name = f"{name}-{kind}"
            names.add(theme_name)
            for fname in files:
                if fname in IGNORE_ICONS:
                    continue
                if not fname.endswith('.svg'):
                    continue

                if kind == "symbolic":
                    if not fname.endswith('-symbolic.svg'):
                        bad_symbolic.append(os.path.join(orig, fname))
                        continue
                    else:
                        # Make filenames consistant for comparison
                        fname = fname.replace('-symbolic.svg', '.svg')
                elif kind == "scalable" and fname.endswith('-symbolic.svg'):
                    bad_scalable.append(os.path.join(orig, fname))
                    continue

                filename = os.path.join(root, fname)
                data[filename].add(theme_name)

    if bad_symbolic:
        yield BAD_SYMBOLIC_NAME, bad_symbolic
    if bad_scalable:
        yield BAD_SCALABLE_NAME, bad_scalable

    only_found_in = defaultdict(list)
    missing_from = defaultdict(list)

    for filename in sorted(data):
        datum = data[filename]
        if datum == names:
            continue
        elif len(datum) == 1:
            only_found_in[list(datum)[0]].append(filename)
            continue
        for theme in (names - datum):
            missing_from[theme].append(filename)

    if only_found_in:
        yield ONLY_FOUND_IN, only_found_in
    if missing_from:
        yield MISSING_FROM, missing_from

if __name__ == '__main__':
    errors = list(find_errors_in(icon_themes()))
    if errors:
        count = 0
        for error, themes in errors:
            if isinstance(themes, list):
                count += len(themes)
            elif isinstance(themes, dict):
                count += sum([len(v) for v in themes.values()])
        sys.stderr.write(f" == {count} problems found in icon themes! == \n\n")
        for error, themes in errors:
            if error is BAD_SCALABLE_NAME:
                sys.stderr.write(f"Scalable themes should not have symbolic icons in them (They end with -symbolic.svg so won't be used):\n")
                for name in themes:
                    sys.stderr.write(f" - {name}\n")
                sys.stderr.write("\n")
            elif error is BAD_SYMBOLIC_NAME:
                sys.stderr.write(f"Symbolic themes should only have symbolic icons in them (They don't end with -symbolic.svg so can't be used):\n")
                for name in themes:
                    sys.stderr.write(f" - {name}\n")
                sys.stderr.write("\n")
            elif error is MISSING_FROM:
                for theme in themes:
                    sys.stderr.write(f"Icons missing from {theme}:\n")
                    for name in themes[theme]:
                        sys.stderr.write(f" - {name}\n")
                    sys.stderr.write("\n")
            elif error is ONLY_FOUND_IN:
                for theme in themes:
                    sys.stderr.write(f"Icons only found in {theme}:\n")
                    for name in themes[theme]:
                        sys.stderr.write(f" + {name}\n")
                    sys.stderr.write("\n")
            else:
                pass

        sys.exit(5)

# vi:sw=4:expandtab:
