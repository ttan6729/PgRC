#ifndef PGTOOLS_READSMATCHERS_H
#define PGTOOLS_READSMATCHERS_H

#include <iostream>
#include <fstream>
#include <string>

#include "../readsset/PackedReadsSet.h"
#include "ConstantLengthPatternsOnTextHashMatcher.h"
#include "../pseudogenome/persistence/SeparatedPseudoGenomePersistence.h"

using namespace std;

namespace PgTools {

    uint8_t
    countMismatches(const char *pattern, const char *text, uint64_t length, uint8_t maxMismatches = UINT8_MAX);

    class DefaultReadsMatcher {
    protected:
        string pgFilePrefix;
        bool revComplPg;
        PackedReadsSet *readsSet;
        uint32_t matchPrefixLength;

        vector<uint32_t> readMatchPos;
        vector<bool> readMatchRC;

        vector<uint8_t> readMismatchesCount;
        uint_reads_cnt_max matchedReadsCount = 0;

        uint_reads_cnt_max readsCount;
        uint_read_len_max readLength;
        uint_read_len_max matchingLength;

        // stats
        uint64_t multiMatchCount = 0;
        uint64_t falseMatchCount = 0;

        virtual void initMatching();
        virtual void matchConstantLengthReads(const char* txt, uint64_t length, bool revCompMode = false) = 0;
        virtual void writeMatchesInfo(ofstream &offsetsDest, ofstream &missedPatternsDest, ofstream &suffixesDest) = 0;

        virtual SeparatedPseudoGenomeOutputBuilder *createSeparatedPseudoGenomeOutputBuilder(const string &outPgPrefix,
                bool enableRevComp, bool enableMismatches) = 0;

        virtual void initEntryUpdating() = 0;
        virtual void updateEntry(DefaultReadsListEntry &entry, uint_reads_cnt_max matchIdx) = 0;
        virtual void closeEntryUpdating() = 0;

    public:
        static const uint_read_len_max DISABLED_PREFIX_MODE;
        static const uint32_t NOT_MATCHED_VALUE;

        DefaultReadsMatcher(const string &pgFilePrefix, bool revComplPg, PackedReadsSet *readsSet,
                            uint32_t matchPrefixLength);

        virtual ~DefaultReadsMatcher();

        void matchConstantLengthReads();

        static const string OFFSETS_SUFFIX;
        static const string SUFFIXES_SUFFIX;
        static const string MISSED_READS_SUFFIX;

        void writeMatchesInfo(const string &outPrefix);

        const vector<uint_reads_cnt_max> getMatchedReadsIndexes() const;
        const vector<uint32_t> &getReadMatchPos() const;
        const vector<uint8_t> &getReadMismatches() const;

        void writeIntoPseudoGenome(const string &outPgPrefix, const vector<uint_reads_cnt_max> &orgIndexesMapping);
    };

    class DefaultReadsExactMatcher: public DefaultReadsMatcher {
    private:
        DefaultConstantLengthPatternsOnTextHashMatcher* hashMatcher = 0;

    protected:
        void initMatching();
        void matchConstantLengthReads(const char* txt, uint64_t length, bool revCompMode = false);
        void writeMatchesInfo(ofstream &offsetsDest, ofstream &missedPatternsDest, ofstream &suffixesDest);

        SeparatedPseudoGenomeOutputBuilder *createSeparatedPseudoGenomeOutputBuilder(const string &outPgPrefix,
                bool enableRevComp, bool enableMismatches) override;

        void initEntryUpdating() override {};
        void updateEntry(DefaultReadsListEntry &entry, uint_reads_cnt_max matchIdx) override {};
        void closeEntryUpdating() override {};
    public:
        DefaultReadsExactMatcher(const string &pgFilePrefix, bool revComplPg, PackedReadsSet *readsSet,
                                 uint32_t matchPrefixLength);

        virtual ~DefaultReadsExactMatcher();

    };

    class AbstractReadsApproxMatcher: public DefaultReadsMatcher {
    protected:
        uint8_t maxMismatches;
        uint8_t targetMismatches = 0;
        uint8_t minMismatches = 0;

        string pg;

        void writeMatchesInfo(ofstream &offsetsDest, ofstream &missedPatternsDest, ofstream &suffixesDest);

        SeparatedPseudoGenomeOutputBuilder *createSeparatedPseudoGenomeOutputBuilder(const string &outPgPrefix,
                                                                                     bool enableRevComp, bool enableMismatches) override;
        void initEntryUpdating() override;
        string currentRead;
        void updateEntry(DefaultReadsListEntry &entry, uint_reads_cnt_max matchIdx) override;
        void closeEntryUpdating() override;

    public:
        AbstractReadsApproxMatcher(const string &pgFilePrefix, bool revComplPg, PackedReadsSet *readsSet,
                                   uint32_t matchPrefixLength, uint8_t targetMismatches, uint8_t maxMismatches, uint8_t minMismatches = 0);

        virtual ~AbstractReadsApproxMatcher();
    };

    class DefaultReadsApproxMatcher: public AbstractReadsApproxMatcher {
    private:
        DefaultConstantLengthPatternsOnTextHashMatcher* hashMatcher = 0;

    protected:
        uint_read_len_max partLength;

        void initMatching();
        void matchConstantLengthReads(const char* txt, uint64_t length, bool revCompMode = false);

    public:
        DefaultReadsApproxMatcher(const string &pgFilePrefix, bool revComplPg, PackedReadsSet *readsSet,
                                  uint32_t matchPrefixLength, uint8_t targetMismatches, uint8_t maxMismatches, uint8_t minMismatches = 0);

        virtual ~DefaultReadsApproxMatcher();
    };

    class InterleavedReadsApproxMatcher: public AbstractReadsApproxMatcher {
    private:
        InterleavedConstantLengthPatternsOnTextHashMatcher* hashMatcher = 0;

    protected:
        uint_read_len_max partLength;

        void initMatching();
        void matchConstantLengthReads(const char* txt, uint64_t length, bool revCompMode = false);

    public:
        InterleavedReadsApproxMatcher(const string &pgFilePrefix, bool revComplPg, PackedReadsSet *readsSet,
                                  uint32_t matchPrefixLength, uint8_t targetMismatches, uint8_t maxMismatches, uint8_t minMismatches = 0);

        virtual ~InterleavedReadsApproxMatcher();
    };

    void mapReadsIntoPg(const string &pgFilePrefix, bool revComplPg, PackedReadsSet *readsSet,
                        uint_read_len_max matchPrefixLength, uint8_t targetMismatches, uint8_t maxMismatches,
                        char mismatchesMode, uint8_t minMismatches, bool dumpInfo, const string &pgDestFilePrefix,
                        const vector<uint_reads_cnt_max>& orgIndexesMapping, bool divisionComplement,
                        const string &outDivisionFile);
}

#endif //PGTOOLS_READSMATCHERS_H
