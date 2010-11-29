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

#include "graphiteng/Types.h"

namespace org { namespace sil { namespace graphite { namespace v2 {

class GrFont;
class Slot;
class GrSegment;

enum attrCode {
    kslatAdvX = 0, kslatAdvY,
    kslatAttTo,
    kslatAttX, kslatAttY, kslatAttGpt,
    kslatAttXOff, kslatAttYOff,
    kslatAttWithX, kslatAttWithY, kslatWithGpt,
    kslatAttWithXOff, kslatAttWithYOff,
    kslatAttLevel,
    kslatBreak,
    kslatCompRef,
    kslatDir,
    kslatInsert,
    kslatPosX, kslatPosY,
    kslatShiftX, kslatShiftY,
    kslatUserDefnV1,
    kslatMeasureSol, kslatMeasureEol,
    kslatJStretch, kslatJShrink, kslatJStep, kslatJWeight, kslatJWidth,
    
    kslatUserDefn = kslatJStretch + 30,
    
    kslatMax,
    kslatNoEffect = kslatMax + 1
};


extern "C"
{
    //slots are owned by their segment
    GRNG_EXPORT const Slot* slot_next_in_segment(const Slot* p/*not NULL*/);    //may give a base slot or a slot which is attached to another
    GRNG_EXPORT const Slot* slot_attached_to(const Slot* p/*not NULL*/);        //returns NULL iff base. If called repeatedly on result, will get to a base
    inline bool slot_is_base(const Slot* p/*not NULL*/) { return slot_attached_to(p)==NULL; }
 
    GRNG_EXPORT const Slot* slot_first_attachment(const Slot* p/*not NULL*/);        //returns NULL iff no attachments.
        //if slot_first_attachment(p) is not NULL, then slot_attached_to(slot_first_attachment(p))==p.
    
    GRNG_EXPORT const Slot* slot_next_sibling_attachment(const Slot* p/*not NULL*/);        //returns NULL iff no more attachments.
        //if slot_next_sibling_attachment(p) is not NULL, then slot_attached_to(slot_next_sibling_attachment(p))==slot_attached_to(p).
    
    
    GRNG_EXPORT unsigned short slot_gid(const Slot* p/*not NULL*/);
    GRNG_EXPORT float slot_origin_X(const Slot* p/*not NULL*/);
    GRNG_EXPORT float slot_origin_Y(const Slot* p/*not NULL*/);
    GRNG_EXPORT float slot_advance(const Slot* p/*not NULL*/, const GrFont *font);
    GRNG_EXPORT int slot_before(const Slot* p/*not NULL*/);
    GRNG_EXPORT int slot_after(const Slot* p/*not NULL*/);
    GRNG_EXPORT int slot_attr(const Slot* p/*not NULL*/, const GrSegment* pSeg/*not NULL*/, attrCode index, uint8 subindex); //tbd - do we need to expose this?
     
    GRNG_EXPORT bool slot_is_insert_before(const Slot* p/*not NULL*/);
    GRNG_EXPORT int slot_original(const Slot* p/*not NULL*/);
//  GRNG_EXPORT size_t id(const Slot* p/*not NULL*/);
}

}}}} // namespace