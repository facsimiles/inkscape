// SPDX-License-Identifier: GPL-2.0-or-later
#include "node.h"

void Inkscape::XML::Node::setAttribute(Inkscape::Util::const_char_ptr const key,
                                       Inkscape::Util::const_char_ptr value,
                                       bool is_interactive) {
    this->setAttributeImpl(key.ptr, value.ptr, is_interactive);
}
