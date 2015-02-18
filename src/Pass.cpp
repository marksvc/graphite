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
    If not, write to the Free Software Foundation, 51 Franklin Street,
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#include "inc/Main.h"
#include "inc/debug.h"
#include "inc/Endian.h"
#include "inc/Pass.h"
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <math.h>
#include "inc/Segment.h"
#include "inc/Code.h"
#include "inc/Rule.h"
#include "inc/Error.h"
#include "inc/Collider.h"

using namespace graphite2;
using vm::Machine;
typedef Machine::Code  Code;


Pass::Pass()
: m_silf(0),
  m_cols(0),
  m_rules(0),
  m_ruleMap(0),
  m_startStates(0),
  m_transitions(0),
  m_states(0),
  m_flags(0),
  m_iMaxLoop(0),
  m_numGlyphs(0),
  m_numRules(0),
  m_numStates(0),
  m_numTransition(0),
  m_numSuccess(0),
  m_numColumns(0),
  m_minPreCtxt(0),
  m_maxPreCtxt(0)
{
}

Pass::~Pass()
{
    free(m_cols);
    free(m_startStates);
    free(m_transitions);
    free(m_states);
    free(m_ruleMap);

    delete [] m_rules;
}

bool Pass::readPass(const byte * const pass_start, size_t pass_length, size_t subtable_base, GR_MAYBE_UNUSED Face & face, Error &e)
{
    const byte *                p = pass_start,
               * const pass_end   = p + pass_length;
    size_t numRanges;

    if (e.test(pass_length < 40, E_BADPASSLENGTH)) return face.error(e); 
    // Read in basic values
    m_flags = be::read<byte>(p);
    m_iMaxLoop = be::read<byte>(p);
    be::skip<byte>(p,2); // skip maxContext & maxBackup
    m_numRules = be::read<uint16>(p);
    if (e.test(!m_numRules && !(m_flags & 7), E_BADEMPTYPASS)) return face.error(e);
    be::skip<uint16>(p);   // fsmOffset - not sure why we would want this
    const byte * const pcCode = pass_start + be::read<uint32>(p) - subtable_base,
               * const rcCode = pass_start + be::read<uint32>(p) - subtable_base,
               * const aCode  = pass_start + be::read<uint32>(p) - subtable_base;
    be::skip<uint32>(p);
    m_numStates = be::read<uint16>(p);
    m_numTransition = be::read<uint16>(p);
    m_numSuccess = be::read<uint16>(p);
    m_numColumns = be::read<uint16>(p);
    numRanges = be::read<uint16>(p);
    be::skip<uint16>(p, 3); // skip searchRange, entrySelector & rangeShift.
    assert(p - pass_start == 40);
    // Perform some sanity checks.
    if ( e.test(m_numTransition > m_numStates, E_BADNUMTRANS)
            || e.test(m_numSuccess > m_numStates, E_BADNUMSUCCESS)
            || e.test(m_numSuccess + m_numTransition < m_numStates, E_BADNUMSTATES)
            || e.test(m_numRules && numRanges == 0, E_NORANGES))
        return face.error(e);

    m_successStart = m_numStates - m_numSuccess;
    if (e.test(p + numRanges * 6 - 4 > pass_end, E_BADPASSLENGTH)) return face.error(e);
    m_numGlyphs = be::peek<uint16>(p + numRanges * 6 - 4) + 1;
    // Calculate the start of various arrays.
    const byte * const ranges = p;
    be::skip<uint16>(p, numRanges*3);
    const byte * const o_rule_map = p;
    be::skip<uint16>(p, m_numSuccess + 1);

    // More sanity checks
    if (e.test(reinterpret_cast<const byte *>(o_rule_map + m_numSuccess*sizeof(uint16)) > pass_end
            || p > pass_end, E_BADRULEMAPLEN))
        return face.error(e);
    const size_t numEntries = be::peek<uint16>(o_rule_map + m_numSuccess*sizeof(uint16));
    const byte * const   rule_map = p;
    be::skip<uint16>(p, numEntries);

    if (e.test(p + 2*sizeof(uint8) > pass_end, E_BADPASSLENGTH)) return face.error(e);
    m_minPreCtxt = be::read<uint8>(p);
    m_maxPreCtxt = be::read<uint8>(p);
    if (e.test(m_minPreCtxt > m_maxPreCtxt, E_BADCTXTLENBOUNDS)) return face.error(e);
    const byte * const start_states = p;
    be::skip<int16>(p, m_maxPreCtxt - m_minPreCtxt + 1);
    const uint16 * const sort_keys = reinterpret_cast<const uint16 *>(p);
    be::skip<uint16>(p, m_numRules);
    const byte * const precontext = p;
    be::skip<byte>(p, m_numRules);
    be::skip<byte>(p);     // skip reserved byte

    if (e.test(p + sizeof(uint16) > pass_end, E_BADCTXTLENS)) return face.error(e);
    const size_t pass_constraint_len = be::read<uint16>(p);
    const uint16 * const o_constraint = reinterpret_cast<const uint16 *>(p);
    be::skip<uint16>(p, m_numRules + 1);
    const uint16 * const o_actions = reinterpret_cast<const uint16 *>(p);
    be::skip<uint16>(p, m_numRules + 1);
    const byte * const states = p;
    be::skip<int16>(p, m_numTransition*m_numColumns);
    be::skip<byte>(p);          // skip reserved byte
    if (e.test(p != pcCode, E_BADPASSCCODEPTR)) return face.error(e);
    be::skip<byte>(p, pass_constraint_len);
    if (e.test(p != rcCode, E_BADRULECCODEPTR)
        || e.test(size_t(rcCode - pcCode) != pass_constraint_len, E_BADCCODELEN)) return face.error(e);
    be::skip<byte>(p, be::peek<uint16>(o_constraint + m_numRules));
    if (e.test(p != aCode, E_BADACTIONCODEPTR)) return face.error(e);
    be::skip<byte>(p, be::peek<uint16>(o_actions + m_numRules));

    // We should be at the end or within the pass
    if (e.test(p > pass_end, E_BADPASSLENGTH)) return face.error(e);

    // Load the pass constraint if there is one.
    if (pass_constraint_len)
    {
        face.error_context(face.error_context() + 1);
        m_cPConstraint = vm::Machine::Code(true, pcCode, pcCode + pass_constraint_len, 
                                  precontext[0], be::peek<uint16>(sort_keys), *m_silf, face);
        if (e.test(!m_cPConstraint, E_OUTOFMEM)
                || e.test(m_cPConstraint.status(), m_cPConstraint.status() + E_CODEFAILURE))
            return face.error(e);
        face.error_context(face.error_context() - 1);
    }
    if (m_numRules)
    {
        if (!readRanges(ranges, numRanges, e)) return face.error(e);
        if (!readRules(rule_map, numEntries,  precontext, sort_keys,
                   o_constraint, rcCode, o_actions, aCode, face, e)) return false;
    }
#ifdef GRAPHITE2_TELEMETRY
    telemetry::category _states_cat(face.tele.states);
#endif
    return m_numRules ? readStates(start_states, states, o_rule_map, face, e) : true;
}


