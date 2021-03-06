/*-------------------------------------------------------------------------
 * This source code is confidential proprietary information which is
 * copyright (c) 2018, 2019 TrueBlocks, LLC (http://trueblocks.io)
 * All Rights Reserved
 *------------------------------------------------------------------------*/
#include "acctlib.h"
#include "options.h"

//---------------------------------------------------------------
bool visitFinalIndexFiles(const string_q& path, void* data) {
    if (endsWith(path, "/")) {
        return forEveryFileInFolder(path + "*", visitFinalIndexFiles, data);

    } else {
        // Pick up some useful data for either method...
        COptions* options = reinterpret_cast<COptions*>(data);

        // Filenames take the form 'start-end.[txt|bin]' where both 'start' and 'end'
        // are inclusive. Silently skips unknown files in the folder (such as shell scripts).
        if (!contains(path, "-") || !endsWith(path, ".bin"))
            return !shouldQuit();

        timestamp_t ts;
        blknum_t firstBlock = bnFromPath(path, options->lastBlockInFile, ts);
        ASSERT(unused != NOPOS);
        ASSERT(options->lastBlockInFile != NOPOS);

        // If the user told us to start late, start late
        if (options->lastBlockInFile != 0 && options->lastBlockInFile < options->scanRange.first)
            return !shouldQuit();

        // If the user told us to end early, end early
        if (options->scanRange.second != NOPOS && firstBlock > options->scanRange.second)
            return !shouldQuit();

        if (isTestMode() && options->lastBlockInFile > 5000000)
            return !shouldQuit();

        return options->visitBinaryFile(path, data) && !shouldQuit();
    }

    ASSERT(0);  // should not happen
    return !shouldQuit();
}

//----------------------------------------------------------------------------------
bool newReadBloomFromBinary(CNewBloomArray& blooms, const string_q& fileName) {
    blooms.clear();
    CArchive bloomCache(READING_ARCHIVE);
    if (bloomCache.Lock(fileName, modeReadOnly, LOCK_NOWAIT)) {
        uint32_t n;
        bloomCache.Read(n);
        for (size_t i = 0; i < n; i++) {
            bloom_nt bloom;
            uint32_t nI;
            bloomCache.Read(nI);
            bloom.nInserted = nI;
            bloomCache.Read(bloom.bits, sizeof(uint8_t), bloom_nt::BYTE_SIZE);
            blooms.push_back(bloom);
        }
        bloomCache.Close();
        return true;
    }
    return false;
}

