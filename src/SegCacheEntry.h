/*  GRAPHITENG LICENSING

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
#pragma once

#include "Main.h"

namespace org { namespace sil { namespace graphite { namespace v2 {

class GrSegment;
class Slot;
class SegCacheEntry;
class SegCachePrefixEntry;

typedef enum {
    /** number of characters used in initial prefix tree */
    ePrefixLength =2,
    /** Segments are purged according to the formular:
     * accessCount < (totalAccesses)/(ePurgeFactor * maxSegments) */
    ePurgeFactor = 5,
    /** Maximum number of Segments to store which have the same
     * prefix. Needed to prevent unique identifiers flooding the cache */
    eMaxSuffixCount = 15,
    /** sub-Segments longer than this are not cached
        * (in Unicode code points) */
    eMaxCachedSeg = 16
} SegCacheLength;

class SegCacheCharInfo
{
public:
    uint16 m_unicode;
    uint16 m_before;
    uint16 m_after;
};

/**
 * SegCacheEntry stores the result of running the engine for specific unicode
 * code points in the typical mid-line situation.
 */
class SegCacheEntry
{
    friend class SegCachePrefixEntry;
public:
    SegCacheEntry() :
        m_glyphLength(0), m_charInfo(NULL), m_glyph(NULL), m_attr(NULL),
        m_accessCount(0), m_lastAccess(0)
    {}
    SegCacheEntry(const uint16 * cmapGlyphs, size_t length, GrSegment * seg, size_t charOffset, long long cacheTime);
    ~SegCacheEntry() { clear(); };
    void clear();
    size_t glyphLength() const { return m_glyphLength; }
    const Slot * first() const { return m_glyph; }
    const Slot * last() const { return m_glyph + (m_glyphLength - 1); }

    void log(size_t unicodeLength) const;
    /** Total number of times this entry has been accessed since creation */
    unsigned long long accessCount() const { return m_accessCount; }
    /** "time" of last access where "time" is measured in accesses to the cache owning this entry */
    void accessed(unsigned long long cacheTime) const
    {
        m_lastAccess = cacheTime; ++m_accessCount;
    };

    int compareRank(const SegCacheEntry & entry) const
    {
        if (m_accessCount > entry.m_accessCount) return 1;
        else if (m_accessCount < entry.m_accessCount) return 1;
        else if (m_lastAccess > entry.m_lastAccess) return 1;
        else if (m_lastAccess < entry.m_lastAccess) return -1;
        return 0;
    }
    long unsigned int lastAccess() const { return m_lastAccess; };

    CLASS_NEW_DELETE
private:

    size_t m_glyphLength;
    /** glyph ids resulting from cmap mapping from unicode to glyph before substitution
     * the length of this array is determined by the position in the SegCachePrefixEntry */
    SegCacheCharInfo * m_charInfo;
    /** slots after shapping and positioning */
    Slot * m_glyph;
    uint16 * m_attr;
    mutable unsigned long long m_accessCount;
    mutable unsigned long long m_lastAccess;
};

}}}} // end namespace