bool Pass::readRules(const byte * rule_map, const size_t num_entries,
                     const byte *precontext, const uint16 * sort_key,
                     const uint16 * o_constraint, const byte *rc_data,
                     const uint16 * o_action,     const byte * ac_data,
                     Face & face, Error &e)
{
    const byte * const ac_data_end = ac_data + be::peek<uint16>(o_action + m_numRules);
    const byte * const rc_data_end = rc_data + be::peek<uint16>(o_constraint + m_numRules);

    if (e.test(!(m_rules = new Rule [m_numRules]), E_OUTOFMEM)) return face.error(e);
    precontext += m_numRules;
    sort_key   += m_numRules;
    o_constraint += m_numRules;
    o_action += m_numRules;

    // Load rules.
    const byte * ac_begin = 0, * rc_begin = 0,
               * ac_end = ac_data + be::peek<uint16>(o_action),
               * rc_end = rc_data + be::peek<uint16>(o_constraint);
    Rule * r = m_rules + m_numRules - 1;
    for (size_t n = m_numRules; n; --n, --r, ac_end = ac_begin, rc_end = rc_begin)
    {
        face.error_context((face.error_context() & 0xFFFF00) + EC_ARULE + ((n - 1) << 24));
        r->preContext = *--precontext;
        r->sort       = be::peek<uint16>(--sort_key);
#ifndef NDEBUG
        r->rule_idx   = n - 1;
#endif
        if (r->sort > 63 || r->preContext >= r->sort || r->preContext > m_maxPreCtxt || r->preContext < m_minPreCtxt)
            return false;
        ac_begin      = ac_data + be::peek<uint16>(--o_action);
        --o_constraint;
        rc_begin      = be::peek<uint16>(o_constraint) ? rc_data + be::peek<uint16>(o_constraint) : rc_end;

        if (ac_begin > ac_end || ac_begin > ac_data_end || ac_end > ac_data_end
                || rc_begin > rc_end || rc_begin > rc_data_end || rc_end > rc_data_end)
            return false;
        r->action     = new vm::Machine::Code(false, ac_begin, ac_end, r->preContext, r->sort, *m_silf, face);
        r->constraint = new vm::Machine::Code(true,  rc_begin, rc_end, r->preContext, r->sort, *m_silf, face);

        if (e.test(!r->action || !r->constraint, E_OUTOFMEM)
                || e.test(r->action->status() != Code::loaded, r->action->status() + E_CODEFAILURE)
                || e.test(r->constraint->status() != Code::loaded, r->constraint->status() + E_CODEFAILURE)
                || e.test(!r->constraint->immutable(), E_MUTABLECCODE))
            return face.error(e);
    }

    // Load the rule entries map
    face.error_context((face.error_context() & 0xFFFF00) + EC_APASS);
    RuleEntry * re = m_ruleMap = gralloc<RuleEntry>(num_entries);
    if (e.test(!re, E_OUTOFMEM)) return face.error(e);
    for (size_t n = num_entries; n; --n, ++re)
    {
        const ptrdiff_t rn = be::read<uint16>(rule_map);
        if (e.test(rn >= m_numRules, E_BADRULENUM))  return face.error(e);
        re->rule = m_rules + rn;
    }

    return true;
}