#define BREAK_PT 1
//---------------------------------------------------------------
bool COptions::visitBinaryFile(const string_q& path, void* data) {
    string_q l_funcName = "visitBinaryFile";
    static uint32_t n = 0;

    //    COptions *options = reinterpret_cast<COptions*>(data);
    string_q bPath = substitute(substitute(path, indexFolder_finalized, indexFolder_blooms), ".bin", ".bloom");
    if (fileExists(bPath)) {
        CNewBloomArray blooms;
        newReadBloomFromBinary(blooms, bPath);
        bool hit = false;
        for (size_t a = 0; a < monitors.size() && !hit; a++) {
            if (isMember(blooms, monitors[a].address))
                hit = true;
        }

        if (!hit) {
            if (!(++n % BREAK_PT)) {
                ostringstream os;
                os << bBlue << "Skip blocks" << cOff << " " << substitute(path, indexFolder_finalized, "./") << "\r";
                LOG_INFO(os.str());
            }
            // none of them hit, so write last block for each of them
            for (size_t ac = 0; ac < monitors.size() && !hit; ac++) {
                CAccountWatch* acct = &monitors[ac];
                string_q filename = getMonitorPath(acct->address);
                bool exists = fileExists(filename);
                acct->fm_mode = (exists ? FM_PRODUCTION : FM_STAGING);
                acct->writeLastBlock(lastBlockInFile + 1);
            }
            return true;
        }
    }

    if (!(++n % BREAK_PT)) {
        ostringstream os;
        os << cYellow << "Scan blocks" << cOff << " " << substitute(path, indexFolder_finalized, "./") << "\r";
        LOG_INFO(os.str());
    }

    CArchive* chunk = NULL;
    char* rawData = NULL;
    uint32_t nAddrs = 0;

    bool indexHit = false;
    string_q hits;
    for (size_t ac = 0; ac < monitors.size() && !shouldQuit(); ac++) {
        CAccountWatch* acct = &monitors[ac];
        string_q filename = getMonitorPath(acct->address);
        bool exists = fileExists(filename);
        acct->fm_mode = (exists ? FM_PRODUCTION : FM_STAGING);

        if (!acct->openCacheFile1())
            EXIT_FAIL("Could not open cache file " + getMonitorPath(acct->address, acct->fm_mode) + ".");

        CAppearanceArray_base items;
        items.reserve(300000);

        addrbytes_t array = addr_2_Bytes(acct->address);

        if (!chunk) {
            chunk = new CArchive(READING_ARCHIVE);
            if (!chunk || !chunk->Lock(path, modeReadOnly, LOCK_NOWAIT))
                EXIT_FAIL("Could not open index file " + path + ".");

            size_t sz = fileSize(path);
            rawData = reinterpret_cast<char*>(malloc(sz + (2 * 59)));
            if (!rawData) {
                chunk->Release();
                delete chunk;
                chunk = NULL;
                EXIT_FAIL("Could not allocate memory for data.");
            }

            bzero(rawData, sz + (2 * 59));
            size_t nRead = chunk->Read(rawData, sz, sizeof(char));
            if (nRead != sz)
                EXIT_FAIL("Could not read entire file.");

            CHeaderRecord_base* h = reinterpret_cast<CHeaderRecord_base*>(rawData);
            ASSERT(h->magic == MAGIC_NUMBER);
            ASSERT(bytes_2_Hash(h->hash) == versionHash);
            nAddrs = h->nAddrs;
            // uint32_t nRows = h->nRows; not used
        }

        CAddressRecord_base search;
        for (size_t i = 0; i < 20; i++)
            search.bytes[i] = array[i];
        CAddressRecord_base* found =
            (CAddressRecord_base*)bsearch(&search, (rawData + sizeof(CHeaderRecord_base)),  // NOLINT
                                          nAddrs, sizeof(CAddressRecord_base), findAppearance);

        if (found) {
            indexHit = true;
            hits += (acct->address.substr(0, 6) + "..");
            CAddressRecord_base* addrsOnFile =
                reinterpret_cast<CAddressRecord_base*>(rawData + sizeof(CHeaderRecord_base));
            CAppearance_base* blocksOnFile = reinterpret_cast<CAppearance_base*>(&addrsOnFile[nAddrs]);
            for (size_t i = found->offset; i < found->offset + found->cnt; i++) {
                CAppearance_base item(blocksOnFile[i].blk, blocksOnFile[i].txid);
                items.push_back(item);
            }

            if (items.size()) {
                lockSection(true);
                acct->writeAnArray(items);
                acct->writeLastBlock(lastBlockInFile + 1);
                lockSection(false);
            }
        } else {
            acct->writeLastBlock(lastBlockInFile + 1);
        }
    }

    if (chunk) {
        chunk->Release();
        delete chunk;
        chunk = NULL;
    }

    if (rawData) {
        delete rawData;
        rawData = NULL;
    }

    ostringstream os;
    os << cBlue << "    bloom file hit ";
    os << (indexHit ? cGreen : cRed) << (indexHit ? ("index file hits: " + hits) : "false positive") << cOff;
    os << " at " << cTeal << substitute(path, indexFolder_finalized, "./");
    os << cOff;
    LOG_INFO(os.str());

    return !shouldQuit();
}
