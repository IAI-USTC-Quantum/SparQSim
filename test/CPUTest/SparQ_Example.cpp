#include "sparse_state_simulator.h"

using namespace qram_simulator;

int example_1() {
    /***********    
    * Example 1
     * Covers allocating a quantum state, allocating quantum registers, and applying quantum gate operations to the quantum state.
     *
     * 1. First, create a SparseState object, which is used to represent the quantum state.
     * 2. Then, allocate the quantum registers you need and initialize them.
     * 3. When calling a quantum gate operation, you need to construct a Callable, which is a function object that operates on the SparseState object.
     *
     * Refer to the README for the definitions of the specific operations.
     **********/

    // Create an empty quantum state
    SparseState s;

    // 1. Start by allocating the quantum registers you need
    // 1) Allocate a register with 2 bits of type UnsignedInteger
    auto reg0 = AddRegister("reg0", UnsignedInteger, 2)(s);
    // 2) Allocate a register with 1 bit of type Boolean
    auto reg1 = AddRegister("reg1", Boolean, 1)(s);
    // 3) Print its current state
    (StatePrint(Detail))(s);

    // The output is as follows:
    // StatePrint (mode=Detail)
    // |(0)reg0 : UInt2 | |(1)reg1 : Bool1 |   (this line shows the register information)
    // 1.000000+0.000000i  reg0=|0> reg1=|false> (from this line on, each quantum state component is enumerated)

    // 2. Start applying operations to the registers
    // 1) Apply a Hadamard gate to reg0: first create a Hadamard_Int_Full object via the argument list; this object is a Callable that can operate on the SparseState object.
    (Hadamard_Int_Full(reg0))(s);

    // 2) Apply a Pauli-X gate to reg1: likewise, create an X_Bool object and apply it to reg2.
    (X_Bool(reg1))(s);

    // 3. Print its current state
    (StatePrint(Detail))(s);

    // The output is as follows:
    // StatePrint (mode=Detail)
    // | (0)reg0 : UInt2 | |(1)reg1 : Bool1 |
    // 0.500000 + 0.000000i  reg0 = | 0 > reg1 = | true >
    // 0.500000 + 0.000000i  reg0 = | 1 > reg1 = | true >
    // 0.500000 + 0.000000i  reg0 = | 2 > reg1 = | true >
    // 0.500000 + 0.000000i  reg0 = | 3 > reg1 = | true >
    
    // reg0's quantum state has been acted on by the Hadamard gate, and reg1's quantum state has been acted on by the Pauli-X gate.

    return 0;
}

int example_2() {
    /***********
     * Example 2
     *
     * Shows how to add control operations to quantum operations.
     * In total, there are 4 kinds of control operations
     * conditioned_by_all_ones : control on all bits of the register being 1
     * conditioned_by_bit : control on a specified bit of the register being 1
     * conditioned_by_nonzeros : control on the register having at least one non-zero bit
     * conditioned_by_value : control on the value of specified bits of the register; e.g., for a 3-bit register, it can be controlled to be any value from 0 to 7
     *
    **********/

    {
        /* Example 2.1 */
        // Create an empty quantum state
        SparseState s;

        /* Allocate two registers of type General, with 2 and 1 bits respectively. */
        auto reg0 = AddRegister("reg0", General, 2)(s);
        auto reg1 = AddRegister("reg1", General, 1)(s);

        /*
        reg0 (0) -- X -- C --
        reg0 (1) ------- C --
        reg1 (0) ------- X --
        */

        // Apply an X gate to bit 0 of reg0, and a CCX (Toffoli) gate to bit 0 of reg1
        (X_Bool(reg0, 0))(s);
        (X_Bool(reg1).conditioned_by_all_ones(reg0))(s);

        // Print the quantum state
        (StatePrint(Detail))(s);

        // The output is as follows:
        // StatePrint (mode=Detail)
        // | (0)reg0 : Reg2 | |(1)reg1 : Reg1 |
        // 1.000000 + 0.000000i  reg0 = | 01 > reg1 = | 0 >
        // This means the X gate on reg1 is controlled on all bits of reg0 being 1; since reg0 is |01>, the X gate on reg1 did not act.
    }

    /* The System::clear() function can be used to clear all quantum register information in the system.*/
    System::clear();

    {
        /* Example 2.2 */
        // Create an empty quantum state
        SparseState s;

        /* Allocate two registers of type General, with 3 and 1 bits respectively. */
        auto reg0 = AddRegister("reg0", General, 3)(s);
        auto reg1 = AddRegister("reg1", General, 1)(s);

        /*
        reg0 (0) -- X -- C --
        reg0 (1) ------------
        reg0 (2) ---X----C --
        reg1 (0) ------- X --
        */

        // Apply an X gate to bit 0 of reg0, and a CCX (Toffoli) gate to bit 0 of reg1
        (X_Bool(reg0, 0))(s);
        (X_Bool(reg0, 2))(s);
        (X_Bool(reg1).conditioned_by_bit({ {reg0, 0}, {reg0, 2} }))(s);

        // Print the quantum state
        (StatePrint(Detail))(s);

        // The output is as follows:
        // StatePrint (mode=Detail)
        // | (0)reg0 : Reg3 | |(1)reg1 : Reg1 |
        // 1.000000 + 0.000000i  reg0 = | 101 > reg1 = | 1 >
        // This means the X gate on reg1 is controlled on bits 0 and 2 of reg0 being 1; since reg0 is |101>, the X gate on reg1 acted.
    }
    return 0;
}


int main() {
    //example_1();
    example_2();
    return 0;
}