static int cmpRuleEntry(const void *a, const void *b) { return (*(RuleEntry *)a < *(RuleEntry *)b ? -1 :
                                                                (*(RuleEntry *)b < *(RuleEntry *)a ? 1 : 0)); }

bool Pass::readStates(const byte * starts, const byte *states, const byte * o_rule_map, GR_MAYBE_UNUSED Face & face, Error &e)
{
#ifdef GRAPHITE2_TELEMETRY
    telemetry::category _states_cat(face.tele.starts);
#endif
    m_startStates = gralloc<uint16>(m_maxPreCtxt - m_minPreCtxt + 1);
#ifdef GRAPHITE2_TELEMETRY
    telemetry::set_category(face.tele.states);
#endif
    m_states      = gralloc<State>(m_numStates);
#ifdef GRAPHITE2_TELEMETRY
    telemetry::set_category(face.tele.transitions);
#endif
    m_transitions      = gralloc<uint16>(m_numTransition * m_numColumns);

    if (e.test(!m_startStates || !m_states || !m_transitions, E_OUTOFMEM)) return face.error(e);
    // load start states
    for (uint16 * s = m_startStates,
                * const s_end = s + m_maxPreCtxt - m_minPreCtxt + 1; s != s_end; ++s)
    {
        *s = be::read<uint16>(starts);
        if (e.test(*s >= m_numStates, E_BADSTATE))
        {
            face.error_context((face.error_context() & 0xFFFF00) + EC_ASTARTS + ((s - m_startStates) << 24));
            return face.error(e); // true;
        }
    }

    // load state transition table.
    for (uint16 * t = m_transitions,
                * const t_end = t + m_numTransition*m_numColumns; t != t_end; ++t)
    {
        *t = be::read<uint16>(states);
        if (e.test(*t >= m_numStates, E_BADSTATE))
        {
            face.error_context((face.error_context() & 0xFFFF00) + EC_ATRANS + (((t - m_transitions) / m_numColumns) << 24));
            return face.error(e);
        }
    }

    State * s = m_states,
          * const success_begin = m_states + m_numStates - m_numSuccess;
    const RuleEntry * rule_map_end = m_ruleMap + be::peek<uint16>(o_rule_map + m_numSuccess*sizeof(uint16));
    for (size_t n = m_numStates; n; --n, ++s)
    {
        RuleEntry * const begin = s < success_begin ? 0 : m_ruleMap + be::read<uint16>(o_rule_map),
                  * const end   = s < success_begin ? 0 : m_ruleMap + be::peek<uint16>(o_rule_map);

        if (e.test(begin >= rule_map_end || end > rule_map_end || begin > end, E_BADRULEMAPPING))
        {
            face.error_context((face.error_context() & 0xFFFF00) + EC_ARULEMAP + (n << 24));
            return face.error(e);
        }
        s->rules = begin;
        s->rules_end = (end - begin <= FiniteStateMachine::MAX_RULES)? end :
            begin + FiniteStateMachine::MAX_RULES;
        qsort(begin, end - begin, sizeof(RuleEntry), &cmpRuleEntry);
    }

    return true;
}

