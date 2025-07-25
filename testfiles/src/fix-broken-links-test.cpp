// SPDX-License-Identifier: GPL-2.0-or-later

#include "io/fix-broken-links.h"

#include <gtest/gtest.h>

namespace Inkscape {

TEST(FixBrokenLinksTest, SplitPath)
{
#ifdef _WIN32
    EXPECT_EQ(splitPath("C:\\images\\\\.\\file.svg"), (std::vector<std::string>{"C:", "images", "file.svg"}));
#else
    EXPECT_EQ(splitPath("/home/user//./file.svg"), (std::vector<std::string>{"home", "user", "file.svg"}));
#endif
}

} // namespace Inkscape
