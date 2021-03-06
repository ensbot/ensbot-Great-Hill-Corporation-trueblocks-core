/*-------------------------------------------------------------------------------------------
 * qblocks - fast, easily-accessible, fully-decentralized data from blockchains
 * copyright (c) 2018, 2019 TrueBlocks, LLC (http://trueblocks.io)
 *
 * This program is free software: you may redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version. This program is
 * distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details. You should have received a copy of the GNU General
 * Public License along with this program. If not, see http://www.gnu.org/licenses/.
 *-------------------------------------------------------------------------------------------*/
#include "basetypes.h"
#include "database.h"
#include "exportcontext.h"
#include "options_base.h"
#include "colors.h"
#include "filenames.h"
#include "accountname.h"
#include "rpcresult.h"

namespace qblocks {

//--------------------------------------------------------------------------------
void COptionsBase::registerOptions(size_t nP, COption const* pP) {
    // TODO(tjayrush): global data
    arguments.clear();
    cntParams = nP;
    pParams = pP;
}

//--------------------------------------------------------------------------------
// TODO(tjayrush): global data - but okay, a program only has one name
string_q COptionsBase::g_progName = "quickBlocks";

//--------------------------------------------------------------------------------
void COptionsBase::setProgName(const string_q& name) {
    g_progName = name;
}

//--------------------------------------------------------------------------------
string_q COptionsBase::getProgName(void) const {
    return g_progName;
}

//--------------------------------------------------------------------------------
bool COptionsBase::prepareArguments(int argCountIn, const char* argvIn[]) {
    if (argCountIn > 0)  // always is, but check anyway
        COptionsBase::g_progName = CFilename(argvIn[0]).getFilename();
    if (!getEnvStr("PROG_NAME").empty())
        COptionsBase::g_progName = getEnvStr("PROG_NAME");
    if (getEnvStr("NO_COLOR") == "true")
        colorsOff();
    if (getEnvStr("REDIR_CERR") == "true")
        cerr.rdbuf(cout.rdbuf());

    CStringArray separatedArgs;
    for (int i = 1; i < argCountIn; i++) {  // skip argv[0]
        CStringArray parts;
        string_q arg = substituteAny(argvIn[i], "\n\r\t", " ");
        explode(parts, arg, ' ');
        for (auto part : parts) {
            if (!part.empty()) {
                separatedArgs.push_back(part);
            }
        }
    }

    if (isTestMode()) {
        size_t cnt = 0;
        ostringstream os;
        cerr << getProgName() << " argc: " << (separatedArgs.size() + 1) << " ";
        for (auto arg : separatedArgs) {
            cerr << "[" << ++cnt << ":" << trim(arg) << "] ";
            os << trim(arg) << " ";
        }
        cerr << endl;
        cerr << getProgName() << " " << os.str() << endl;
    }

    //        // send out the environment, if any non-default
    //        if (isTestMode() || isLevelOn(sev_debug4)) {
    //            if (isApiMode())                        { LOG4("API_MODE=",    getEnvStr("API_MODE")); }
    //            if (getEnvStr("DOCKER_MODE") == "true") { LOG4("DOCKER_MODE=", getEnvStr("DOCKER_MODE")); }
    //            if (!getEnvStr("IPFS_PATH").empty())    { LOG4("IPFS_PATH=",   getEnvStr("IPFS_PATH")); }
    //        }

    CStringArray argumentsIn;
    for (auto arg : separatedArgs) {
        replace(arg, "--verbose", "-v");
        while (!arg.empty()) {
            string_q opt = expandOption(arg);  // handles case of -rf for example
            if (isReadme && isEnabled(OPT_HELP))
                return usage();
            if (opt == "-")
                return usage("Raw '-' not supported.");
            else if (!opt.empty())
                argumentsIn.push_back(opt);  // args[nArgs++] = opt;
        }
    }
    if (argumentsIn.size() < minArgs)  // the first arg is the program's name, so we use <=
        return usage("Not enough arguments presented.");

    //        string_q stdInCmds;
    //        if (hasStdIn) {
    //            // reading from stdin, expect only a list of addresses, one per line.
    //            char c = static_cast<char>(getchar());
    //            while (c != EOF) {
    //                stdInCmds += c;
    //                c = static_cast<char>(getchar());
    //            }
    //            if (!endsWith(stdInCmds, "\n"))
    //                stdInCmds += "\n";
    //        }

    //-----------------------------------------------------------------------------------
    // Collapse commands that have 'permitted' sub options (i.e. colon ":" args)
    //-----------------------------------------------------------------------------------
    CStringArray argumentsOut;
    for (size_t i = 0; i < argumentsIn.size(); i++) {
        string_q arg = argumentsIn[i];
        bool combine = false;
        for (size_t j = 0; j < cntParams && !combine; j++) {
            if (!pParams[j].permitted.empty()) {
                string_q shortName = pParams[j].shortName;
                string_q longName = pParams[j].longName;
                if (shortName == arg || startsWith(longName, arg)) {
                    // We want to pull the next parameter into this one since it's a ':' param
                    combine = true;
                }
            }
        }

        if (!combine && (arg == "-v" || arg == "-verbose" || arg == "--verbose")) {
            if (i < argumentsIn.size() - 1) {
                uint64_t n = str_2_Uint(argumentsIn[i + 1]);
                if (n > 0 && n <= 10) {
                    // We want to pull the next parameter into this one since it's a ':' param
                    combine = true;
                }
            }
            if (!combine) {
                arg = "--verbose:1";
            }
        }

        if (!combine && (arg == "-x" || arg == "--fmt")) {
            if (i < argumentsIn.size() - 1)
                combine = true;
        }

        if (combine && i < (argumentsIn.size() - 1)) {
            argumentsOut.push_back(arg + ":" + argumentsIn[i + 1]);
            i++;
        } else {
            argumentsOut.push_back(arg);
        }
    }

    //-----------------------------------------------------------------------------------
    // We now have 'nArgs' command line arguments stored in the array 'args.'  We spin
    // through them doing one of two things
    //
    // (1) handle any arguments common to all programs and remove them from the array
    // (2) identify any --file arguments and store them for later use
    //-----------------------------------------------------------------------------------
    string_q cmdFileName = "";
    for (uint64_t i = 0; i < argumentsOut.size(); i++) {
        string_q arg = argumentsOut[i];
        if (startsWith(arg, "--file:")) {
            cmdFileName = substitute(arg, "--file:", "");
            replace(cmdFileName, "~/", getHomeFolder());
            if (!fileExists(cmdFileName)) {
                return usage("--file: '" + cmdFileName + "' not found. Quitting.");
            }

        } else if (startsWith(arg, "-v:") || startsWith(arg, "--verbose:")) {
            verbose = true;
            arg = substitute(substitute(arg, "-v:", ""), "--verbose:", "");
            if (!arg.empty()) {
                if (!isUnsigned(arg))
                    return usage("Invalid verbose level '" + arg + "'. Quitting...");
                verbose = str_2_Uint(arg);
            }

        } else if (startsWith(arg, "-x:") || startsWith(arg, "--fmt:")) {
            arg = substitute(substitute(arg, "--fmt:", ""), "-x:", "");
            if (arg == "txt") {
                exportFmt = TXT1;
            } else if (arg == "csv") {
                exportFmt = CSV1;
            } else if (arg == "json") {
                exportFmt = JSON1;
            } else if (arg == "api") {
                exportFmt = API1;
            } else {
                return usage("Export format (" + arg + ") must be one of [ json | txt | csv | api ]. Quitting...");
            }
            argumentsOut[i] = "";
        }
    }

    // remove empty arguments
    CStringArray argumentsOut3;
    for (auto arg : argumentsOut)
        if (!arg.empty())
            argumentsOut3.push_back(arg);

    // If we have a command file, we will use it, if not we will creat one and pretend we have one.
    string_q commandList = "";
    for (auto arg : argumentsOut3) {
        if (!contains(arg, "--file:"))
            commandList += (arg + " ");
    }
    commandList += '\n';

    if (!cmdFileName.empty()) {
        string_q toAll;
        if (!commandList.empty())
            toAll = (" " + substitute(commandList, "\n", ""));
        commandList = "";
        // The command line also has a --file in it, so add these commands as well
        string_q contents;
        asciiFileToString(cmdFileName, contents);
        cleanString(contents, false);
        if (contents.empty())
            return usage("Command file '" + cmdFileName + "' is empty. Quitting...");
        if (startsWith(contents, "NOPARSE\n")) {
            commandList = contents;
            nextTokenClear(commandList, '\n');
            commandList += toAll;
        } else {
            CStringArray lines;
            explode(lines, contents, '\n');
            for (auto command : lines) {
                if (!command.empty() && !startsWith(command, ";")) {  // ignore comments
                    commandList += (command + toAll + "\n");
                }
            }
        }
    }
    //        commandList += stdInCmds;
    explode(commandLines, commandList, '\n');
    for (auto& item : commandLines)
        item = trim(item);
    if (commandLines.empty())
        commandLines.push_back("--noop");

    return 1;
}

//--------------------------------------------------------------------------------
bool COptionsBase::standardOptions(string_q& cmdLine) {
    // Note: check each item individual in case more than one appears on the command line
    cmdLine += " ";
    replace(cmdLine, "--output ", "--output:");

    if (contains(cmdLine, "--noop ")) {
        // do nothing
        replaceAll(cmdLine, "--noop ", "");
    }

    if (contains(cmdLine, "--version ")) {
        cerr << getProgName() << " " << getVersionStr() << "\n";
        return false;
    }

    if (contains(cmdLine, "--nocolor ")) {
        replaceAll(cmdLine, "--nocolor ", "");
        colorsOff();
    }

    if (contains(cmdLine, "--no_header ")) {
        replaceAll(cmdLine, "--no_header ", "");
        isNoHeader = true;
    }

    if (isEnabled(OPT_HELP) && (contains(cmdLine, "-h ") || contains(cmdLine, "--help "))) {
        usage();
        return false;
    }

    if (isEnabled(OPT_ETHER) && contains(cmdLine, "--ether ")) {
        replaceAll(cmdLine, "--ether ", "");
        expContext().asEther = true;
        expContext().asDollars = false;
        expContext().asWei = false;
    }

    if (isEnabled(OPT_RAW) && contains(cmdLine, "--very_raw ")) {
        replaceAll(cmdLine, "--very_raw ", "");
        isRaw = true;
        isVeryRaw = true;
    }

    if (isEnabled(OPT_RAW) && contains(cmdLine, "--raw ")) {
        replaceAll(cmdLine, "--raw ", "");
        isRaw = true;
    }

    if (isEnabled(OPT_OUTPUT) && contains(cmdLine, "--output:")) {
        redirFilename = substitute(cmdLine, "--output:", "|");
        nextTokenClear(redirFilename, '|');
        redirFilename = nextTokenClear(redirFilename, ' ');
        if (redirFilename.empty())
            return usage("Please provide a filename for the --output option. Quitting...");
        if (!isTestMode() && !startsWith(redirFilename, '/'))
            return usage("Output file (" + redirFilename + ") must be a fully qualified path. Quitting...");
        establishFolder(redirFilename);
        ASSERT(!folderExists(outputFn));
        redirStream.open(redirFilename.c_str());
        if (redirStream.is_open()) {
            coutBackup = cout.rdbuf();        // back up cout's streambuf
            cout.rdbuf(redirStream.rdbuf());  // assign streambuf to cout
        }
    }

    if (isEnabled(OPT_WEI) && contains(cmdLine, "--wei ")) {
        replaceAll(cmdLine, "--wei ", "");
        expContext().asEther = false;
        expContext().asDollars = false;
        expContext().asWei = true;
    }

    if (isEnabled(OPT_DOLLARS) && contains(cmdLine, "--dollars ")) {
        replaceAll(cmdLine, "--dollars ", "");
        expContext().asEther = false;
        expContext().asDollars = true;
        expContext().asWei = false;
    }

    if (isEnabled(OPT_PARITY) && contains(cmdLine, "--parity ")) {
        replaceAll(cmdLine, "--parity ", "");
        expContext().spcs = 2;
        expContext().hexNums = true;
        expContext().quoteNums = true;
        expContext().isParity = true;
        for (int i = 0; i < 5; i++)
            if (sorts[i])
                sorts[i]->sortFieldList();
    }

    // A final set of options that do not have command line options
    if (isEnabled(OPT_PREFUND))
        if (!loadPrefunds())
            return usage("Could not open prefunds data. Quitting...");

    cmdLine = trim(cmdLine);
    return true;
}

//--------------------------------------------------------------------------------
bool COptionsBase::builtInCmd(const string_q& arg) {
    if (isEnabled(OPT_HELP) && (arg == "-h" || arg == "--help"))
        return true;

    if (isEnabled(OPT_VERBOSE)) {
        if (startsWith(arg, "-v:") || startsWith(arg, "--verbose:"))
            return true;
    }

    if (isEnabled(OPT_FMT)) {
        if (startsWith(arg, "-x:") || startsWith(arg, "--fmt:"))
            return true;
    }

    if (isEnabled(OPT_ETHER) && arg == "--ether")
        return true;
    if (isEnabled(OPT_RAW) && arg == "--raw")
        return true;
    if (isEnabled(OPT_OUTPUT) && startsWith(arg, "--output:"))
        return true;
    if (isEnabled(OPT_RAW) && arg == "--very_raw")
        return true;
    if (isEnabled(OPT_WEI) && arg == "--wei")
        return true;
    if (isEnabled(OPT_DOLLARS) && arg == "--dollars")
        return true;
    if (isEnabled(OPT_PARITY) && (arg == "--parity"))
        return true;
    if (arg == "--version")
        return true;
    if (arg == "--nocolor")
        return true;
    if (arg == "--noop")
        return true;
    return false;
}

//-----------------------------------------------------------------------
void COptionsBase::configureDisplay(const string_q& tool, const string_q& dataType, const string_q& defFormat,
                                    const string_q& meta) {
    string_q format;
    switch (exportFmt) {
        case NONE1:
            format = defFormat;
            manageFields(dataType + ":" + cleanFmt(format, exportFmt));
            break;
        case TXT1:
        case CSV1:
            format = getGlobalConfig(tool)->getConfigStr("display", "format", format.empty() ? defFormat : format);
            manageFields(dataType + ":" + cleanFmt((format.empty() ? defFormat : format), exportFmt));
            break;
        case API1:
        case JSON1:
            format = "";
            break;
    }
    if (expContext().asEther)
        format = substitute(format, "{BALANCE}", "{ETHER}");
    if (expContext().asDollars)
        format = substitute(format, "{BALANCE}", "{DOLLARS}");
    expContext().fmtMap["meta"] = meta;
    expContext().fmtMap["format"] = cleanFmt(format, exportFmt);
    expContext().fmtMap["header"] = cleanFmt(format, exportFmt);
    if (isNoHeader)
        expContext().fmtMap["header"] = "";
}

//---------------------------------------------------------------------------------------------------
bool COptionsBase::confirmUint(const string_q& name, uint64_t& value, const string_q& argIn) const {
    value = NOPOS;

    const COption* param = findParam(name);
    if (!param)
        return usage("Unknown parameter `" + name + "'. Quitting...");
    if (!contains(param->type, "uint") && !contains(param->type, "blknum"))
        return true;

    string_q arg = argIn;
    replace(arg, param->shortName + ":", "");
    replace(arg, name + ":", "");
    replaceAll(arg, "-", "");

    if (!isNumeral(arg))
        return usage("Value to --" + name + " parameter (" + arg + ") must be a valid unsigned integer. Quitting...");
    value = str_2_Uint(arg);
    return true;
}

//---------------------------------------------------------------------------------------------------
bool COptionsBase::confirmBlockNum(const string_q& name, blknum_t& value, const string_q& argIn,
                                   blknum_t latest) const {
    if (!confirmUint(name, value, argIn))
        return false;
    if (value > latest)
        return usage("Block number (" + argIn + ") is greater than the latest block. Quitting...");
    return true;
}

//---------------------------------------------------------------------------------------------------
bool COptionsBase::confirmEnum(const string_q& name, string_q& value, const string_q& argIn) const {
    const COption* param = findParam(name);
    if (!param)
        return usage("Unknown parameter `" + name + "'. Quitting...");
    if (param->type.empty() || !contains(param->type, "enum["))
        return true;

    string_q type = param->type;
    replace(type, "*", "");
    replace(type, "enum", "");
    replace(type, "list<", "");
    replace(type, ">", "");
    replace(type, "[", "|");
    replace(type, "]", "|");

    string_q arg = argIn;
    replace(arg, param->shortName + ":", "");
    replace(arg, name + ":", "");
    replaceAll(arg, "-", "");

    if (!contains(type, "|" + arg + "|")) {
        string_q desc = substitute(substitute(param->description, ", one ", "| One "), "*", "");
        nextTokenClear(desc, '|');
        return usage("Invalid option '" + arg + "' for '" + name + "'." + desc + " required. Quitting...");
    }

    value = arg;
    return true;
}

//---------------------------------------------------------------------------------------------------
const COption* COptionsBase::findParam(const string_q& name) const {
    for (size_t i = 0; i < cntParams; i++) {
        if (startsWith(pParams[i].longName, "--" + name))  // flags, toggles, switches
            return &pParams[i];
        if (startsWith(pParams[i].longName, name))  // positionals
            return &pParams[i];
    }
    return NULL;
}

//--------------------------------------------------------------------------------
COption::COption(const string_q& ln, const string_q& sn, const string_q& t, size_t opts, const string_q& d) {
    description = substitute(d, "&#44;", ",");
    if (ln.empty())
        return;

    is_positional = (opts & OPT_POSITIONAL);
    is_hidden = (opts & OPT_HIDDEN);
    is_optional = !(opts & OPT_REQUIRED);

    type = t;
    permitted = t;
    permitted = substitute(permitted, "<uint32>", "<num>");
    permitted = substitute(permitted, "<uint64>", "<num>");
    permitted = substitute(permitted, "<blknum>", "<num>");
    permitted = substitute(permitted, "<string>", "<str>");
    if (contains(type, "enum")) {
        description += ", one [X] of " + substitute(substitute(substitute(type, "list<", ""), ">", ""), "enum", "");
        replace(description, " [X]", (contains(type, "list") ? " or more" : ""));
        permitted = "<val>";
    }

    longName = "--" + ln + (permitted.empty() ? "" : " " + permitted);
    shortName = (sn.empty() ? "" : "-" + sn);
    if (is_positional)
        longName = shortName = ln;
}

//--------------------------------------------------------------------------------
bool COptionsBase::usage(const string_q& errMsg) const {
    cerr << usageStr(errMsg);
    return false;
}

const char* STR_ERROR_JSON = "{ \"errors\": [ \"[ERRORS]\" ] }\n";

//--------------------------------------------------------------------------------
string_q COptionsBase::usageStr(const string_q& errMsg) const {
    if (isApiMode())
        cout << substitute(STR_ERROR_JSON, "[ERRORS]", getProgName() + " - " + errMsg);

    ostringstream os;
    if (isReadme) {
        colorsOff();
        os << "#### Usage\n";
    }

    os << "\n";
    if (!errMsg.empty())
        os << cRed << "  " << errMsg << cOff << "\n\n";
    os << hiUp1 << "Usage:" << hiDown << "    " << getProgName() << " " << options() << "  \n";
    os << purpose();
    os << descriptions() << "\n";
    os << get_notes();
    if (!isReadme) {
        os << bBlue << "  Powered by QBlocks";
        os << (isTestMode() ? "" : " (" + getVersionStr() + ")") << "\n" << cOff;
    }
    string_q ret = os.str().c_str();
    return ret;
}

//--------------------------------------------------------------------------------
string_q COptionsBase::options(void) const {
    string_q positional;

    ostringstream os;
    os << "[";
    for (uint64_t i = 0; i < cntParams; i++) {
        if (pParams[i].is_positional) {
            positional += (" " + pParams[i].longName);

        } else if (pParams[i].is_hidden) {
            // invisible option

        } else if (!pParams[i].shortName.empty()) {
            os << pParams[i].shortName << "|";
        }
    }
    if (isEnabled(OPT_VERBOSE))
        os << "-v|";
    if (isEnabled(OPT_HELP))
        os << "-h";
    os << "]";

    replaceAll(positional, "addrs2 blocks", "<address> <address> [address...] [block...]");
    replaceAll(positional, "addrs blocks", "<address> [address...] [block...]");
    replaceAll(positional, "block_list", "< block | date > [ block... | date... ]");
    replaceAll(positional, "transactions", "<tx_id> [tx_id...]");
    replaceAll(positional, "blocks", "<block> [block...]");
    replaceAll(positional, "addrs", "<address> [address...]");
    replaceAll(positional, "files", "<file> [file...]");
    replaceAll(positional, "terms", "<term> [term...]");
    replaceAll(positional, "modes", "<mode> [mode...]");
    if (isReadme)
        positional = substitute(substitute(positional, "<", "&lt;"), ">", "&gt;");
    os << positional;

    return os.str();
}

//--------------------------------------------------------------------------------
string_q COptionsBase::purpose(void) const {
    ostringstream os;
    os << hiUp1 << "Purpose:" << hiDown << "  ";
    string_q purpose;
    for (size_t p = 0; p < cntParams; p++)
        if (pParams[p].longName.empty())  // program description
            purpose = pParams[p].description;
    os << substitute(purpose, "\n", "\n        ") << "\n";
    if (!endsWith(purpose, "\n"))
        os << "\n";
    return os.str();
}

//--------------------------------------------------------------------------------
const char* STR_ONE_LINE = "| {S} | {L} | {D} |\n";

string_q COptionsBase::oneDescription(const string_q& sN, const string_q& lN, const string_q& d, bool isPositional,
                                      bool required) const {
    ostringstream os;
    if (isReadme) {
        // When we are writing the readme file...
        string_q line = STR_ONE_LINE;
        replace(line, "{S}", sN);
        replace(line, "{L}", lN);
        replace(line, "{D}", substitute((d + (required && isPositional ? " (required)" : "")), "|", "&#124;"));
        os << line;

    } else {
        // When we are writing to the command line...
        string_q line = "\t" + substitute(substitute(string_q(STR_ONE_LINE), " ", ""), "|", "");
        replace(line, "{S}", (isPositional ? "" : padRight(sN, 3)));
        if (isPositional) {
            replace(line, "{L}", padRight(lN, 22));
        } else {
            replace(line, "{L}", padRight((lN.empty() ? "" : " (" + lN + ")"), 19));
        }
        replace(line, "{D}", d + (required && isPositional ? " (required)" : ""));
        os << line;
    }
    return os.str();
}

//--------------------------------------------------------------------------------
string_q COptionsBase::get_notes(void) const {
    if ((!isReadme && !verbose) || (notes.size() == 0))
        return "";

    string_q nn;
    for (auto n : notes)
        nn += (n + "\n");

    string_q lead = (isReadme ? "" : "\t");
    string_q trail = (isReadme ? "\n" : "\n");
    while (!isReadme && contains(nn, '`')) {
        replace(nn, "`", hiUp2);
        replace(nn, "`", hiDown);
    }

    ostringstream os;
    os << hiUp1 << "Notes:" << hiDown << "\n" << (isReadme ? "\n" : "");
    while (!nn.empty()) {
        string_q line = substitute(nextTokenClear(nn, '\n'), "|", "\n" + lead + " ");
        os << lead << "- " << line << "\n";
    }
    os << "\n";
    return substitute(os.str(), "-   ", "  - ");
}

//--------------------------------------------------------------------------------
string_q COptionsBase::descriptions(void) const {
    ostringstream os;
    os << hiUp1 << "Where:" << hiDown << "  \n";
    if (isReadme) {
        os << "\n";
        os << "| Short Cut | Option | Description |\n";
        os << "| -------: | :------- | :------- |\n";
    }

    size_t nHidden = 0;
    for (uint64_t i = 0; i < cntParams; i++) {
        string_q sName = pParams[i].shortName;
        string_q lName = substitute(pParams[i].longName, "addrs2", "addrs");
        string_q descr = trim(pParams[i].description);
        bool isPositional = pParams[i].is_positional;
        if (!pParams[i].is_hidden && !sName.empty()) {
            bool isReq = !pParams[i].is_optional;
            sName = (isPositional ? "" : sName);
            lName = substitute(substitute((isPositional ? substitute(lName, "-", "") : lName), "!", ""), "~", "");
            os << oneDescription(sName, lName, descr, isPositional, isReq);
        }
        if (pParams[i].is_hidden)
            nHidden++;
    }

    // For testing purposes, we show the hidden options
    if (isTestMode() && nHidden) {
        os << "\n#### Hidden options (shown during testing only)\n";
        for (uint64_t i = 0; i < cntParams; i++) {
            string_q sName = pParams[i].shortName;
            string_q lName = pParams[i].longName;
            string_q descr = trim(pParams[i].description);
            bool isPositional = pParams[i].is_positional;
            if (pParams[i].is_hidden && !sName.empty()) {
                bool isReq = !pParams[i].is_optional;
                lName = substitute(substitute((isPositional ? substitute(lName, "-", "") : lName), "!", ""), "~", "");
                lName = substitute(lName, "@-", "");
                sName = (isPositional ? "" : pParams[i].shortName);
                os << oneDescription(sName, lName, descr, isPositional, isReq);
            }
        }
        os << "#### Hidden options (shown during testing only)\n\n";
    }

    if (isEnabled(OPT_FMT) && (verbose || isTestMode()))
        os << oneDescription("-x", "--fmt <val>", "export format, one of [none|json*|txt|csv|api]");
    if (isEnabled(OPT_VERBOSE))
        os << oneDescription("-v", "--verbose", "set verbose level. Either -v, --verbose or -v:n where 'n' is level");
    if (isEnabled(OPT_HELP))
        os << oneDescription("-h", "--help", "display this help screen");
    return os.str();
}

//--------------------------------------------------------------------------------
string_q COptionsBase::expandOption(string_q& arg) {
    string_q ret = arg;

    // Check that we don't have a regular command with a single dash, which
    // should report an error in client code
    for (uint64_t i = 0; i < cntParams; i++) {
        if (pParams[i].longName == arg) {
            arg = "";
            return ret;
        }
    }

    // Not an option
    if (!startsWith(arg, '-') || startsWith(arg, "--")) {
        arg = "";
        return ret;
    }

    // Stdin case
    if (arg == "-") {
        arg = "";
        return ret;
    }

    // Single option
    if (arg.length() == 2) {
        arg = "";
        return ret;
    }

    // Special case
    if (arg == "-th" || arg == "-ht") {
        isReadme = true;
        hiUp1 = hiUp2 = hiDown = '`';
        arg = "";
        replaceAll(ret, "-th", "");
        replaceAll(ret, "-ht", "");
        if (!isEnabled(OPT_HELP))
            ret = "-h";
        return ret;
    }

    // This may be a command with two -a -b (or more) single options
    if (arg.length() > 2 && arg[2] == ' ') {
        ret = extract(arg, 0, 2);
        arg = extract(arg, 3);
        return ret;
    }

    // One of the range commands. These must be alone on
    // the line (this is a bug for -rf:txt for example)
    if (contains(arg, ":") || contains(arg, "=")) {
        arg = "";
        return ret;
    }

    // This is a ganged-up option. We need to pull it apart by returning
    // the first two chars, and saving the rest for later.
    ret = extract(arg, 0, 2);
    arg = "-" + extract(arg, 2);
    return ret;
}

//--------------------------------------------------------------------------------
int sortParams(const void* c1, const void* c2) {
    const COption* p1 = reinterpret_cast<const COption*>(c1);
    const COption* p2 = reinterpret_cast<const COption*>(c2);
    if (p1->shortName == "-h")
        return 1;
    else if (p2->shortName == "-h")
        return -1;
    return p1->shortName.compare(p2->shortName);
}

//--------------------------------------------------------------------------------
uint64_t verbose = false;

//---------------------------------------------------------------------------------------------------
string_q configPath(const string_q& part) {
    return getHomeFolder() + ".quickBlocks/" + part;
}

//------------------------------------------------------------------
void editFile(const string_q& fileName) {
    CToml toml(configPath("quickBlocks.toml"));
    string_q editor = toml.getConfigStr("dev", "editor", "<NOT_SET>");
    if (!isTestMode() && editor == "<NOT_SET>") {
        editor = getEnvStr("EDITOR");
        if (editor.empty()) {
            cerr << endl;
            cerr << cTeal << "\tWarning: " << cOff;
            cerr << "$EDITOR environment setting not found. Either export it or\n";
            cerr << "\tadd an \"[settings] editor=\" value in the config file." << endl << endl;
            return;
        }
    }

    CFilename fn(fileName);
    string_q cmd = "cd " + fn.getPath() + " ; " + editor + " \"" + fn.getFilename() + "\"";
    if (isTestMode()) {
        cerr << "Testing editFile: " << fn.getFilename() << "\n";
        string_q contents;
        asciiFileToString(fileName, contents);
        cout << contents << "\n";
    } else {
        // clang-format off
        if (system(cmd.c_str())) {}  // Don't remove cruft. Silences compiler warnings
        // clang-format on
    }
}

//-------------------------------------------------------------------------
bool COptionsBase::isEnabled(uint32_t q) const {
    return (enableBits & q);
}

void COptionsBase::optionOff(uint32_t q) {
    enableBits &= (~q);
}

void COptionsBase::optionOn(uint32_t q) {
    enableBits |= q;
}

//--------------------------------------------------------------------------------
int sortByBlockNum(const void* v1, const void* v2) {
    const CNameValue* b1 = reinterpret_cast<const CNameValue*>(v1);
    const CNameValue* b2 = reinterpret_cast<const CNameValue*>(v2);
    if (b1->first == "latest")
        return 1;
    if (b2->first == "latest")
        return -1;
    if (contains(b1->second, "tbd") && contains(b1->second, "tbd"))
        return b1->second.compare(b2->second);
    if (contains(b1->second, "tbd"))
        return 1;
    if (contains(b2->second, "tbd"))
        return -1;
    return static_cast<int>(str_2_Uint(b1->second) - str_2_Uint(b2->second));
}

//-----------------------------------------------------------------------
const CToml* getGlobalConfig(const string_q& name) {
    static CToml* toml = NULL;
    static string_q components = "quickBlocks|";

    if (name == "reload" && toml) {
        toml->clear();
        toml = NULL;
    }

    if (!toml) {
        static CToml theToml("");

        // Forces a reload
        theToml.clear();
        theToml.setFilename(configPath("quickBlocks.toml"));
        theToml.readFile(configPath("quickBlocks.toml"));
        toml = &theToml;

        // Always load the program's custom config if it exists
        string_q fileName = configPath(COptionsBase::g_progName + ".toml");
        if (fileExists(fileName) && !contains(components, COptionsBase::g_progName + "|")) {
            components += COptionsBase::g_progName + "|";
            CToml custom(fileName);
            toml->mergeFile(&custom);
        }
    }

    // If we're told explicitly to load another config, do that here
    if (!name.empty() && name != "reload") {
        string_q fileName = configPath(name + ".toml");
        if (fileExists(fileName) && !contains(components, name + "|")) {
            components += name + "|";
            CToml custom(fileName);
            toml->mergeFile(&custom);
        }
    }

    return toml;
}

//-----------------------------------------------------------------------
static bool sortByValue(const CNameValue& p1, const CNameValue& p2) {
    blknum_t b1 = str_2_Uint(p1.second);
    blknum_t b2 = str_2_Uint(p2.second);
    if (b1 == 0) {
        if (p1.first == "latest")
            b1 = NOPOS;
        if (p1.first == "constantinople")
            b1 = (NOPOS - 2);
    }
    if (b2 == 0) {
        if (p2.first == "latest")
            b2 = NOPOS;
        if (p2.first == "constantinople")
            b2 = (NOPOS - 2);
    }
    return b1 < b2;
}

//-----------------------------------------------------------------------
// TODO(tjayrush): global data
CNameValueArray COptionsBase::specials;

//-----------------------------------------------------------------------
void COptionsBase::loadSpecials(void) {
    specials.clear();
    extern const char* STR_DEFAULT_WHENBLOCKS;
    string_q specialsStr = STR_DEFAULT_WHENBLOCKS;
    CKeyValuePair keyVal;
    while (keyVal.parseJson3(specialsStr)) {
        CNameValue pair = make_pair(keyVal.jsonrpc, keyVal.result);
        specials.push_back(pair);
        keyVal = CKeyValuePair();  // reset
    }
    sort(specials.begin(), specials.end(), sortByValue);
    return;
}

//--------------------------------------------------------------------------------
bool COptionsBase::forEverySpecialBlock(NAMEVALFUNC func, void* data) {
    if (!func)
        return false;
    if (specials.size() == 0)
        loadSpecials();
    for (auto special : specials)
        if (!(*func)(special, data))
            return false;
    return true;
}

//--------------------------------------------------------------------------------
bool COptionsBase::findSpecial(CNameValue& pair, const string_q& arg) {
    if (specials.size() == 0)
        loadSpecials();
    for (auto special : specials) {
        if (arg == special.first) {
            pair = special;
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------
COptionsBase::COptionsBase(void) : namesFile("") {
    minArgs = 1;
    isReadme = false;
    isRaw = false;
    isVeryRaw = false;
    isNoHeader = false;
    exportFmt = (isApiMode() ? API1 : TXT1);
    enableBits = OPT_DEFAULT;
    scanRange = make_pair(0, NOPOS);
    for (int i = 0; i < 5; i++)
        sorts[i] = NULL;
    hiUp1 = (isTestMode() ? "" : cYellow) + "  ";
    hiUp2 = (isTestMode() ? "" : cTeal);
    hiDown = (isTestMode() ? "" : cOff);
    arguments.clear();
    commandLines.clear();
    namedAccounts.clear();
    pParams = NULL;
    cntParams = 0;
    coutBackup = NULL;
    redirFilename = "";
    // redirStream
}

//--------------------------------------------------------------------------------
COptionsBase::~COptionsBase(void) {
    closeRedirect();
}

//-----------------------------------------------------------------------
void COptionsBase::setSorts(CRuntimeClass* c1, CRuntimeClass* c2, CRuntimeClass* c3) {
    sorts[0] = c1;
    sorts[1] = c2;
    sorts[2] = c3;
}

//--------------------------------------------------------------------------------
bool COptionsBase::isRedirected(void) const {
    return (coutBackup != NULL);
}

//--------------------------------------------------------------------------------
void COptionsBase::closeRedirect(void) {
    if (coutBackup != NULL) {
        cout.rdbuf(coutBackup);  // restore cout's original streambuf
        redirStream.close();
        coutBackup = NULL;
        cout << redirFilename;
        redirFilename = "";
    }
}

//-----------------------------------------------------------------------
bool COptionsBase::loadPrefunds(void) {
    // Note: we don't need to check the dates to see if the prefunds.txt file has been updated
    // since it will never change. In that sense, the binary file is always right once it's created.
    string_q binFile = configPath("prefunds.bin");
    string_q txtFile = configPath("prefunds.txt");
    if (!fileExists(binFile)) {
        if (!fileExists(txtFile))
            return false;
        CStringArray lines;
        asciiFileToLines(txtFile, lines);
        for (auto line : lines) {
            CStringArray parts;
            explode(parts, line, '\t');
            prefundWeiMap[toLower(parts[0])] = str_2_Wei(parts[1]);
        }
        CArchive archive(WRITING_ARCHIVE);
        if (!archive.Lock(binFile, modeWriteCreate, LOCK_NOWAIT))
            return false;
        addr_wei_mp::iterator it = prefundWeiMap.begin();
        archive << uint64_t(prefundWeiMap.size());
        while (it != prefundWeiMap.end()) {
            archive << it->first << it->second;
            it++;
        }
        archive.Release();
        return true;
    }
    CArchive archive(READING_ARCHIVE);
    if (!archive.Lock(binFile, modeReadOnly, LOCK_NOWAIT))
        return false;
    uint64_t count;
    archive >> count;
    for (size_t i = 0; i < count; i++) {
        string_q key;
        wei_t wei;
        archive >> key >> wei;
        prefundWeiMap[key] = wei;
    }
    archive.Release();
    return true;
}

//-----------------------------------------------------------------------
bool COptionsBase::getNamedAccount(CAccountName& acct, const string_q& addr) const {
    if (namedAccounts.size() == 0) {
        uint64_t save = verbose;
        verbose = false;
        COptionsBase* pThis = (COptionsBase*)this;  // NOLINT
        if (!contains(namesFile.getFullPath(), "names.txt"))
            pThis->namesFile = CFilename(configPath("names/names.txt"));
        pThis->loadNames();
        verbose = save;
    }

    for (size_t i = 0; i < namedAccounts.size(); i++) {
        if (namedAccounts[i].address % addr) {
            acct = namedAccounts[i];
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------
string_q COptionsBase::getNamedAccount(const string_q& addr) const {
    CAccountName item;
    item.address = addr;
    getNamedAccount(item, item.address);
    return substitute(substitute(item.name, "(", ""), ")", "");
}

//-----------------------------------------------------------------------
bool COptionsBase::loadNames(void) {
    // If we're already loaded or editing, return
    if (namedAccounts.size() > 0)
        return true;

    string_q textFile = namesFile.getFullPath();
    string_q binFile = substitute(textFile, ".txt", ".bin");

    time_q txtDate = fileLastModifyDate(textFile);
    time_q binDate = fileLastModifyDate(binFile);
    if (verbose && !isTestMode())
        cout << "txtDate: " << txtDate << " binDate: " << binDate << "\n";

    if (binDate > txtDate) {
        CArchive nameCache(READING_ARCHIVE);
        if (nameCache.Lock(binFile, modeReadOnly, LOCK_NOWAIT)) {
            if (verbose && !isTestMode())
                cout << "Reading from binary cache\n";
            nameCache >> namedAccounts;
            nameCache.Release();
            return true;
        }
    }

    if (verbose && !isTestMode())
        cout << "Reading from text database\n";

    // Read the data from the names database and clean it up if needed
    CStringArray lines;
    asciiFileToLines(textFile, lines);
    for (auto line : lines) {
        if (!startsWith(line, '#') && contains(line, "0x")) {
            CAccountName account(line);
            if (isAddress(account.address))
                namedAccounts.push_back(account);
        }
    }
    if (lines.size() == 0) {
        cerr << "Something went wrong loading names file. Quitting...";
        return false;
    }

    CArchive nameCache(WRITING_ARCHIVE);
    if (nameCache.Lock(binFile, modeWriteCreate, LOCK_CREATE)) {
        if (verbose && !isTestMode())
            cout << "Writing binary cache\n";
        nameCache << namedAccounts;
        nameCache.Release();
    }

    return true;
}

//-----------------------------------------------------------------------
string_q cleanFmt(const string_q& str, format_t fmt) {
    string_q ret = (substitute(substitute(substitute(str, "\n", ""), "\\n", "\n"), "\\t", "\t"));
    if (fmt == CSV1)
        ret = "\"" + substitute(ret, "\t", "\",\"") + "\"";
    return ret;
}

const char* STR_DEFAULT_WHENBLOCKS =
    "[ "
    "{ name: \"first\", value: 0 },"
    "{ name: \"firstTrans\", value: 46147 },"
    "{ name: \"iceage\", value: 200000 },"
    "{ name: \"devcon1\", value: 543626 },"
    "{ name: \"homestead\", value: 1150000 },"
    "{ name: \"daofund\", value: 1428756 },"
    "{ name: \"daohack\", value: 1718497 },"
    "{ name: \"daofork\", value: 1920000 },"
    "{ name: \"devcon2\", value: 2286910 },"
    "{ name: \"tangerine\", value: 2463000 },"
    "{ name: \"spurious\", value: 2675000 },"
    "{ name: \"stateclear\", value: 2717576 },"
    "{ name: \"eea\", value: 3265360 },"
    "{ name: \"ens2\", value: 3327417 },"
    "{ name: \"parityhack1\", value: 4041179 },"
    "{ name: \"byzantium\", value: 4370000 },"
    "{ name: \"devcon3\", value: 4469339 },"
    "{ name: \"parityhack2\", value: 4501969 },"
    "{ name: \"kitties\", value: 4605167 },"
    "{ name: \"makerdao\", value: 4620855 },"
    "{ name: \"devcon4\", value: 6610517 },"
    "{ name: \"constantinople\", value: 7280000 },"
    "{ name: \"devcon5\", value: 8700401 },"
    "{ name: \"istanbul\", value: 9069000 },"
    "{ name: \"muirglacier\", value: 9200000 },"
    "{ name: \"latest\", value:\"\" }"
    "]";
}  // namespace qblocks