bool Pass::readRanges(const byte * ranges, size_t num_ranges, Error &e)
{
    m_cols = gralloc<uint16>(m_numGlyphs);
    if (e.test(!m_cols, E_OUTOFMEM)) return false;
    memset(m_cols, 0xFF, m_numGlyphs * sizeof(uint16));
    for (size_t n = num_ranges; n; --n)
    {
        uint16     * ci     = m_cols + be::read<uint16>(ranges),
                   * ci_end = m_cols + be::read<uint16>(ranges) + 1,
                     col    = be::read<uint16>(ranges);

        if (e.test(ci >= ci_end || ci_end > m_cols+m_numGlyphs || col >= m_numColumns, E_BADRANGE))
            return false;

        // A glyph must only belong to one column at a time
        while (ci != ci_end && *ci == 0xffff)
            *ci++ = col;

        if (e.test(ci != ci_end, E_BADRANGE))
            return false;
    }
    return true;
}


void Pass::runGraphite(Machine & m, FiniteStateMachine & fsm) const
{
    Slot *s = m.slotMap().segment.first();
    if (!s || !testPassConstraint(m)) return;
    if ((m_flags & 7))
    {
        if (!(m.slotMap().segment.flags() & Segment::SEG_INITCOLLISIONS))
        {
            m.slotMap().segment.positionSlots(0, 0, 0, true);
//            m.slotMap().segment.flags(m.slotMap().segment.flags() | Segment::SEG_INITCOLLISIONS);
        }
        if (!collisionAvoidance(&m.slotMap().segment, m.slotMap().segment.dir(), fsm.dbgout)) return;
    }
    if (!m_numRules) return;
    Slot *currHigh = s->next();

#if !defined GRAPHITE2_NTRACING
    if (fsm.dbgout)  *fsm.dbgout << "rules" << json::array;
    json::closer rules_array_closer(fsm.dbgout);
#endif

    m.slotMap().highwater(currHigh);
    int lc = m_iMaxLoop;
    do
    {
        if (!(m_flags & 7) || m.slotMap().segment.collisionInfo(s)->flags() & SlotCollision::COLL_ISCOL)
            findNDoRule(s, m, fsm);
        else
            s = s->next();
        if (s && (s == m.slotMap().highwater() || m.slotMap().highpassed() || --lc == 0)) {
            if (!lc)
                s = m.slotMap().highwater();
            lc = m_iMaxLoop;
            if (s)
                m.slotMap().highwater(s->next());
        }
    } while (s);
}

bool Pass::runFSM(FiniteStateMachine& fsm, Slot * slot) const
{
    fsm.reset(slot, m_maxPreCtxt);
    if (fsm.slots.context() < m_minPreCtxt)
        return false;

    uint16 state = m_startStates[m_maxPreCtxt - fsm.slots.context()];
    uint8  free_slots = SlotMap::MAX_SLOTS;
    do
    {
        fsm.slots.pushSlot(slot);
        if (--free_slots == 0
         || slot->gid() >= m_numGlyphs
         || m_cols[slot->gid()] == 0xffffU
         || state >= m_numTransition)
            return free_slots != 0;

        const uint16 * transitions = m_transitions + state*m_numColumns;
        state = transitions[m_cols[slot->gid()]];
        if (state >= m_successStart)
            fsm.rules.accumulate_rules(m_states[state]);

        slot = slot->next();
    } while (state != 0 && slot);

    fsm.slots.pushSlot(slot);
    return true;
}

#if !defined GRAPHITE2_NTRACING

inline
Slot * input_slot(const SlotMap &  slots, const int n)
{
    Slot * s = slots[slots.context() + n];
    if (!s->isCopied())     return s;

    return s->prev() ? s->prev()->next() : (s->next() ? s->next()->prev() : slots.segment.last());
}

inline
Slot * output_slot(const SlotMap &  slots, const int n)
{
    Slot * s = slots[slots.context() + n - 1];
    return s ? s->next() : slots.segment.first();
}

#endif //!defined GRAPHITE2_NTRACING

