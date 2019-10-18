/*-------------------------------------------------------------------------------------------
 * qblocks - fast, easily-accessible, fully-decentralized data from blockchains
 * copyright (c) 2018 Great Hill Corporation (http://greathill.com)
 *
 * This program is free software: you may redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version. This program is
 * distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details. You should have received a copy of the GNU General
 * Public License along with this program. If not, see http://www.gnu.org/licenses/.
 *-------------------------------------------------------------------------------------------*/
#include "options.h"

//---------------------------------------------------------------------------------------------------
static const COption params[] = {
// BEG_CODE_OPTIONS
    COption("trans_list", "", "list<tx_id>", OPT_REQUIRED | OPT_POSITIONAL, "a space-separated list of one or more transaction identifiers (tx_hash, bn.txID, blk_hash.txID)"),
    COption("articulate", "a", "", OPT_SWITCH, "articulate the transactions if an ABI is found for the 'to' address"),
    COption("count_only", "c", "", OPT_SWITCH, "show the number of traces for the transaction only (fast)"),
    COption("no_header", "n", "", OPT_SWITCH, "do not show the header row"),
    COption("skip_ddos", "s", "enum[on*|off]", OPT_HIDDEN | OPT_FLAG, "skip over dDos transactions during export ('on' by default)"),
    COption("fmt", "x", "enum[none|json*|txt|csv|api]", OPT_HIDDEN | OPT_FLAG, "export format"),
    COption("", "", "", OPT_DESCRIPTION, "Retrieve a transaction's traces from the local cache or a running node."),
// END_CODE_OPTIONS
};
static const size_t nParams = sizeof(params) / sizeof(COption);

//---------------------------------------------------------------------------------------------------
bool COptions::parseArguments(string_q& command) {

    if (!standardOptions(command))
        return false;

// BEG_CODE_LOCAL_INIT
    bool no_header = false;
    string_q skip_ddos = "";
// END_CODE_LOCAL_INIT

    Init();
    explode(arguments, command, ' ');
    for (auto arg : arguments) {
        if (false) {
            // do nothing -- make auto code generation easier
// BEG_CODE_AUTO
        } else if (arg == "-a" || arg == "--articulate") {
            articulate = true;

        } else if (arg == "-c" || arg == "--count_only") {
            count_only = true;

        } else if (arg == "-n" || arg == "--no_header") {
            no_header = true;

        } else if (startsWith(arg, "-s:") || startsWith(arg, "--skip_ddos:")) {
            if (!confirmEnum("skip_ddos", skip_ddos, arg))
                return false;

// END_CODE_AUTO
        } else if (startsWith(arg, '-')) {  // do not collapse

            if (!builtInCmd(arg)) {
                return usage("Invalid option: " + arg);
            }

        } else {

            string_q errorMsg;
            if (!wrangleTxId(arg, errorMsg))
                return usage(errorMsg);
            string_q ret = transList.parseTransList(arg);
            if (!ret.empty())
                return usage(ret);
        }
    }

    // Data wrangling
    if (!transList.hasTrans())
        return usage("Please specify at least one transaction identifier.");

    if (isRaw)
        exportFmt = JSON1;

    if (articulate) {
        // show certain fields and hide others
        manageFields(defHide, false);
        manageFields(defShow, true);
        manageFields("CParameter:strDefault", false);  // hide
        manageFields("CTransaction:price", false);  // hide
        manageFields("CFunction:outputs", true);  // show
        manageFields("CTransaction:input", true);  // show
        manageFields("CLogEntry:topics", true);  // show
        abi_spec.loadAbiKnown("all");
    }

    // Display formatting
    string_q format;
    switch (exportFmt) {
        case NONE1:
        case TXT1:
        case CSV1:
            format = getGlobalConfig("getTrace")->getConfigStr("display", "format", format.empty() ? STR_DISPLAY_TRACE : format);
            manageFields("CTransaction:" + cleanFmt(format, exportFmt));
            manageFields("CTrace:" + cleanFmt(format, exportFmt));
            manageFields("CTraceAction:" + substitute(cleanFmt(format, exportFmt), "ACTION::", ""));
            manageFields("CTraceResult:" + substitute(cleanFmt(format, exportFmt), "RESULT::", ""));
            break;
        case API1:
        case JSON1:
            format = "";
            break;
    }
    expContext().fmtMap["format"] = expContext().fmtMap["header"] = cleanFmt(format, exportFmt);
    if (count_only)
        expContext().fmtMap["format"] = expContext().fmtMap["header"] = "[{HASH}]\t[{TRACESCNT}]";
    if (no_header)
        expContext().fmtMap["header"] = "";

    skipDdos = getGlobalConfig("acctExport")->getConfigBool("settings", "skipDdos", (skip_ddos == "off" ? false : true));;

    return true;
}

//---------------------------------------------------------------------------------------------------
void COptions::Init(void) {
    registerOptions(nParams, params);
    optionOn(OPT_RAW | OPT_OUTPUT);

// BEG_CODE_INIT
    articulate = false;
    count_only = false;
// END_CODE_INIT

    skipDdos = false;
    transList.Init();
}

//---------------------------------------------------------------------------------------------------
COptions::COptions(void) {
    setSorts(GETRUNTIME_CLASS(CBlock), GETRUNTIME_CLASS(CTransaction), GETRUNTIME_CLASS(CReceipt));
    Init();
    first = true;
}

//--------------------------------------------------------------------------------
COptions::~COptions(void) {
}

//--------------------------------------------------------------------------------
string_q COptions::postProcess(const string_q& which, const string_q& str) const {
    if (which == "options") {
        return substitute(str, "trans_list", "<tx_id> [tx_id...]");

    } else if (which == "notes" && (verbose || COptions::isReadme)) {

        string_q ret;
        ret += "[{trans_list}] is one or more space-separated identifiers which may be either a transaction hash,|"
                "a blockNumber.transactionID pair, or a blockHash.transactionID pair, or any combination.\n";
        ret += "This tool checks for valid input syntax, but does not check that the transaction requested exists.\n";
        ret += "This tool retrieves information from the local node or rpcProvider if configured "
                    "(see documentation).\n";
        ret += "If the queried node does not store historical state, the results may be undefined.\n";
        return ret;
    }
    return str;
}
