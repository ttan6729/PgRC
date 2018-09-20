#include "matcher.h"

#include "ConstantLengthPatternsOnTextHashMatcher.h"
#include "../readsset/PackedReadsSet.h"


void exactMatchConstantLengthPatterns(string text, string readsFile, ofstream& offsetsDest,
                                      ofstream& missedPatternsDest) {
    clock_checkpoint();
    cout << "Reading reads set\n";
    PackedReadsSet* readsSet = PackedReadsSet::readReadsSet(readsFile);
    readsSet->printout();
    cout << "... checkpoint " << clock_millis() << " msec. " << endl;
    cout << "Feeding patterns...\n" << endl;
    const uint_read_len_max readLength = readsSet->readLength(0);
    ConstantLengthPatternsOnTextHashMatcher hashMatcher(readLength);
    const uint_reads_cnt_max readsCount = readsSet->readsCount();
    for(uint_reads_cnt_max i = 0; i < readsCount; i++)
        hashMatcher.addPattern(readsSet->getRead(i).data(), i);
    cout << "... checkpoint " << clock_millis() << " msec. " << endl;
    cout << "Matching...\n" << endl;
    hashMatcher.iterateOver(text.data(), text.length());

    vector<uint32_t> readMatchPos(readsCount, UINT32_MAX);
    
    int i = 0;
    uint32_t matchCount = 0;
    uint64_t multiMatchCount = 0;
    uint64_t falseMatchCount = 0;
    while (hashMatcher.moveNext()) {
        const uint64_t matchPosition = hashMatcher.getHashMatchTextPosition();
        const uint32_t matchReadIndex = hashMatcher.getHashMatchPatternIndex();
        const string &matchedRead = readsSet->getRead(matchReadIndex);

        bool exactMatch = strncmp(text.data() + matchPosition, matchedRead.data(), readLength) == 0;
        if (exactMatch) {
            if (readMatchPos[matchReadIndex] == UINT32_MAX) {
                readMatchPos[matchReadIndex] = matchPosition;
                matchCount++;
            } else
                multiMatchCount++;
        } else
            falseMatchCount++;
      if (i++ < 1) {
            cout << "Matched: " << matchReadIndex << "; "
                 << matchPosition << "; " << exactMatch << endl;
            cout << matchedRead << endl;
            const basic_string<char, char_traits<char>, allocator<char>> &pgPart = text.substr(matchPosition, readLength);
            cout << pgPart << endl;
        }
    }
    cout << "... finished matching in  " << clock_millis() << " msec. " << endl;
    cout << "Exact matched " << matchCount << " reads (" << (readsCount - matchCount)
         << " left; " << multiMatchCount << " multi-matches). False matches reported: " << falseMatchCount << "." << endl;

    cout << "Writing output files...\n" << endl;
    for(uint_reads_cnt_max i = 0; i < readsCount; i++) {
        if (readMatchPos[i] == UINT32_MAX)
            missedPatternsDest << readsSet->getRead(i).data() << "\n";
        else
            offsetsDest << i << "\t" << readMatchPos[i] << "\n";
    }

    cout << "... matching and writing output files completed in  " << clock_millis() << " msec. " << endl;

    delete(readsSet);
}

uint8_t countMismatches(const char *pattern, const char *text, uint64_t length, uint8_t max_mismatches) {
    uint8_t res = 0;
    const char* patEnd = pattern + length;
    while (pattern != patEnd) {
        if (*pattern++ != *text++) {
            if (res++ >= max_mismatches)
                return UINT8_MAX;
        }
    }

    return res;
}

void reportMismatches(const char *read, const char *pgPart, const uint_read_len_max length, ofstream &mismatchesDest) {
    uint64_t pos = 0;
    do {
        if (read[pos] != pgPart[pos])
            mismatchesDest << "\t" << pos << "\t" << read[pos];
    } while (++pos < length);
}