void Pass::findNDoRule(Slot * & slot, Machine &m, FiniteStateMachine & fsm) const
{
    assert(slot);

    if (runFSM(fsm, slot))
    {
        // Search for the first rule which passes the constraint
        const RuleEntry *        r = fsm.rules.begin(),
                        * const re = fsm.rules.end();
        while (r != re && !testConstraint(*r->rule, m)) ++r;

#if !defined GRAPHITE2_NTRACING
        if (fsm.dbgout)
        {
            if (fsm.rules.size() != 0)
            {
                *fsm.dbgout << json::item << json::object;
                dumpRuleEventConsidered(fsm, *r);
                if (r != re)
                {
                    const int adv = doAction(r->rule->action, slot, m);
                    dumpRuleEventOutput(fsm, *r->rule, slot);
                    if (r->rule->action->deletes()) fsm.slots.collectGarbage();
                    adjustSlot(adv, slot, fsm.slots);
                    *fsm.dbgout << "cursor" << objectid(dslot(&fsm.slots.segment, slot))
                            << json::close; // Close RuelEvent object

                    return;
                }
                else
                {
                    *fsm.dbgout << json::close  // close "considered" array
                            << "output" << json::null
                            << "cursor" << objectid(dslot(&fsm.slots.segment, slot->next()))
                            << json::close;
                }
            }
        }
        else
#endif
        {
            if (r != re)
            {
                const int adv = doAction(r->rule->action, slot, m);
                if (r->rule->action->deletes()) fsm.slots.collectGarbage();
                adjustSlot(adv, slot, fsm.slots);
                return;
            }
        }
    }

    slot = slot->next();
}

#if !defined GRAPHITE2_NTRACING

void Pass::dumpRuleEventConsidered(const FiniteStateMachine & fsm, const RuleEntry & re) const
{
    *fsm.dbgout << "considered" << json::array;
    for (const RuleEntry *r = fsm.rules.begin(); r != &re; ++r)
    {
        if (r->rule->preContext > fsm.slots.context())
            continue;
        *fsm.dbgout << json::flat << json::object
                    << "id" << r->rule - m_rules
                    << "failed" << true
                    << "input" << json::flat << json::object
                        << "start" << objectid(dslot(&fsm.slots.segment, input_slot(fsm.slots, -r->rule->preContext)))
                        << "length" << r->rule->sort
                        << json::close  // close "input"
                    << json::close; // close Rule object
    }
}


void Pass::dumpRuleEventOutput(const FiniteStateMachine & fsm, const Rule & r, Slot * const last_slot) const
{
    *fsm.dbgout     << json::item << json::flat << json::object
                        << "id"     << &r - m_rules
                        << "failed" << false
                        << "input" << json::flat << json::object
                            << "start" << objectid(dslot(&fsm.slots.segment, input_slot(fsm.slots, 0)))
                            << "length" << r.sort - r.preContext
                            << json::close // close "input"
                        << json::close  // close Rule object
                << json::close // close considered array
                << "output" << json::object
                    << "range" << json::flat << json::object
                        << "start"  << objectid(dslot(&fsm.slots.segment, input_slot(fsm.slots, 0)))
                        << "end"    << objectid(dslot(&fsm.slots.segment, last_slot))
                    << json::close // close "input"
                    << "slots"  << json::array;
    const Position rsb_prepos = last_slot ? last_slot->origin() : fsm.slots.segment.advance();
    fsm.slots.segment.positionSlots(0);

    for(Slot * slot = output_slot(fsm.slots, 0); slot != last_slot; slot = slot->next())
        *fsm.dbgout     << dslot(&fsm.slots.segment, slot);
    *fsm.dbgout         << json::close  // close "slots"
                    << "postshift"  << (last_slot ? last_slot->origin() : fsm.slots.segment.advance()) - rsb_prepos
                << json::close;         // close "output" object

}

#endif


inline
bool Pass::testPassConstraint(Machine & m) const
{
    if (!m_cPConstraint) return true;

    assert(m_cPConstraint.constraint());

    m.slotMap().reset(*m.slotMap().segment.first(), 0);
    m.slotMap().pushSlot(m.slotMap().segment.first());
    vm::slotref * map = m.slotMap().begin();
    const uint32 ret = m_cPConstraint.run(m, map);

#if !defined GRAPHITE2_NTRACING
    json * const dbgout = m.slotMap().segment.getFace()->logger();
    if (dbgout)
        *dbgout << "constraint" << (ret && m.status() == Machine::finished);
#endif

    return ret && m.status() == Machine::finished;
}


