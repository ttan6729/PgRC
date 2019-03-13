#include "SeparatedPseudoGenomePersistence.h"
#include "PseudoGenomePersistence.h"
#include "../TemplateUserGenerator.h"

#include <cstdio>

namespace PgTools {

    void SeparatedPseudoGenomePersistence::writePseudoGenome(PseudoGenomeBase *pgb, const string &pseudoGenomePrefix,
            IndexesMapping* orgIndexesMapping, bool revComplPairFile) {
        clock_checkpoint();
        SeparatedPseudoGenomeOutputBuilder builder(pseudoGenomePrefix, !revComplPairFile, true);
        builder.writePseudoGenome(pgb, orgIndexesMapping, revComplPairFile);
        builder.build();
        cout << "Writing (" << pseudoGenomePrefix << ") pseudo genome files in " << clock_millis() << " msec." << endl << endl;
    }

    void SeparatedPseudoGenomePersistence::writeSeparatedPseudoGenome(SeparatedPseudoGenome *sPg,
            const string &pseudoGenomePrefix, bool skipPgSequence) {
        clock_checkpoint();
        SeparatedPseudoGenomeOutputBuilder builder(sPg->getReadsList()->isRevCompEnabled(),
                sPg->getReadsList()->areMismatchesEnabled());
        builder.feedSeparatedPseudoGenome(sPg, skipPgSequence);
        builder.build(pseudoGenomePrefix);
        cout << "Writing (" << pseudoGenomePrefix << ") pseudo genome files in " << clock_millis() << " msec." << endl << endl;
    }

    void SeparatedPseudoGenomePersistence::compressSeparatedPseudoGenomeReadsList(SeparatedPseudoGenome *sPg,
                                                                      ostream* pgrcOut, uint8_t coder_level,
                                                                      bool skipPgSequence) {
        clock_checkpoint();
        SeparatedPseudoGenomeOutputBuilder builder(sPg->getReadsList()->isRevCompEnabled(),
                                                   sPg->getReadsList()->areMismatchesEnabled());
        builder.feedSeparatedPseudoGenome(sPg, true);
        builder.compressedBuild(*pgrcOut, coder_level);
        cout << "Compressed Pg reads list in " << clock_millis() << " msec." << endl << endl;
        if (!skipPgSequence) {
            clock_checkpoint();
            writeCompressed(*pgrcOut, sPg->getPgSequence().data(), sPg->getPgSequence().size(), LZMA_CODER, coder_level,
                            PGRC_DATAPERIODCODE_8_t);
            cout << "Compressed Pg sequence in " << clock_millis() << " msec." << endl << endl;
        }
    }

    SeparatedPseudoGenome* SeparatedPseudoGenomePersistence::loadSeparatedPseudoGenome(const string &pgPrefix,
            bool skipReadsList){
        PseudoGenomeHeader* pgh = 0;
        ReadsSetProperties* prop = 0;
        bool plainTextReadMode = false;
        SeparatedPseudoGenomePersistence::getPseudoGenomeProperties(pgPrefix, pgh, prop, plainTextReadMode);
        string pgSequence = SeparatedPseudoGenomePersistence::loadPseudoGenomeSequence(pgPrefix);
        ConstantAccessExtendedReadsList* caeRl = skipReadsList?0:
                ConstantAccessExtendedReadsList::loadConstantAccessExtendedReadsList(pgPrefix, pgh->getPseudoGenomeLength());
        SeparatedPseudoGenome* spg = new SeparatedPseudoGenome(std::move(pgSequence), caeRl, prop);
        delete(pgh);
        delete(prop);
        return spg;
    }


    std::ifstream SeparatedPseudoGenomePersistence::getPseudoGenomeSrc(const string &pseudoGenomePrefix) {
        const string pgFile = pseudoGenomePrefix + SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX;
        std::ifstream pgSrc(pgFile, std::ios::in | std::ios::binary);
        if (pgSrc.fail()) {
            fprintf(stderr, "cannot open pseudogenome file %s\n", pgFile.c_str());
            exit(EXIT_FAILURE);
        }
        return pgSrc;
    }

    string SeparatedPseudoGenomePersistence::loadPseudoGenomeSequence(const string &pseudoGenomePrefix) {
        std::ifstream pgSrc = getPseudoGenomeSrc(pseudoGenomePrefix);
        pgSrc.seekg(0, std::ios::end);
        size_t size = pgSrc.tellg();
        std::string buffer(size, ' ');
        pgSrc.seekg(0);
        PgSAHelpers::readArray(pgSrc, &buffer[0], size);
        return buffer;
    }


    void SeparatedPseudoGenomePersistence::writePseudoGenomeSequence(string &pgSequence, string pgPrefix) {
        ofstream pgDest =
                getPseudoGenomeElementDest(pgPrefix, SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX, true);
        PgSAHelpers::writeArray(pgDest, (void*) pgSequence.data(), pgSequence.length());
        pgDest.close();
        acceptTemporaryPseudoGenomeElement(pgPrefix, SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX, true);
    }

    std::ifstream SeparatedPseudoGenomePersistence::getPseudoGenomeElementSrc(const string &pseudoGenomePrefix,
                                                                              const string &fileSuffix) {
        const string pgElFile = pseudoGenomePrefix + fileSuffix;
        std::ifstream pgElSrc(pgElFile, std::ios::in | std::ios::binary);
        if (pgElSrc.fail())
            fprintf(stderr, "warning: pseudogenome element file %s does not exist (or cannot be opened for reading)\n",
                    pgElFile.c_str());

        return pgElSrc;
    }

