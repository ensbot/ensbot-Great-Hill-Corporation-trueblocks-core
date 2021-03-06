/*-------------------------------------------------------------------------
 * This source code is confidential proprietary information which is
 * copyright (c) 2018, 2019 TrueBlocks, LLC (http://trueblocks.io)
 * All Rights Reserved
 *------------------------------------------------------------------------*/
#include "options.h"

//------------------------------------------------------------------------------------------------
bool COptions::handle_list(void) {
    ENTER8("handle_" + mode);
    nodeNotRequired();

    if (contains(tool_flags, "help")) {
        ostringstream os;
        os << "acctScrape --help";
        LOG_CALL(os.str());
        // clang-format off
        if (system(os.str().c_str())) {}  // Don't remove cruft. Silences compiler warnings
        // clang-format on
        EXIT_NOMSG8(true);
    }

    if (addrs.empty())
        EXIT_USAGE("'chifra list' requires an Ethereum address.");

    CFreshenArray fa;
    for (auto a : addrs)
        fa.push_back(CFreshen(a));
    if (!freshen_internal(FM_PRODUCTION, fa, "", freshen_flags))
        EXIT_FAIL("'chifra list' freshen_internal returned false");

    for (auto addr : addrs) {
        ostringstream os;
        os << "cacheMan "
           << " --fix " << addr << " >&2 && ";
        os << "acctExport --appearances " << tool_flags << " " << addr;
        LOG_CALL(os.str());
        // clang-format off
        if (system(os.str().c_str())) {}  // Don't remove cruft. Silences compiler warnings
        // clang-format on
    }

    EXIT_NOMSG8(true);
}
