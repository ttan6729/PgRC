#ifndef PGTOOLS_EXTENDEDREADSLISTITERATORINTERFACE_H
#define PGTOOLS_EXTENDEDREADSLISTITERATORINTERFACE_H

#include <stdint.h>
#include <unistd.h>

#include "../../../pgsaconfig.h"

namespace PgTools {

    using namespace PgSAReadsSet;
    using namespace PgSAIndex;

    template <int maxMismatches, typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len>
    struct ReadsListEntry {
        uint_read_len pos;
        uint_read_len offset;
        uint_read_len length;
        bool revComp = false;
        uint8_t mismatchesCount = 0;
        uint8_t mismatchCode[maxMismatches];
        uint_read_len mismatchOffset[maxMismatches];

        ReadsListEntry(uint_read_len pos, uint_read_len length, bool revComp = false) : pos(pos), length(length),
                                                                                revComp(revComp) {
            offset = pos;
        }

        void addMismatch(uint8_t code, uint_read_len offset) {
            mismatchCode[mismatchesCount] = code;
            mismatchOffset[mismatchesCount++] = offset;
        }

        void advanceEntryByPosition(uint_read_len pos, bool revComp = false) {
            this->offset = pos - this->pos;
            this->revComp = revComp;
            this->mismatchesCount = 0;
        }

        void recedeEntryByPosition(uint_read_len pos, bool revComp = false) {
            this->offset = this->pos - pos;
            this->revComp = revComp;
            this->mismatchesCount = 0;
        }

        void advanceEntryByOffset(uint_read_len offset, bool revComp = false) {
            this->advanceEntryByPosition(this->pos + offset, revComp);
        }

        void recedeEntryByOffset(uint_read_len offset, bool revComp = false) {
            this->recedeEntryByPosition(this->pos - offset, revComp);
        }
    };

    template <int maxMismatches, typename uint_read_len, typename uint_reads_cnt, typename uint_pg_len>
    class ExtendedReadsListIteratorInterface {
    public:
        virtual ~ExtendedReadsListIteratorInterface() {};

        virtual bool moveNext() = 0;

        virtual const ReadsListEntry<maxMismatches, uint_read_len, uint_reads_cnt, uint_pg_len>& peekReadEntry();
    };

    typedef ReadsListEntry<UINT8_MAX,  uint_read_len_max, uint_reads_cnt_max, uint_pg_len_max> DefaultReadsListEntry;
    typedef ExtendedReadsListIteratorInterface<UINT8_MAX, uint_read_len_max, uint_reads_cnt_max, uint_pg_len_max> DefaultReadsListIteratorInterface;

}

#endif //PGTOOLS_EXTENDEDREADSLISTITERATORINTERFACE_H