bool Pass::testConstraint(const Rule & r, Machine & m) const
{
    const uint16 curr_context = m.slotMap().context();
    if (unsigned(r.sort - r.preContext) > m.slotMap().size() - curr_context
        || curr_context - r.preContext < 0) return false;
    if (!*r.constraint) return true;
    assert(r.constraint->constraint());

    vm::slotref * map = m.slotMap().begin() + curr_context - r.preContext;
    for (int n = r.sort; n && map; --n, ++map)
    {
        if (!*map) continue;
        const int32 ret = r.constraint->run(m, map);
        if (!ret || m.status() != Machine::finished)
            return false;
    }

    return true;
}


void SlotMap::collectGarbage()
{
    for(Slot **s = begin(), *const *const se = end() - 1; s != se; ++s) {
        Slot *& slot = *s;
        if(slot->isDeleted() || slot->isCopied())
            segment.freeSlot(slot);
    }
}



int Pass::doAction(const Code *codeptr, Slot * & slot_out, vm::Machine & m) const
{
    assert(codeptr);
    if (!*codeptr) return 0;
    SlotMap   & smap = m.slotMap();
    vm::slotref * map = &smap[smap.context()];
    smap.highpassed(false);

    int32 ret = codeptr->run(m, map);

    if (m.status() != Machine::finished)
    {
        slot_out = NULL;
        smap.highwater(0);
        return 0;
    }

    slot_out = *map;
    return ret;
}


void Pass::adjustSlot(int delta, Slot * & slot_out, SlotMap & smap) const
{
    if (delta < 0)
    {
        if (!slot_out)
        {
            slot_out = smap.segment.last();
            ++delta;
            if (smap.highpassed() && !smap.highwater())
                smap.highpassed(false);
        }
        while (++delta <= 0 && slot_out)
        {
            if (smap.highpassed() && smap.highwater() == slot_out)
                smap.highpassed(false);
            slot_out = slot_out->prev();
        }
    }
    else if (delta > 0)
    {
        if (!slot_out)
        {
            slot_out = smap.segment.first();
            --delta;
        }
        while (--delta >= 0 && slot_out)
        {
            slot_out = slot_out->next();
            if (slot_out == smap.highwater() && slot_out)
                smap.highpassed(true);
        }
    }
}

