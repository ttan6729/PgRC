#include "SimplePgMatcher.h"

#include "../pseudogenome/persistence/SeparatedPseudoGenomePersistence.h"

namespace PgTools {

    SimplePgMatcher::SimplePgMatcher(const string& srcPgPrefix, uint32_t minMatchLength)
            :srcPgPrefix(srcPgPrefix), targetMatchLength(minMatchLength), minMatchLength(minMatchLength) {

        cout << "Reading source pseudogenome..." << endl;
        srcPg = PgTools::SeparatedPseudoGenomePersistence::getPseudoGenome(srcPgPrefix);
        cout << "Pseudogenome length: " << srcPg.length() << endl;

        matcher = new DefaultTextMatcher(srcPg, minMatchLength);
    }

    SimplePgMatcher::~SimplePgMatcher() {
        delete(matcher);
    }

    void SimplePgMatcher::exactMatchPg() {
        bool destPgIsSrcPg = srcPgPrefix == targetPgPrefix;

        if (!destPgIsSrcPg) {
            cout << "Reading target pseudogenome..." << endl;

            destPg = PgTools::SeparatedPseudoGenomePersistence::getPseudoGenome(targetPgPrefix);
            cout << "Pseudogenome length: " << destPg.length() << endl;
        } else
            destPg = srcPg;

        if (revComplMatching)
            destPg = PgSAHelpers::reverseComplement(destPg);

        matcher->matchTexts(textMatches, destPg, destPgIsSrcPg, revComplMatching, targetMatchLength);

        if (revComplMatching) {
            correctDestPositionDueToRevComplMatching();
            destPg = destPgIsSrcPg?srcPg:PgSAHelpers::reverseComplement(destPg);
        }
    }

    using namespace PgTools;

    void SimplePgMatcher::correctDestPositionDueToRevComplMatching() {
        for (TextMatch& match: textMatches)
            match.posDestText = destPg.length() - (match.posDestText + match.length);
    }

    string SimplePgMatcher::getTotalMatchStat(uint_pg_len_max totalMatchLength) {
        return toString(totalMatchLength) + " (" + toString((totalMatchLength * 100.0) / destPg.length(), 1)+ "%)";
    }

    void SimplePgMatcher::markAndRemoveExactMatches(const string &destPgFilePrefix, bool revComplMatching) {
        this->targetPgPrefix = destPgFilePrefix;
        this->revComplMatching = revComplMatching;

        exactMatchPg();

        if (srcPgPrefix == destPgFilePrefix)
            resolveMappingCollisionsInTheSameText();

        ofstream pgDest = SeparatedPseudoGenomePersistence::getPseudoGenomeElementDest(destPgFilePrefix, SeparatedPseudoGenomePersistence::PSEUDOGENOME_FILE_SUFFIX, true);
        ofstream pgMapOffDest = SeparatedPseudoGenomePersistence::getPseudoGenomeElementDest(destPgFilePrefix, SeparatedPseudoGenomePersistence::PSEUDOGENOME_MAPPING_OFFSETS_FILE_SUFFIX, true);
        ofstream pgMapLenDest = SeparatedPseudoGenomePersistence::getPseudoGenomeElementDest(destPgFilePrefix, SeparatedPseudoGenomePersistence::PSEUDOGENOME_MAPPING_LENGTHS_FILE_SUFFIX, true);

        sort(textMatches.begin(), textMatches.end(), [this](const TextMatch& match1, const TextMatch& match2) -> bool
        { return match1.posDestText < match2.posDestText; });
        uint_pg_len_max pos = 0;
        uint_pg_len_max totalDestOverlap = 0;
        uint_pg_len_max totalMatched = 0;
        bool isPgLengthStd = srcPg.length() <= UINT32_MAX;
        for(TextMatch& match: textMatches) {
            if (match.posDestText < pos) {
                uint_pg_len_max overflow = pos - match.posDestText;
                if (overflow > match.length) {
                    totalDestOverlap += match.length;
                    match.length = 0;
                    continue;
                }
                totalDestOverlap += overflow;
                match.length -= overflow;
                match.posDestText += overflow;
                if (!revComplMatching)
                    match.posSrcText += overflow;
            }
            if (match.length < minMatchLength) {
                totalDestOverlap += match.length;
                continue;
            }
            totalMatched += match.length;
            PgSAHelpers::writeArray(pgDest, (void*) (destPg.data() + pos), match.posDestText - pos);
            pgDest.put(128);
            if (isPgLengthStd)
                PgSAHelpers::writeValue<uint32_t>(pgMapOffDest, match.posSrcText);
            else
                PgSAHelpers::writeValue<uint64_t>(pgMapOffDest, match.posSrcText);
            PgSAHelpers::writeUIntByteFrugal(pgMapLenDest, match.length - minMatchLength);
            pos = match.endPosDestText();
        }
        PgSAHelpers::writeArray(pgDest, (void*) (destPg.data() + pos), destPg.length() - pos);
        pgDest.close();
        pgMapOffDest.close();
        pgMapLenDest.close();
        SeparatedPseudoGenomePersistence::acceptTemporaryPseudoGenomeElements(destPgFilePrefix, false);

        cout << "Final size of Pg: " << (destPg.length() - totalMatched) << " (removed: " <<
             getTotalMatchStat(totalMatched) << "; " << totalDestOverlap << " chars in overlapped dest symbol)" << endl;
    }

    void SimplePgMatcher::resolveMappingCollisionsInTheSameText() {
        for (TextMatch& match: textMatches) {
            if (match.posSrcText > match.posDestText) {
                uint64_t tmp = match.posSrcText;
                match.posSrcText = match.posDestText;
                match.posDestText = tmp;
            }
            if (revComplMatching && match.endPosSrcText() > match.posDestText) {
                uint64_t margin = (match.endPosSrcText() - match.posDestText + 1) / 2;
                match.length -= margin;
                match.posDestText += margin;
            }
        }
    }

    void SimplePgMatcher::matchPgInPgFiles(const string &goodPgPrefix, const string &badPgPrefix, uint_pg_len_max targetMatchLength,
                         bool revComplMatching) {

        PgTools::SimplePgMatcher matcher(goodPgPrefix, targetMatchLength);
        matcher.markAndRemoveExactMatches(goodPgPrefix, revComplMatching);
        matcher.markAndRemoveExactMatches(badPgPrefix, revComplMatching);
    }
}

