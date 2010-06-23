#ifndef PASS_INCLUDE
#define PASS_INCLUDE

#include "VMScratch.h"
#include "code.h"

class Segment;
class FontImpl;
class FontFace;
class Silf;

class Pass
{
public:
    bool readPass(void *pPass, size_t lPass, int numGlyphs);
    void runGraphite(Segment *seg, FontFace *face, Silf *silf, VMScratch *vms);
    int findNDoRule(Segment *seg, int iSlot, VMScratch *vms, Silf *silf);
    int testConstraint(code *codeptr, int iSlot, Segment *seg, Silf *silf, VMScratch *vms);
    int doAction(code *m_cAction, int startSlot, Segment *seg, Silf *silf, VMScratch *vms);
    void init(Silf *silf) { m_silf = silf; }

protected:
    Silf *m_silf;
    byte m_iMaxLoop;
    uint16 m_numRules;
    uint16 m_sRows;
    uint16 m_sTransition;
    uint16 m_sSuccess;
    uint16 m_sColumns;
    uint16 *m_cols;
    uint16 *m_ruleidx;
    uint16 *m_ruleMap;
    uint16 *m_ruleSorts;
    byte m_minPreCtxt;
    byte m_maxPreCtxt;
    uint16 *m_startStates;
    byte *m_rulePreCtxt;
    code m_cPConstraint;
    code *m_cConstraint;
    code *m_cActions;
    int16 *m_sTable;
};

#endif