bool Pass::collisionAvoidance(Segment *seg, int dir, json * const dbgout) const
{
    ShiftCollider shiftcoll;
    KernCollider kerncoll;
    bool isfirst = true;
    const uint8 numLoops = m_flags & 7;   // number of loops permitted to fix collisions; does not include kerning
    bool hasCollisions = false;
    bool hasKerns = false;
    Slot *start = seg->first();      // turn on collision fixing for the first slot
    Slot *end = NULL;
    bool moved = false;

#if !defined GRAPHITE2_NTRACING
    if (dbgout)
        *dbgout << "collisions" << json::array
            << json::flat << json::object << "num-loops" << numLoops << json::close;
    json::closer collisions_array_closer(dbgout);
#endif

    while (start)
    {
#if !defined GRAPHITE2_NTRACING
        if (dbgout)  *dbgout << json::object << "phase" << "1" << "moves" << json::array;
#endif
        hasCollisions = false;
        end = NULL;
        // phase 1 : position shiftable glyphs, ignoring kernable glyphs
        for (Slot *s = start; s; s = s->next())
        {
            const SlotCollision * c = seg->collisionInfo(s);
            if (start && (c->status() & SlotCollision::COLL_FIX) && !(c->flags() & SlotCollision::COLL_KERN))
                hasCollisions |= resolveCollisions(seg, s, start, shiftcoll, false, dir, moved, dbgout);
            else if (c->flags() & SlotCollision::COLL_KERN)
                hasKerns = true;
            if (c->flags() & SlotCollision::COLL_END)
            {
                end = s->next();
                break;
            }
        }

#if !defined GRAPHITE2_NTRACING
        if (dbgout)
            *dbgout << json::close << json::close; // phase-1
#endif

        // phase 2 : loop until happy. 
        for (int i = 0; i < numLoops - 1; ++i)
        {
            if (hasCollisions || moved)
            {

#if !defined GRAPHITE2_NTRACING
                if (dbgout)
                    *dbgout << json::object << "phase" << "2a" << "loop" << i << "moves" << json::array;
#endif
                // phase 2a : if any shiftable glyphs are in collision, iterate backwards,
                // fixing them and ignoring other non-collided glyphs. Note that this handles ONLY
                // glyphs that are actually in collision from phases 1 or 2b, and working backwards
                // has the intended effect of breaking logjams.
                if (hasCollisions)
                {
                    hasCollisions = false;
                    moved = false;
                    for (Slot *s = start; s != end; s = s->next())
                    {
                        SlotCollision * c = seg->collisionInfo(s);
                        c->setShift(Position(0, 0));
                    }
                    end = end ? end->prev() : seg->last();
                    for (Slot *s = end; s != start; s = s->prev())
                    {
                        const SlotCollision * c = seg->collisionInfo(s);
                        if (start && (c->status() & SlotCollision::COLL_FIX) && !(c->flags() & SlotCollision::COLL_KERN)
                                && (c->status() & SlotCollision::COLL_ISCOL)) // ONLY if this glyph is still colliding
                            hasCollisions |= resolveCollisions(seg, s, end, shiftcoll, true, dir, moved, dbgout);
                    }
                    end = end->next();
                }

#if !defined GRAPHITE2_NTRACING
                if (dbgout)
                    *dbgout << json::close << json::close // phase 2a
                        << json::object << "phase" << "2b" << "loop" << i << "moves" << json::array;
#endif

                // phase 2b : redo basic diacritic positioning pass for ALL glyphs. Each successive loop adjusts 
                // glyphs from their current adjusted position, which has the effect of gradually minimizing the  
                // resulting adjustment; ie, the final result will be gradually closer to the original location.  
                // Also it allows more flexibility in the final adjustment, since it is moving along the  
                // possible 8 vectors from successively different starting locations.
                if (moved)
                {
                    moved = false;
                    for (Slot *s = start; s != end; s = s->next())
                    {
                        const SlotCollision * c = seg->collisionInfo(s);
                        if (start && (c->status() & SlotCollision::COLL_FIX) && !(c->flags() & SlotCollision::COLL_KERN))
                            hasCollisions |= resolveCollisions(seg, s, start, shiftcoll, false, dir, moved, dbgout);
                    }
                }
        //      if (!hasCollisions) // no, don't leave yet because phase 2b will continue to improve things
        //          break;
#if !defined GRAPHITE2_NTRACING
                if (dbgout)
                    *dbgout << json::close << json::close; // phase 2
#endif
            }
        }
        start = NULL;
        for (Slot *s = end; s; s = s->next())
        {
            if (seg->collisionInfo(s)->flags() & SlotCollision::COLL_START)
            {
                start = s;
                break;
            }
        }
    }

    // phase 3 : handle kerning of clusters
#if !defined GRAPHITE2_NTRACING
    if (dbgout)
        *dbgout << json::object << "phase" << "3" << "moves" << json::array;
#endif
    if (hasKerns)
    {
        float currKern = 0;
        start = seg->first();
        for (Slot *s = seg->first(); s; s = s->next())
        {
            const SlotCollision * c = seg->collisionInfo(s);
            if (start && (c->flags() & SlotCollision::COLL_KERN) && (c->status() & SlotCollision::COLL_FIX))
                currKern = resolveKern(seg, s, start, kerncoll, dir, currKern, dbgout);
            if (c->flags() & SlotCollision::COLL_END)
                start = NULL;
            if (c->flags() & SlotCollision::COLL_START)
                start = s;
        }
    }
    for (Slot *s = seg->first(); s; s = s->next())
    {
        SlotCollision *c = seg->collisionInfo(s);
        const Position newOffset = c->shift();
        const Position nullPosition(0, 0);
        c->setOffset(newOffset + c->offset());
        c->setShift(nullPosition);
    }
    seg->positionSlots();
    
#if !defined GRAPHITE2_NTRACING
    if (dbgout)
        *dbgout << json::close << json::close; // phase 3
#endif
    
    return hasCollisions;   // but this isn't accurate if we have kerning
}

