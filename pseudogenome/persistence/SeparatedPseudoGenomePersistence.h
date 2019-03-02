#ifndef PGTOOLS_SEPARATEDPSEUDOGENOMEPERSISTENCE_H
#define PGTOOLS_SEPARATEDPSEUDOGENOMEPERSISTENCE_H

#include "../DefaultPseudoGenome.h"
#include "../PackedPseudoGenome.h"
#include "../SeparatedPseudoGenome.h"
#include "../../utils/helper.h"
#include "../../readsset/persistance/ReadsSetPersistence.h"
#include "../readslist/iterator/ReadsListIteratorExtendedWrapper.h"
#include "../readslist/SeparatedExtendedReadsList.h"
#include "../../utils/LzmaLib.h"

namespace PgTools {

    class SeparatedPseudoGenomePersistence {
    private:

        static bool acceptTemporaryPseudoGenomeElement(const string &pseudoGenomePrefix, const string& fileSuffix, bool alwaysRemoveExisting);

        static void appendIndexesFromPg(string pgFilePrefix, vector<uint_reads_cnt_std> &idxs);
        static void writePairMapping(basic_string<char> &pgFilePrefix, vector<uint_reads_cnt_std> orgIdxs);

    public:
        static void writePseudoGenome(PseudoGenomeBase* pgb, const string &pseudoGenomePrefix,
                IndexesMapping* orgIndexesMapping, bool revComplPairFile = false);
        static void writeSeparatedPseudoGenome(SeparatedPseudoGenome *sPg, const string &pseudoGenomePrefix,
            ostream* pgrcOut = 0, bool skipPgSequence = false);
        static SeparatedPseudoGenome* loadSeparatedPseudoGenome(const string &pgPrefix, bool skipReadsList = false);

        static std::ifstream getPseudoGenomeSrc(const string &pseudoGenomePrefix);
        static string loadPseudoGenomeSequence(const string &pseudoGenomePrefix);

        static void getPseudoGenomeProperties(const string &pseudoGenomePrefix, PseudoGenomeHeader *&pgh,
                ReadsSetProperties *&rsProp, bool &plainTextReadMode);

        static std::ifstream getPseudoGenomeElementSrc(const string &pseudoGenomePrefix, const string& fileSuffix);
        static std::ofstream getPseudoGenomeElementDest(const string &pseudoGenomePrefix, const string &fileSuffix,
                                                        bool temporary = false);
        static void acceptTemporaryPseudoGenomeElements(const string &pseudoGenomePrefix, bool clearReadsListDescriptionIfOverride);

        const static string PSEUDOGENOME_FILE_SUFFIX;
        const static string READSLIST_POSITIONS_FILE_SUFFIX;
        const static string READSLIST_ORIGINAL_INDEXES_FILE_SUFFIX;
        const static string PSEUDOGENOME_PROPERTIES_SUFFIX;
        const static string READSLIST_REVERSECOMPL_FILE_SUFFIX;
        const static string READSLIST_MISMATCHES_COUNT_FILE_SUFFIX;
        const static string READSLIST_MISMATCHED_SYMBOLS_FILE_SUFFIX;
        const static string READSLIST_MISMATCHES_POSITIONS_FILE_SUFFIX;

        const static string READSLIST_OFFSETS_FILE_SUFFIX;
        const static string READSLIST_MISMATCHES_REVOFFSETS_FILE_SUFFIX;

        const static string READSLIST_PAIR_FIRST_INDEXES_FILE_SUFFIX;
        const static string READSLIST_PAIR_FIRST_OFFSETS_FILE_SUFFIX;
        const static string READSLIST_PAIR_FIRST_SOURCE_FLAG_FILE_SUFFIX;

        const static string PSEUDOGENOME_MAPPING_OFFSETS_FILE_SUFFIX;
        const static string PSEUDOGENOME_MAPPING_LENGTHS_FILE_SUFFIX;

        const static string TEMPORARY_FILE_SUFFIX;
        const static string PG_FILES_EXTENSION;

        static bool enableReadPositionRepresentation;
        static bool enableRevOffsetMismatchesRepresentation;

        static void dumpPgPairs(vector<string> pgFilePrefixes);

        static void writePseudoGenomeSequence(string &pgSequence, string pgPrefix);
    };

    class SeparatedPseudoGenomeOutputBuilder {
    private:
        const string pseudoGenomePrefix;

        bool onTheFlyMode() { return !pseudoGenomePrefix.empty(); }

        ostream* pgDest = 0;
        ostream* pgPropDest = 0;
        ostream* rlPosDest = 0;
        ostream* rlOrgIdxDest = 0;
        ostream* rlRevCompDest = 0;
        ostream* rlMisCntDest = 0;
        ostream* rlMisSymDest = 0;
        ostream* rlMisPosDest = 0;

        ostream* rlOffDest = 0;
        ostream* rlMisRevOffDest = 0;

        bool disableRevComp = false;
        bool disableMismatches = false;

        void initDest(ostream *&dest, const string &fileSuffix);
        void initReadsListDests();

        DefaultReadsListIteratorInterface *rlIt = 0;
        bool iterationPaused = false;

        uint_pg_len_max lastWrittenPos = 0;
        uint_reads_cnt_max readsCounter = 0;

        PseudoGenomeHeader* pgh = 0;
        ReadsSetProperties* rsProp = 0;

        void prebuildAssert(bool requireOnTheFlyMode);
        void buildProps();

        void freeDest(ostream* &dest);
        void freeDests();
    public:

        SeparatedPseudoGenomeOutputBuilder(bool disableRevComp = false, bool disableMismatches = false);
        SeparatedPseudoGenomeOutputBuilder(const string pseudoGenomePrefix, bool disableRevComp = false, bool disableMismatches = false);

        virtual ~SeparatedPseudoGenomeOutputBuilder();

        void copyPseudoGenomeProperties(const string &pseudoGenomePrefix);
        void copyPseudoGenomeProperties(SeparatedPseudoGenome* sPg);

        void writeReadEntry(const DefaultReadsListEntry &rlEntry);

        void setReadsSourceIterator(DefaultReadsListIteratorInterface *rlIt);
        uint_pg_len_max writeReadsFromIterator(uint_pg_len_max stopPos = (uint_pg_len_max) -1);
        void writeExtraReadEntry(const DefaultReadsListEntry &rlEntry);

        void writePseudoGenome(PseudoGenomeBase* pgb, IndexesMapping* orgIndexesMapping, bool revComplPairFile = "false");
        void writeSeparatedPseudoGenome(SeparatedPseudoGenome *sPg, bool skipPgSequence = false);
        void appendPseudoGenome(const string &pg);

        void build();
        void build(const string &pgPrefix);
        void compressedBuild(ostream &pgrcOut);

        void compressDest(ostream* dest, ostream &pgrcOut, uint8_t coder_type, uint8_t coder_level, int coder_param = -1);
        void destToFile(ostream *dest, const string &fileName);
    };



}

#endif //PGTOOLS_SEPARATEDPSEUDOGENOMEPERSISTENCE_H