void approxMatchConstantLengthPatterns(string text, string readsFile, ofstream& offsetsDest, uint8_t max_mismatches,
                                       ofstream& missedPatternsDest) {
    uint8_t min_mismatches = 1;

    clock_checkpoint();
    cout << "Reading reads set\n";
    PackedReadsSet* readsSet = PackedReadsSet::readReadsSet(readsFile);
    readsSet->printout();
    cout << "... checkpoint " << clock_millis() << " msec. " << endl;
    cout << "Feeding patterns...\n" << endl;
    const uint_read_len_max readLength = readsSet->readLength(0);
    const uint_read_len_max partLength = readLength / (max_mismatches + 1);
    ConstantLengthPatternsOnTextHashMatcher hashMatcher(partLength);
    const uint_reads_cnt_max readsCount = readsSet->readsCount();
    for(uint_reads_cnt_max i = 0; i < readsCount; i++) {
        const char *read = readsSet->getRead(i).data();
        for(uint8_t j = 0; j <= max_mismatches; j++, read += partLength)
            hashMatcher.addPattern(read, i * (max_mismatches + 1) + j);
    }
    cout << "... checkpoint " << clock_millis() << " msec. " << endl;
    cout << "Matching...\n" << endl;
    hashMatcher.iterateOver(text.data(), text.length());

    vector<uint32_t> readMatchPos(readsCount, UINT32_MAX);
    vector<uint8_t> readMismatches(readsCount, UINT8_MAX);

    int i = 0;
    uint32_t matchCount = 0;
    uint64_t falseMatchCount = 0;
    uint64_t multiMatchCount = 0;
    while (hashMatcher.moveNext()) {
        const uint32_t matchIndex = hashMatcher.getHashMatchPatternIndex();
        uint32_t matchReadIndex = matchIndex / (max_mismatches + 1);
        if (readMismatches[matchReadIndex] <= min_mismatches)
            continue;
        const string &matchedRead = readsSet->getRead(matchReadIndex);
        uint64_t matchPosition = hashMatcher.getHashMatchTextPosition();
        const uint8_t positionShift = ((matchIndex % (max_mismatches + 1)) * partLength);
        if (positionShift > matchPosition)
            continue;
        matchPosition -= positionShift;
        if (matchPosition + readLength > text.length())
            continue;

        uint8_t mismatchesCount = countMismatches(matchedRead.data(), text.data() + matchPosition, readLength, max_mismatches);
        if (mismatchesCount < readMismatches[matchReadIndex]) {
            readMatchPos[matchReadIndex] = matchPosition;
            readMismatches[matchReadIndex] = mismatchesCount;
            matchCount++;
        } else if (mismatchesCount == UINT8_MAX)
            falseMatchCount++;
        else
            multiMatchCount++;
        if (mismatchesCount < UINT8_MAX && i++ < 2) {
            cout << "Matched: " << matchIndex << "; "
                 << matchPosition << "; " << (int) mismatchesCount << endl;
            cout << matchedRead << endl;
            const basic_string<char, char_traits<char>, allocator<char>> &pgPart = text.substr(matchPosition, readLength);
            cout << pgPart << endl;
        }
    }
    cout << "... finished matching in  " << clock_millis() << " msec. " << endl;
    cout << "Matched " << matchCount << " reads (" << (readsCount - matchCount)
         << " left; " << multiMatchCount << " multi-matches). False matches reported: " << falseMatchCount << "." << endl;

    cout << "Writing output files...\n" << endl;
    vector<uint_reads_cnt_max> mismatchedReadsCount(max_mismatches + 1, 0);
    for(uint_reads_cnt_max i = 0; i < readsCount; i++) {
        if (readMatchPos[i] == UINT32_MAX)
            missedPatternsDest << readsSet->getRead(i).data() << "\n";
        else {
            offsetsDest << i << "\t" << readMatchPos[i];
            reportMismatches(readsSet->getRead(i).data(), text.data() + readMatchPos[i], readLength, offsetsDest);
            offsetsDest << "\n";
            mismatchedReadsCount[readMismatches[i]]++;
        }
    }

    for(uint8_t i = 0; i <= max_mismatches; i++)
        cout << "Matched " << mismatchedReadsCount[i] << " reads with " << (int) i << " mismatches." << endl;

    cout << "... matching and writing output files completed in  " << clock_millis() << " msec. " << endl;

    delete(readsSet);
}

