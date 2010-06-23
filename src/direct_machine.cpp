// This direct threaded interpreter implmentation for machine.h
// Author: Tim Eves

// Build either this interpreter or the call_machine implementation.
// The direct threaded interpreter is relies upon a gcc feature called 
// labels-as-values so is only portable to compilers that support the 
// extension (gcc only as far as I know) however it should build on any
// architecture gcc supports. 
// This is twice as fast as the call threaded model and is likely faster on 
// inorder processors with short pipelines and little branch prediction such 
// as the ARM and possibly Atom chips.


#include <cassert>
#include "machine.h"


#define STARTOP(name)           name: {
#if defined(CHECK_STACK)
#define ENDOP                   }; if (!machine::check_stack(sp, stack_base, stack_top)) \
                                        goto end; \
                                goto **++ip;
#else
#define ENDOP                   }; goto **++ip;
#endif
#define EXIT(status)            push(status); goto end

#define action(name,param_sz)   {&&name, param_sz}

namespace {

const void * direct_machine(const bool get_table_mode,
                            const bool constrained,
                            const instr   * program,
                            const byte    * data,
                            uint32        * stack_base, 
                            const size_t    length,
                            Segment * const seg_ptr,
                            int  &          is,
                            machine::status_t &     status)
{
    // We need to define and return to opcode table from within this function 
    // other inorder to take the addresses of the instruction bodies.
    #include "opcode_table.h"
    if (get_table_mode)
        return constrained ? opcode_table_constrained : opcode_table;

    // Declare virtual machine registers
    const instr   * ip = program;
    const byte    * dp = data;
    // We give enough guard space so that one instruction can over/under flow 
    // the stack and cause no damage this condition will then be caught by
    // check_stack.
    uint32        * sp        = stack_base + length - 2;
    uint32 * const  stack_top = stack_base + 2;
    Segment &       seg = *seg_ptr;
    stack_base = sp;
    
    // start the program
    goto **ip;

    // Pull in the opcode definitions
    #include "opcodes.h"
    
    end:
    machine::check_final_stack(sp, stack_base-1, stack_top, status);
    return reinterpret_cast<const void *>(*sp);
}

}

const opcode_t * machine::get_opcode_table(bool constraint) throw()
{
    machine::status_t dummy;
    int is_dummy;
    return static_cast<const opcode_t *>(direct_machine(true, constraint, 0, 0, 0, 0, 0, is_dummy, dummy));
}


uint32  machine::run(const instr  * program,
                     const byte   * data,
                     uint32       * stack_base, 
                     const size_t   length,
                     Segment & seg, 
                     int &          islot_idx,
                     status_t &     status)
{
    assert(program != 0);
    assert(data != 0);
    assert(stack_base !=0);
    assert(length > 8);
    
    const void * ret = direct_machine(false, false, program, data,
                                      stack_base, length, &seg, 
                                      islot_idx, status);
    return reinterpret_cast<ptrdiff_t>(ret);
}


         