// Fix collisions for the given slot.
// Return true if everything was fixed, false if there are still collisions remaining.
// isRev means be we are processing backwards.
bool Pass::resolveCollisions(Segment *seg, Slot *slot, Slot *start,
        ShiftCollider &coll, GR_MAYBE_UNUSED bool isRev, int dir, bool &moved,
        json * const dbgout) const
{
    Slot *s;
    SlotCollision *cslot = seg->collisionInfo(slot);
    coll.initSlot(seg, slot, cslot->limit(), cslot->margin(), cslot->shift(), cslot->offset(), dir, dbgout);
    bool collides = false;
    bool ignoreForKern = !isRev; // ignore kernable glyphs that preceed the target glyph
    Position zero(0., 0.);
    
    // Look for collisions with the neighboring glyphs.
    for (s = start; s; s = isRev ? s->prev() : s->next())
    {
        SlotCollision *c = seg->collisionInfo(s);
        if (s != slot && !(c->status() & SlotCollision::COLL_IGNORE) 
                      && (!ignoreForKern || !(c->flags() & SlotCollision::COLL_KERN))
                      && (!isRev || !ignoreForKern || !(c->status() & SlotCollision::COLL_FIX) || (c->flags() & SlotCollision::COLL_KERN)))
            collides |= coll.mergeSlot(seg, s, c->shift(), dbgout);
        else if (s == slot)
            ignoreForKern = !ignoreForKern;
            
        if (s != start && (c->flags() & (isRev ? SlotCollision::COLL_START : SlotCollision::COLL_END)))
            break;
    }
    bool isCol;
    if (collides || cslot->shift().x != 0. || cslot->shift().y != 0.)
    {
        Position shift = coll.resolve(seg, isCol, dbgout);
        // isCol has been set to true if a collision remains.
        if (fabs(shift.x) < 1e38 && fabs(shift.y) < 1e38)
        {
            if (shift.x != cslot->shift().x || shift.y != cslot->shift().y)
                moved = true;
            cslot->setShift(shift);
        }
    }
    else
    {
        // This glyph is not colliding with anything.
        isCol = false;
        
#if !defined GRAPHITE2_NTRACING
        if (dbgout)
        {
            *dbgout << json::object 
                            << "missed" << objectid(dslot(seg, slot));
            coll.debug(dbgout, seg, -1);
            *dbgout << json::close;
        }
#endif
    }

    // Set the is-collision flag bit.
    if (isCol)
    { cslot->setStatus(cslot->status() | SlotCollision::COLL_ISCOL | SlotCollision::COLL_KNOWN); }
    else
    { cslot->setStatus((cslot->status() & ~SlotCollision::COLL_ISCOL) | SlotCollision::COLL_KNOWN); }
    return isCol;
}

float Pass::resolveKern(Segment *seg, Slot *slot, Slot *start, KernCollider &coll, int dir, float currKern,
    json *const dbgout) const
{
    Slot *s;
    float currGap = 0.;
    float currSpace = 0.;
    bool collides = false;
    SlotCollision *cslot = seg->collisionInfo(slot);
    bool seenEnd = cslot->flags() & SlotCollision::COLL_END;
    coll.initSlot(seg, slot, cslot->limit(), cslot->margin(), cslot->shift(), cslot->offset(), dir, dbgout);

    for (s = slot->next(); s; s = s->next())
    {
        SlotCollision *c = seg->collisionInfo(s);
        const Rect &bb = seg->theGlyphBBoxTemporary(s->gid());
        if (bb.bl.y == 0. && bb.tr.y == 0.)
            currSpace += s->advance();
        else if (s != slot && !(c->status() & SlotCollision::COLL_IGNORE) && !s->isChildOf(slot))
            collides |= coll.mergeSlot(seg, s, c->shift(), currSpace, dir, dbgout);
        if (c->flags() & SlotCollision::COLL_END)
        {
            if (seenEnd)
                break;
            else
                seenEnd = true;
        }
    }
    if (collides)
    {
        Position mv = coll.resolve(seg, dir, cslot->margin(), dbgout);
        cslot->setShift(mv);
        return mv.x;
    }
    return 0.;
}


// find cluster base
// find cluster ymin, ymax
// find bound for each slice (slice being cslot->margin() tall)
// iterate forward from slot to end + 1 (including slot as possible end)
    // for each slot is xmax - base.xmin > currmax, then skip
    // is it a space glyph, then subtract advance from mindiff. If mindiff < 0 then undo subtract, update advance and return
    // test each box/subbox against slice value in x
        // if possible test slice+1 and slice-1 at angle
        // if OK, perahps update mindiff
// return mindiff + currKern and update advance or whatever