    std::ofstream SeparatedPseudoGenomePersistence::getPseudoGenomeElementDest(const string &pseudoGenomePrefix,
                                                                               const string &fileSuffix,
                                                                               bool temporary) {
        string pgElFile = pseudoGenomePrefix + fileSuffix;
        if (temporary)
            pgElFile = pgElFile + TEMPORARY_FILE_SUFFIX;

        std::ofstream destPgEl(pgElFile, std::ios::out | std::ios::binary | std::ios::trunc);
        return destPgEl;
    }


    bool SeparatedPseudoGenomePersistence::acceptTemporaryPseudoGenomeElement(const string &pseudoGenomePrefix,
                                                                              const string &fileSuffix,
                                                                              bool alwaysRemoveExisting) {
        string pgElFile = pseudoGenomePrefix + fileSuffix;
        string pgElTempFile = pgElFile + TEMPORARY_FILE_SUFFIX;
        if (std::ifstream(pgElFile) && (std::ifstream(pgElTempFile) || alwaysRemoveExisting))
            remove(pgElFile.c_str());
        if (std::ifstream(pgElTempFile))
            return rename(pgElTempFile.c_str(), pgElFile.c_str()) == 0;
        return false;
    }

    void SeparatedPseudoGenomePersistence::acceptTemporaryPseudoGenomeElements(const string &pseudoGenomePrefix,
            bool clearReadsListDescriptionIfOverride) {
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, PSEUDOGENOME_FILE_SUFFIX, false);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, PSEUDOGENOME_PROPERTIES_SUFFIX, false);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_POSITIONS_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_ORIGINAL_INDEXES_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_REVERSECOMPL_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_MISMATCHES_COUNT_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_MISMATCHED_SYMBOLS_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_MISMATCHES_POSITIONS_FILE_SUFFIX, clearReadsListDescriptionIfOverride);

        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_OFFSETS_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_MISMATCHES_REVOFFSETS_FILE_SUFFIX, clearReadsListDescriptionIfOverride);

        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_PAIR_FIRST_INDEXES_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_PAIR_FIRST_OFFSETS_FILE_SUFFIX, clearReadsListDescriptionIfOverride);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, READSLIST_PAIR_FIRST_SOURCE_FLAG_FILE_SUFFIX, clearReadsListDescriptionIfOverride);

        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, PSEUDOGENOME_MAPPING_OFFSETS_FILE_SUFFIX, false);
        acceptTemporaryPseudoGenomeElement(pseudoGenomePrefix, PSEUDOGENOME_MAPPING_LENGTHS_FILE_SUFFIX, false);
    }

    const string SeparatedPseudoGenomePersistence::PG_FILES_EXTENSION = ".pg";

    const string SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX = "" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::PSEUDOGENOME_PROPERTIES_SUFFIX = "_prop" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_POSITIONS_FILE_SUFFIX = "_rl_pos" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_ORIGINAL_INDEXES_FILE_SUFFIX = "_rl_idx" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_REVERSECOMPL_FILE_SUFFIX = "_rl_rc" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_COUNT_FILE_SUFFIX = "_rl_mis_cnt" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_MISMATCHED_SYMBOLS_FILE_SUFFIX = "_rl_mis_sym" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_POSITIONS_FILE_SUFFIX = "_rl_mis_pos" + PG_FILES_EXTENSION;


    const string SeparatedPseudoGenomePersistence::READSLIST_OFFSETS_FILE_SUFFIX = "_rl_off" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_REVOFFSETS_FILE_SUFFIX = "_rl_mis_roff" + PG_FILES_EXTENSION;

    const string SeparatedPseudoGenomePersistence::READSLIST_PAIR_FIRST_INDEXES_FILE_SUFFIX = "_rl_pr_idx" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_PAIR_FIRST_OFFSETS_FILE_SUFFIX = "_rl_pr_off" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::READSLIST_PAIR_FIRST_SOURCE_FLAG_FILE_SUFFIX = "_rl_pr_sf" + PG_FILES_EXTENSION;

    const string SeparatedPseudoGenomePersistence::PSEUDOGENOME_MAPPING_OFFSETS_FILE_SUFFIX = "_map_off" + PG_FILES_EXTENSION;
    const string SeparatedPseudoGenomePersistence::PSEUDOGENOME_MAPPING_LENGTHS_FILE_SUFFIX = "_map_len" + PG_FILES_EXTENSION;

    const string SeparatedPseudoGenomePersistence::TEMPORARY_FILE_SUFFIX = ".temp";

    bool SeparatedPseudoGenomePersistence::enableReadPositionRepresentation = false;
    bool SeparatedPseudoGenomePersistence::enableRevOffsetMismatchesRepresentation = true;

    void SeparatedPseudoGenomePersistence::getPseudoGenomeProperties(const string &pseudoGenomePrefix,
                                                                     PseudoGenomeHeader *&pgh,
                                                                     ReadsSetProperties *&rsProp,
                                                                     bool &plainTextReadMode) {
        ifstream pgPropSrc(pseudoGenomePrefix + SeparatedPseudoGenomePersistence::PSEUDOGENOME_PROPERTIES_SUFFIX,
                           ios_base::in | ios_base::binary);
        if (pgPropSrc.fail()) {
            fprintf(stderr, "Cannot read pseudogenome properties (%s does not open).\n",
                    pseudoGenomePrefix.c_str());
            exit(EXIT_FAILURE);
        }
        pgh = new PseudoGenomeHeader(pgPropSrc);
        rsProp = new ReadsSetProperties(pgPropSrc);
        plainTextReadMode = confirmTextReadMode(pgPropSrc);
        pgPropSrc.close();
    }

    void SeparatedPseudoGenomePersistence::appendIndexesFromPg(string pgFilePrefix, vector<uint_reads_cnt_std> &idxs) {
        bool plainTextReadMode;
        PseudoGenomeHeader* pgh;
        ReadsSetProperties* rsProp;
        getPseudoGenomeProperties(pgFilePrefix, pgh, rsProp, plainTextReadMode);
        ifstream orgIdxsSrc = getPseudoGenomeElementSrc(pgFilePrefix, READSLIST_ORIGINAL_INDEXES_FILE_SUFFIX);
        const uint_reads_cnt_max readsCount = pgh->getReadsCount();
        idxs.reserve(idxs.size() + readsCount);
        for(uint_reads_cnt_std i = 0; i < readsCount; i++) {
            uint_reads_cnt_std idx;
            readValue<uint_reads_cnt_std>(orgIdxsSrc, idx, plainTextReadMode);
            idxs.push_back(idx);
        }
        delete(pgh);
        delete(rsProp);
    }

    void SeparatedPseudoGenomePersistence::writePairMapping(string &pgFilePrefix,
                                                            vector<uint_reads_cnt_std> orgIdxs) {
        ofstream pair1OffsetsDest = getPseudoGenomeElementDest(pgFilePrefix, READSLIST_PAIR_FIRST_OFFSETS_FILE_SUFFIX, true);
        ofstream pair1SrcFlagDest = getPseudoGenomeElementDest(pgFilePrefix, READSLIST_PAIR_FIRST_SOURCE_FLAG_FILE_SUFFIX, true);
        ofstream pair1IndexesDest = getPseudoGenomeElementDest(pgFilePrefix, READSLIST_PAIR_FIRST_INDEXES_FILE_SUFFIX, true);
        writeReadMode(pair1OffsetsDest, PgSAHelpers::plainTextWriteMode);
        writeReadMode(pair1IndexesDest, PgSAHelpers::plainTextWriteMode);
        uint_reads_cnt_std readsCount = orgIdxs.size();
        vector<uint_reads_cnt_std> rev(readsCount);
        for(uint_reads_cnt_std i = 0; i < readsCount; i++)
            rev[orgIdxs[i]] = i;
        vector<bool> isReadDone(readsCount, false);
        for(uint32_t i = 0; i < readsCount; i++) {
            if (isReadDone[i])
                continue;
            uint_reads_cnt_std idx = orgIdxs[i];
            uint_reads_cnt_std pairIdx = idx % 2?(idx-1):(idx+1);
            uint_reads_cnt_std pairI = rev[pairIdx];
            isReadDone[pairI] = true;
            writeValue<uint_reads_cnt_std>(pair1OffsetsDest, pairI > i?pairI - i: readsCount - (i - pairI));
            writeValue<uint8_t>(pair1SrcFlagDest, idx % 2);
            writeValue<uint_reads_cnt_std>(pair1IndexesDest, idx);
        }
        pair1IndexesDest.close();
        pair1OffsetsDest.close();
        acceptTemporaryPseudoGenomeElements(pgFilePrefix, false);
    }

    void SeparatedPseudoGenomePersistence::dumpPgPairs(vector<string> pgFilePrefixes) {
        clock_checkpoint();
        vector<uint_reads_cnt_std> orgIdxs;
        for(string pgFilePrefix: pgFilePrefixes)
            SeparatedPseudoGenomePersistence::appendIndexesFromPg(pgFilePrefix, orgIdxs);

        SeparatedPseudoGenomePersistence::writePairMapping(pgFilePrefixes[0], orgIdxs);
        cout << "... dumping pairs completed in " << clock_millis() << " msec. " << endl;
    }

    void SeparatedPseudoGenomePersistence::compressReadsOrder(ostream &pgrcOut,
            const vector<uint_reads_cnt_std>& orgIdxs, uint8_t coder_level,
            bool completeOrderInfo, bool singleFileMode) {
        clock_checkpoint();
        cout << endl;
        uint_reads_cnt_std readsCount = orgIdxs.size();
        int lzma_coder_param = readsCount <= UINT32_MAX ? PGRC_DATAPERIODCODE_32_t : PGRC_DATAPERIODCODE_64_t;
        vector<uint_reads_cnt_std> rev(readsCount);
        for (uint_reads_cnt_std i = 0; i < readsCount; i++)
            rev[orgIdxs[i]] = i;
        if (completeOrderInfo && singleFileMode) {
            cout << "Reverse index of original indexes... ";
            writeCompressed(pgrcOut, (char *) rev.data(), rev.size() * sizeof(uint_reads_cnt_std), LZMA_CODER,
                    coder_level, lzma_coder_param);
        } else {
            // absolute original index of a processed pair base
            vector<uint_reads_cnt_std> pairBaseOrgIdx;
            if (completeOrderInfo)
                pairBaseOrgIdx.reserve(readsCount / 2);
            // coding reads list index offset of paired read
            vector<uint8_t> offsetInUint8Flag;
            offsetInUint8Flag.reserve(readsCount / 2);
            vector<uint8_t> offsetInUint8Value;
            offsetInUint8Value.reserve(readsCount / 2); // estimated
            vector<uint8_t> deltaInInt8Flag;
            deltaInInt8Flag.reserve(readsCount / 4); // estimated
            vector<int8_t> deltaInInt8Value;
            deltaInInt8Value.reserve(readsCount / 16); // estimated
            vector<uint_reads_cnt_std> fullOffset;
            deltaInInt8Value.reserve(readsCount / 8); // estimated

            vector<bool> isReadDone(readsCount, false);
            int64_t refPrev = 0;
            int64_t prev = 0;
            bool match = false;
            for (uint32_t i1 = 0; i1 < readsCount; i1++) {
                if (isReadDone[i1])
                    continue;
                uint_reads_cnt_std orgIdx = orgIdxs[i1];
                uint_reads_cnt_std pairOrgIdx = orgIdx % 2 ? (orgIdx - 1) : (orgIdx + 1);
                uint_reads_cnt_std i2 = rev[pairOrgIdx]; // i2 > i1
                isReadDone[i2] = true;
                if (completeOrderInfo)
                    pairBaseOrgIdx.push_back(orgIdx);
                int64_t pairOffset = i2 - i1;
                offsetInUint8Flag.push_back((uint8_t) (pairOffset <= UINT8_MAX));
                if (pairOffset <= UINT8_MAX) {
                    offsetInUint8Value.push_back((uint8_t) pairOffset);
                    continue;
                }
                const int64_t delta = pairOffset - refPrev;
                const bool isDeltaInInt8 = delta <= INT8_MAX && delta >= INT8_MIN;
                deltaInInt8Flag.push_back((uint8_t) isDeltaInInt8);
                if (isDeltaInInt8) {
                    match = true;
                    deltaInInt8Value.push_back((int8_t) delta);
                    refPrev = pairOffset;
                } else {
                    if (!match || refPrev != prev)
                        refPrev = pairOffset;
                    fullOffset.push_back(pairOffset);
                    match = false;
                }
                prev = pairOffset;
            }

            cout << "Uint8 reads list offsets of pair reads (flag)... ";
            writeCompressed(pgrcOut, (char *) offsetInUint8Flag.data(), offsetInUint8Flag.size() * sizeof(uint8_t),
                            PPMD7_CODER, coder_level, 3);
            cout << "Uint8 reads list offsets of pair reads (value)... ";
//        writeCompressed(pgrcOut, (char*) offsetInUint8Value.data(), offsetInUint8Value.size() * sizeof(uint8_t), LZMA_CODER, coder_level, PGRC_DATAPERIODCODE_8_t);
            writeCompressed(pgrcOut, (char *) offsetInUint8Value.data(), offsetInUint8Value.size() * sizeof(uint8_t),
                            PPMD7_CODER, coder_level, 2);
            cout << "Offsets deltas of pair reads (flag)... ";
            writeCompressed(pgrcOut, (char *) deltaInInt8Flag.data(), deltaInInt8Flag.size() * sizeof(uint8_t),
                            PPMD7_CODER, coder_level, 3);
            cout << "Offsets deltas of pair reads (value)... ";
            writeCompressed(pgrcOut, (char *) deltaInInt8Value.data(), deltaInInt8Value.size() * sizeof(int8_t),
                            PPMD7_CODER, coder_level, 2);
            cout << "Full reads list offsets of pair reads ... ";
            writeCompressed(pgrcOut, (char *) fullOffset.data(), fullOffset.size() * sizeof(uint_reads_cnt_std),
                            LZMA_CODER, coder_level, lzma_coder_param);
            if (completeOrderInfo) {
                cout << "Original indexes of pair bases... ";
                writeCompressed(pgrcOut, (char *) pairBaseOrgIdx.data(), pairBaseOrgIdx.size() * sizeof(uint_reads_cnt_std),
                                LZMA_CODER, coder_level, lzma_coder_param);
            }
        }
        cout << "... compressing order information completed in " << clock_millis() << " msec. " << endl;
    }

    void SeparatedPseudoGenomePersistence::decompressReadsOrder(istream &pgrcIn,
                                                                vector<uint_reads_cnt_std> &rlIdxOrder,
                                                                bool completeOrderInfo, bool singleFileMode) {
        if (!completeOrderInfo && singleFileMode)
            return;

        if (completeOrderInfo && singleFileMode)
            readCompressed<uint_reads_cnt_std>(pgrcIn, rlIdxOrder);
        else {
            vector<uint8_t> offsetInUint8Flag;
            vector<uint8_t> offsetInUint8Value;
            vector<uint8_t> deltaInInt8Flag;
            vector<int8_t> deltaInInt8Value;
            vector<uint_reads_cnt_std> fullOffset;
            readCompressed<uint8_t>(pgrcIn, offsetInUint8Flag);
            readCompressed<uint8_t>(pgrcIn, offsetInUint8Value);
            readCompressed<uint8_t>(pgrcIn, deltaInInt8Flag);
            readCompressed<int8_t>(pgrcIn, deltaInInt8Value);
            readCompressed<uint_reads_cnt_std>(pgrcIn, fullOffset);

            uint_reads_cnt_std readsCount = offsetInUint8Flag.size() * 2;
            rlIdxOrder.resize(readsCount);
            vector<bool> isReadDone(readsCount, false);
            int64_t pairOffset = 0;
            int64_t pairCounter = -1;
            int64_t offIdx = -1;
            int64_t delFlagIdx = -1;
            int64_t delIdx = -1;
            int64_t fulIdx = -1;
            int64_t refPrev = 0;
            int64_t prev = 0;
            bool match = false;

            for(uint_reads_cnt_std i = 0; i < readsCount; i++) {
                if (isReadDone[i])
                    continue;
                if (offsetInUint8Flag[++pairCounter])
                    pairOffset = offsetInUint8Value[++offIdx];
                else if (deltaInInt8Flag[++delFlagIdx]) {
                    pairOffset = refPrev + deltaInInt8Value[++delIdx];
                    refPrev = pairOffset;
                    match = true;
                    prev = pairOffset;
                } else {
                    pairOffset = fullOffset[++fulIdx];
                    if (!match || refPrev != prev)
                        refPrev = pairOffset;
                    match = false;
                    prev = pairOffset;
                }
                rlIdxOrder[pairCounter * 2] = i;
                rlIdxOrder[pairCounter * 2 + 1] = i + pairOffset;
                isReadDone[i + pairOffset] = true;
            }
            if (completeOrderInfo) {
                vector<uint_reads_cnt_std> pairBaseOrgIdx;
                readCompressed<uint_reads_cnt_std>(pgrcIn, pairBaseOrgIdx);
                vector<uint_reads_cnt_std> peRlIdxOrder = std::move(rlIdxOrder);
                rlIdxOrder.resize(readsCount);
                for(uint_reads_cnt_std p = 0; p < readsCount / 2; p++) {
                    uint_reads_cnt_std orgIdx = pairBaseOrgIdx[p];
                    rlIdxOrder[orgIdx] = peRlIdxOrder[p * 2];
                    rlIdxOrder[orgIdx % 2?orgIdx - 1:orgIdx + 1] = peRlIdxOrder[p * 2 + 1];
                }
            }
        }
    }

    SeparatedPseudoGenomeOutputBuilder::SeparatedPseudoGenomeOutputBuilder(const string pseudoGenomePrefix,
            bool disableRevComp, bool disableMismatches) : pseudoGenomePrefix(pseudoGenomePrefix),
            disableRevComp(disableRevComp), disableMismatches(disableMismatches) {
        initReadsListDests();
    }

    SeparatedPseudoGenomeOutputBuilder::SeparatedPseudoGenomeOutputBuilder(bool disableRevComp, bool disableMismatches)
            : SeparatedPseudoGenomeOutputBuilder("", disableRevComp, disableMismatches) {
        initReadsListDests();
    }

    void SeparatedPseudoGenomeOutputBuilder::initDest(ostream *&dest, const string &fileSuffix) {
        if (dest == 0) {
            if (onTheFlyMode())
                dest = new ofstream(
                        SeparatedPseudoGenomePersistence::getPseudoGenomeElementDest(pseudoGenomePrefix, fileSuffix, true));
            else
                dest = new ostringstream();
        }
    }

    void SeparatedPseudoGenomeOutputBuilder::initReadsListDests() {
        if (SeparatedPseudoGenomePersistence::enableReadPositionRepresentation)
            initDest(rlPosDest, SeparatedPseudoGenomePersistence::READSLIST_POSITIONS_FILE_SUFFIX);
        else
            initDest(rlOffDest, SeparatedPseudoGenomePersistence::READSLIST_OFFSETS_FILE_SUFFIX);
        initDest(rlOrgIdxDest, SeparatedPseudoGenomePersistence::READSLIST_ORIGINAL_INDEXES_FILE_SUFFIX);
        if (!disableRevComp)
            initDest(rlRevCompDest, SeparatedPseudoGenomePersistence::READSLIST_REVERSECOMPL_FILE_SUFFIX);
        if (!disableMismatches) {
            initDest(rlMisCntDest, SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_COUNT_FILE_SUFFIX);
            initDest(rlMisSymDest, SeparatedPseudoGenomePersistence::READSLIST_MISMATCHED_SYMBOLS_FILE_SUFFIX);
            if (SeparatedPseudoGenomePersistence::enableRevOffsetMismatchesRepresentation)
                initDest(rlMisRevOffDest,
                         SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_REVOFFSETS_FILE_SUFFIX);
            else
                initDest(rlMisPosDest, SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_POSITIONS_FILE_SUFFIX);
        }
    }

    void SeparatedPseudoGenomeOutputBuilder::destToFile(ostream *dest, const string &fileName) {
        if (!onTheFlyMode() && dest)
            PgSAHelpers::writeStringToFile(fileName, ((ostringstream*) dest)->str());
    }

    void SeparatedPseudoGenomeOutputBuilder::freeDest(ostream* &dest) {
        if (dest) {
            if (onTheFlyMode())
                ((ofstream*) dest)->close();
            delete(dest);
            dest = 0;
        }
    }

    void SeparatedPseudoGenomeOutputBuilder::freeDests() {
        freeDest(pgDest);
        freeDest(pgPropDest);
        freeDest(rlPosDest);
        freeDest(rlOrgIdxDest);
        freeDest(rlRevCompDest);
        freeDest(rlMisCntDest);
        freeDest(rlMisSymDest);
        freeDest(rlMisPosDest);

        freeDest(rlOffDest);
        freeDest(rlMisRevOffDest);
    }

    void SeparatedPseudoGenomeOutputBuilder::prebuildAssert(bool requireOnTheFlyMode) {
        if (onTheFlyMode() != requireOnTheFlyMode) {
            if (requireOnTheFlyMode)
                fprintf(stderr, "This build mode is supported only in on-the-fly mode.\n");
            else
                fprintf(stderr, "This build mode is not supported in on-the-fly mode.\n");
            exit(EXIT_FAILURE);
        }
        if (pgh == 0) {
            fprintf(stderr, "Pseudo genome header not initialized in separated Pg builder.\n");
            exit(EXIT_FAILURE);
        }
    }

    void SeparatedPseudoGenomeOutputBuilder::buildProps() {
        pgh->setReadsCount(readsCounter);
        rsProp->readsCount = readsCounter;
        rsProp->allReadsLength = rsProp->constantReadLength ? readsCounter * rsProp->maxReadLength : -1;
        if (pgPropDest) {
            delete (pgPropDest);
            pgPropDest = 0;
        }
        initDest(pgPropDest, SeparatedPseudoGenomePersistence::PSEUDOGENOME_PROPERTIES_SUFFIX);
        pgh->write(*pgPropDest);
        rsProp->write(*pgPropDest);
    }

    void SeparatedPseudoGenomeOutputBuilder::build() {
        prebuildAssert(true);
        buildProps();
        writeReadMode(*pgPropDest, plainTextWriteMode);

        freeDests();
        SeparatedPseudoGenomePersistence::acceptTemporaryPseudoGenomeElements(pseudoGenomePrefix, true);
    }

    void SeparatedPseudoGenomeOutputBuilder::build(const string &pgPrefix) {
        if (pgPrefix.empty())
            return;
        prebuildAssert(false);
        buildProps();
        writeReadMode(*pgPropDest, plainTextWriteMode);
        PgSAHelpers::writeStringToFile(pgPrefix + SeparatedPseudoGenomePersistence::PSEUDOGENOME_PROPERTIES_SUFFIX,
                                      ((ostringstream*) pgPropDest)->str());

        destToFile(rlPosDest, pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_POSITIONS_FILE_SUFFIX);
        destToFile(rlOffDest, pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_OFFSETS_FILE_SUFFIX);
        destToFile(rlOrgIdxDest, pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_ORIGINAL_INDEXES_FILE_SUFFIX);
        destToFile(rlRevCompDest, pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_REVERSECOMPL_FILE_SUFFIX);
        destToFile(rlMisCntDest, pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_COUNT_FILE_SUFFIX);
        destToFile(rlMisSymDest, pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_MISMATCHED_SYMBOLS_FILE_SUFFIX);
        destToFile(rlMisRevOffDest,
                   pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_REVOFFSETS_FILE_SUFFIX);
        destToFile(rlMisPosDest, pgPrefix + SeparatedPseudoGenomePersistence::READSLIST_MISMATCHES_POSITIONS_FILE_SUFFIX);
    }

    void SeparatedPseudoGenomeOutputBuilder::compressDest(ostream* dest, ostream &pgrcOut, uint8_t coder_type,
                                                          uint8_t coder_level, int coder_param, SymbolsPackingFacility<uint8_t>* symPacker) {
        if (onTheFlyMode() || !dest) {
            fprintf(stderr, "Error during compression: an input stream missing.\n");
            exit(EXIT_FAILURE);
        }
        string tmp = ((ostringstream*) dest)->str();
        if (symPacker) {
            PgSAHelpers::writeValue<uint64_t>(pgrcOut, tmp.length(), false);
            tmp = symPacker->packSequence(tmp.data(), tmp.length());
        }
        writeCompressed(pgrcOut, tmp, coder_type, coder_level, coder_param);
    }

    void SeparatedPseudoGenomeOutputBuilder::compressRlMisRevOffDest(ostream &pgrcOut, uint8_t coder_level,
            bool transposeMode) {
        const uint8_t MISMATCHES_COUNT_DESTS_LIMIT = coder_level == PGRC_CODER_LEVEL_FAST?1:12;
        PgSAHelpers::writeValue<uint8_t>(pgrcOut, MISMATCHES_COUNT_DESTS_LIMIT);
        if (MISMATCHES_COUNT_DESTS_LIMIT == 1) {
            compressDest(rlMisRevOffDest, pgrcOut, PPMD7_CODER, coder_level, 3);
            return;
        }
        vector<uint8_t> misCnt2DestIdx; // NOT-TESTED => {0, 1, 2, 3, 4, 5, 6, 7, 7, 9, 9, 9 };
        misCnt2DestIdx.insert(misCnt2DestIdx.end(), UINT8_MAX, MISMATCHES_COUNT_DESTS_LIMIT);
        for(uint8_t m = 1; m < MISMATCHES_COUNT_DESTS_LIMIT; m++) {
            misCnt2DestIdx[m] = m;
            PgSAHelpers::writeValue<uint8_t>(pgrcOut, misCnt2DestIdx[m]);
        }
        ostringstream dests[UINT8_MAX];
        istringstream misRevOffSrc(((ostringstream*) rlMisRevOffDest)->str());
        istringstream misCntSrc(((ostringstream*) rlMisCntDest)->str());

        uint8_t misCnt = 0;
        uint16_t revOff = 0;
        for(uint_reads_cnt_max i = 0; i < readsCounter; i++) {
             PgSAHelpers::readValue<uint8_t>(misCntSrc, misCnt, false);
             for(uint8_t m = 0; m < misCnt; m++) {
                 PgSAHelpers::readReadLengthValue(misRevOffSrc, revOff, false);
                 PgSAHelpers::writeReadLengthValue(dests[misCnt2DestIdx[misCnt]], revOff);
             }
        }


        if (transposeMode) {
            for (uint8_t d = 1; d < MISMATCHES_COUNT_DESTS_LIMIT; d++) {
                if (misCnt2DestIdx[d] == misCnt2DestIdx[d - 1] || misCnt2DestIdx[d] == misCnt2DestIdx[d + 1])
                    continue;
                string matrix = dests[d].str();
                uint64_t readsCount = matrix.size() / d / (bytePerReadLengthMode ? 1 : 2);
                if (bytePerReadLengthMode)
                    dests[d].str(transpose<uint8_t>(matrix, readsCount, d));
                else
                    dests[d].str(transpose<uint16_t>(matrix, readsCount, d));
            }
        }

        for(uint8_t m = 1; m <= MISMATCHES_COUNT_DESTS_LIMIT; m++) {
            cout << (int) m << ": ";
            compressDest(&dests[m], pgrcOut, PPMD7_CODER, coder_level, 2);
        }
    }

    void SeparatedPseudoGenomeOutputBuilder::compressedBuild(ostream &pgrcOut, uint8_t coder_level) {
        prebuildAssert(false);
        buildProps();
        writeReadMode(*pgPropDest, false);
        const string tmp = ((ostringstream*) pgPropDest)->str();
        pgrcOut.write(tmp.data(), tmp.length());

//        int lzma_coder_param = this->rsProp->maxReadLength <= UINT8_MAX?PGRC_DATAPERIODCODE_8_t:PGRC_DATAPERIODCODE_16_t;
        cout << "Reads list offsets... ";
//        compressDest(rlOffDest, pgrcOut, LZMA_CODER, PGRC_CODER_LEVEL_MAXIMUM, lzma_coder_param);
        compressDest(rlOffDest, pgrcOut, PPMD7_CODER, coder_level, 3);
        cout << "Reverse complements info... ";
//        compressDest(rlRevCompDest, pgrcOut, LZMA_CODER, PGRC_CODER_LEVEL_MAXIMUM, PGRC_DATAPERIODCODE_8_t);
        compressDest(rlRevCompDest, pgrcOut, PPMD7_CODER, coder_level, 2);
//                &SymbolsPackingFacility<uint8_t>::BinaryPacker);
        cout << "Mismatches counts... ";
//        compressDest(rlMisCntDest, pgrcOut, LZMA_CODER, PGRC_CODER_LEVEL_MAXIMUM, lzma_coder_param);
        compressDest(rlMisCntDest, pgrcOut, PPMD7_CODER, coder_level, 2);
        cout << "Mismatched symbols codes... ";
//        compressDest(rlMisSymDest, pgrcOut, LZMA_CODER, PGRC_CODER_LEVEL_MAXIMUM, lzma_coder_param);
        compressDest(rlMisSymDest, pgrcOut, PPMD7_CODER, coder_level, 2);
//                &SymbolsPackingFacility<uint8_t>::QuaternaryPacker);
        cout << "Mismatches offsets (rev-coded)... " << endl;
//        compressDest(rlMisRevOffDest, pgrcOut, LZMA_CODER, PGRC_CODER_LEVEL_MAXIMUM, lzma_coder_param);
//        compressDest(rlMisRevOffDest, pgrcOut, PPMD7_CODER, coder_level, 3);
        compressRlMisRevOffDest(pgrcOut, coder_level);
    }

    void SeparatedPseudoGenomeOutputBuilder::buildInto(SeparatedPseudoGenome *sPg) {
        prebuildAssert(false);
        buildProps();
        string tmp = ((ostringstream*) rlOrgIdxDest)->str();
        uint_reads_cnt_std* orgIdxPtr = (uint_reads_cnt_std*) tmp.data();
        sPg->getReadsList()->orgIdx.assign(orgIdxPtr, orgIdxPtr + readsCounter);

        // only partially implemented
    }

    void SeparatedPseudoGenomeOutputBuilder::writeReadEntry(const DefaultReadsListEntry &rlEntry) {
        lastWrittenPos = rlEntry.pos;
        if (SeparatedPseudoGenomePersistence::enableReadPositionRepresentation)
            PgSAHelpers::writeValue<uint_pg_len_max>(*rlPosDest, rlEntry.pos);
        else
            PgSAHelpers::writeReadLengthValue(*rlOffDest, rlEntry.offset);
        PgSAHelpers::writeValue<uint_reads_cnt_std>(*rlOrgIdxDest, rlEntry.idx);
        if (!disableRevComp)
            PgSAHelpers::writeValue<uint8_t>(*rlRevCompDest, rlEntry.revComp?1:0);
        if (!disableMismatches) {
            PgSAHelpers::writeValue<uint8_t>(*rlMisCntDest, rlEntry.mismatchesCount);
            if (rlEntry.mismatchesCount) {
                for (uint8_t i = 0; i < rlEntry.mismatchesCount; i++)
                    PgSAHelpers::writeValue<uint8_t>(*rlMisSymDest, rlEntry.mismatchCode[i]);
                if (SeparatedPseudoGenomePersistence::enableRevOffsetMismatchesRepresentation) {
                    uint8_t currentPos = pgh->getMaxReadLength() - 1;
                    for (int16_t i = rlEntry.mismatchesCount - 1; i >= 0; i--) {
                        PgSAHelpers::writeReadLengthValue(*rlMisRevOffDest,
                                                                   currentPos - rlEntry.mismatchOffset[i]);
                        currentPos = rlEntry.mismatchOffset[i] - 1;
                    }
                } else {
                    for (uint8_t i = 0; i < rlEntry.mismatchesCount; i++)
                        PgSAHelpers::writeReadLengthValue(*rlMisPosDest, rlEntry.mismatchOffset[i]);
                }
            }
        }
        readsCounter++;
    }

    void SeparatedPseudoGenomeOutputBuilder::writeExtraReadEntry(const DefaultReadsListEntry &rlEntry) {
        writeReadEntry(rlEntry);
        if (rlIt != 0) {
            ReadsListEntry<255, uint_read_len_max, uint_reads_cnt_max, uint_pg_len_max> &itEntry = this->rlIt->peekReadEntry();
            itEntry.offset -= rlEntry.offset;
        }
    }

    void SeparatedPseudoGenomeOutputBuilder::setReadsSourceIterator(DefaultReadsListIteratorInterface *rlIt) {
        rlIt->rewind();
        this->rlIt = rlIt;
    }

    uint_pg_len_max SeparatedPseudoGenomeOutputBuilder::writeReadsFromIterator(uint_pg_len_max stopPos) {
        if (iterationPaused) {
            if (rlIt->peekReadEntry().pos >= stopPos)
                return lastWrittenPos;
            writeReadEntry(rlIt->peekReadEntry());
            iterationPaused = false;
        }
        while (rlIt->moveNext()) {
            if (rlIt->peekReadEntry().pos >= stopPos) {
                iterationPaused = true;
                return lastWrittenPos;
            }
            writeReadEntry(rlIt->peekReadEntry());
        }
        return -1;
    }

    void SeparatedPseudoGenomeOutputBuilder::writePseudoGenome(PseudoGenomeBase *pgb, IndexesMapping* orgIndexesMapping, bool revComplPairFile) {

        initDest(pgDest, SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX);
        string pg = pgb->getPseudoGenomeVirtual();
        PgSAHelpers::writeArray(*pgDest, (void*) pg.data(), pg.length());

        ReadsListIteratorExtendedWrapperBase* rlIt =
                TemplateUserGenerator::generateReadsListUser<ReadsListIteratorExtendedWrapper, ReadsListIteratorExtendedWrapperBase>(pgb);
        rlIt->applyIndexesMapping(orgIndexesMapping);
        if (revComplPairFile)
            rlIt->applyRevComplPairFileFlag();
        if (pgb->isReadLengthMin())
            bytePerReadLengthMode = true;
        setReadsSourceIterator(rlIt);
        writeReadsFromIterator();

        if (pgh == 0)
            pgh = new PseudoGenomeHeader(pgb);
        if (rsProp == 0)
            rsProp = new ReadsSetProperties(*(pgb->getReadsSetProperties()));
        if (pgh->getReadsCount() != readsCounter) {
            fprintf(stderr, "Incorrect reads count validation while building separated Pg (%llu instead of %llu).\n",
                    readsCounter, pgh->getReadsCount());
            exit(EXIT_FAILURE);
        }

        delete(rlIt);
    }

    void SeparatedPseudoGenomeOutputBuilder::copyPseudoGenomeProperties(const string &pseudoGenomePrefix) {
        ifstream pgPropSrc(pseudoGenomePrefix + SeparatedPseudoGenomePersistence::PSEUDOGENOME_PROPERTIES_SUFFIX,
                           ios_base::in | ios_base::binary);
        if (pgPropSrc.fail()) {
            fprintf(stderr, "Cannot read pseudogenome properties (%s does not open).\n",
                    pseudoGenomePrefix.c_str());
            exit(EXIT_FAILURE);
        }
        if (pgh != 0)
            delete(pgh);
        if (rsProp != 0)
            delete(rsProp);
        this->pgh = new PseudoGenomeHeader(pgPropSrc);
        this->rsProp = new ReadsSetProperties(pgPropSrc);
        pgPropSrc.close();
    }

    void SeparatedPseudoGenomeOutputBuilder::copyPseudoGenomeProperties(SeparatedPseudoGenome* sPg) {
        this->pgh = new PseudoGenomeHeader(sPg);
        this->rsProp = new ReadsSetProperties(*(sPg->getReadsSetProperties()));
        if (sPg->isReadLengthMin())
            bytePerReadLengthMode = true;
    }

    void SeparatedPseudoGenomeOutputBuilder::appendPseudoGenome(const string &pg) {
        if (pgDest) {
            pgh->setPseudoGenomeLength(pgh->getPseudoGenomeLength() + pg.length());
            PgSAHelpers::writeArray(*pgDest, (void *) pg.data(), pg.length());
        } else {
            pgh->setPseudoGenomeLength(pg.length());
            initDest(pgDest, SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX);
            PgSAHelpers::writeArray(*pgDest, (void *) pg.data(), pg.length());
        }
    }

    void SeparatedPseudoGenomeOutputBuilder::feedSeparatedPseudoGenome(SeparatedPseudoGenome *sPg, bool skipPgSequence) {
        if (!skipPgSequence) {
            initDest(pgDest, SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX);
            const string &pg = sPg->getPgSequence();
            PgSAHelpers::writeArray(*pgDest, (void *) pg.data(), pg.length());
        }
        if (sPg->isReadLengthMin())
            bytePerReadLengthMode = true;
        setReadsSourceIterator(sPg->getReadsList());
        writeReadsFromIterator();

        if (pgh == 0)
            pgh = new PseudoGenomeHeader(sPg);
        if (rsProp == 0)
            rsProp = new ReadsSetProperties(*(sPg->getReadsSetProperties()));
        if (pgh->getReadsCount() != readsCounter) {
            fprintf(stderr, "Incorrect reads count validation while building separated Pg (%llu instead of %llu).\n",
                    readsCounter, pgh->getReadsCount());
            exit(EXIT_FAILURE);
        }
    }

    SeparatedPseudoGenomeOutputBuilder::~SeparatedPseudoGenomeOutputBuilder() {
        if (pgh)
            delete(pgh);
        if (rsProp)
            delete(rsProp);

    }
}
