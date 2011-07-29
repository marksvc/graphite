/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, Inc., 59 Temple Place,
    Suite 330, Boston, MA 02111-1307, USA or visit their web page on the
    internet at http://www.fsf.org/licenses/lgpl.html.
*/
#include <graphite2/Segment.h>
#include "Main.h"
#include "Silf.h"
#include "Face.h"
#include "CachedFace.h"
#include "Segment.h"
#include "SegCache.h"
#include "SegCacheStore.h"
#include "processUTF.h"
#include "TtfTypes.h"
#include "TtfUtil.h"

using namespace graphite2;

inline gr_face * api_cast(CachedFace *p) { return static_cast<gr_face*>(static_cast<Face*>(p)); }

class CmapProcessor
{
public:
    CmapProcessor(Face * face, uint16 * buffer) :
        m_cmapTable(TtfUtil::FindCmapSubtable(face->getTable("cmap", NULL), 3, 1)),
        m_buffer(buffer), m_pos(0) {};
    bool processChar(uint32 cid, size_t /*offset*/)      //return value indicates if should stop processing
    {
        assert(cid < 0xFFFF); // only lower plane supported for this test
        m_buffer[m_pos++] = TtfUtil::Cmap31Lookup(m_cmapTable, cid);
        return true;
    }
    size_t charsProcessed() const { return m_pos; } //number of characters processed. Usually starts from 0 and incremented by processChar(). Passed in to LIMIT::needMoreChars
private:
    const void * m_cmapTable;
    uint16 * m_buffer;
    size_t m_pos;
};

bool checkEntries(CachedFace
 * face, const char * testString, uint16 * glyphString, size_t testLength)
{
    gr_feature_val * defaultFeatures = gr_face_featureval_for_lang(api_cast(face), 0);
    SegCache * segCache = face->cacheStore()->getOrCreate(0, *defaultFeatures);
    const SegCacheEntry * entry = segCache->find(glyphString, testLength);
    if (!entry)
    {
        unsigned int offset = 0;
        const char * space = strstr(testString + offset, " ");
        if (space)
        {
            while (space)
            {
                unsigned int wordLength = (space - testString) - offset;
                if (wordLength)
                {
                    entry = segCache->find(glyphString + offset, wordLength);
                    if (!entry)
                    {
                        fprintf(stderr, "failed to find substring at offset %u length %u in '%s'\n",
                                offset, wordLength, testString);
                        return false;
                    }
                }
                while (offset < (space - testString) + 1u)
                {
                    ++offset;
                }
                while (testString[offset] == ' ')
                {
                    ++offset;
                }
                space = strstr(testString + offset, " ");
            }
            if (offset < testLength)
            {
                entry = segCache->find(glyphString + offset, testLength - offset);
                if (!entry)
                {
                    fprintf(stderr, "failed to find last word at offset %u in '%s'\n",
                            offset, testString);
                    return false;
                }
            }
        }
        else
        {
            fprintf(stderr, "entry not found for '%s'\n", testString);
            return false;
        }
    }
    gr_featureval_destroy(defaultFeatures);
    return true;
}

bool testSeg(CachedFace
* face, const gr_font *sizedFont,
             const char * testString,
             size_t * testLength, uint16 ** testGlyphString)
{
    const void * badUtf8 = NULL;
    *testLength = gr_count_unicode_characters(gr_utf8, testString,
                                                    testString + strlen(testString),
                                                    &badUtf8);
    *testGlyphString = gralloc<uint16>(*testLength + 1);
    CharacterCountLimit limit(gr_utf8, testString, *testLength);
    CmapProcessor cmapProcessor(face, *testGlyphString);
    IgnoreErrors ignoreErrors;
    processUTF(limit, &cmapProcessor, &ignoreErrors);

    gr_segment * segA = gr_make_seg(sizedFont, api_cast(face), 0, NULL, gr_utf8, testString,
                        *testLength, 0);
    assert(segA);
    if ((gr_seg_n_slots(segA) == 0) ||
        !checkEntries(face, testString, *testGlyphString, *testLength))
        return false;
   return true;
}

int main(int argc, char ** argv)
{
    assert(sizeof(uintptr) == sizeof(void*));
    const char * fileName = NULL;
    if (argc > 1)
    {
        fileName = argv[1];
    }
    else
    {
        fprintf(stderr, "Usage: %s font.ttf\n", argv[0]);
        return 1;
    }
    FILE * log = fopen("grsegcache.xml", "w");
    graphite_start_logging(log, GRLOG_SEGMENT);
    CachedFace
 *face = static_cast<CachedFace
*>(static_cast<Face
*>(
        (gr_make_file_face_with_seg_cache(fileName, 10, gr_face_default))));
    if (!face)
    {
        fprintf(stderr, "Invalid font, failed to parse tables\n");
        return 3;
    }
    gr_font *sizedFont = gr_make_font(12, api_cast(face));
    const char * testStrings[] = { "a", "aa", "aaa", "aaab", "aaac", "a b c",
        "aaa ", " aa", "aaaf", "aaad", "aaaa"};
    uint16 * testGlyphStrings[sizeof(testStrings)/sizeof(char*)];
    size_t testLengths[sizeof(testStrings)/sizeof(char*)];
    const size_t numTestStrings = sizeof(testStrings) / sizeof (char*);
    
    for (size_t i = 0; i < numTestStrings; i++)
    {
        testSeg(face, sizedFont, testStrings[i], &(testLengths[i]), &(testGlyphStrings[i]));
    }
    gr_feature_val * defaultFeatures = gr_face_featureval_for_lang(api_cast(face), 0);
    SegCache * segCache = face->cacheStore()->getOrCreate(0, *defaultFeatures);
    unsigned int segCount = segCache->segmentCount();
    long long accessCount = segCache->totalAccessCount();
    if (segCount != 10 || accessCount != 16)
    {
        fprintf(stderr, "SegCache contains %u entries, which were used %Ld times\n",
            segCount, accessCount);
        return -2;
    }
    for (size_t i = 0; i < numTestStrings; i++)
    {
        if (!checkEntries(face, testStrings[i], testGlyphStrings[i], testLengths[i]))
            return -3;
    }
    segCount = segCache->segmentCount();
    accessCount = segCache->totalAccessCount();
    if (segCount != 10 || accessCount != 29)
    {
        fprintf(stderr, "SegCache after repeat contains %u entries, which were used %Ld times\n",
            segCount, accessCount);
        return -2;
    }
    // test purge
    size_t len = 0;
    uint16 * testGlyphString = NULL;
    testSeg(face, sizedFont, "ba", &len, &testGlyphString);
    segCount = segCache->segmentCount();
    accessCount = segCache->totalAccessCount();
    if (segCount > 10 || accessCount != 30)
    {
        fprintf(stderr, "SegCache after purge contains %u entries, which were used %Ld times\n",
            segCount, accessCount);
        return -2;
    }
    gr_font_destroy(sizedFont);
    gr_face_destroy(api_cast(face));
    gr_featureval_destroy(defaultFeatures);

    graphite_stop_logging();
    return 0;
}
