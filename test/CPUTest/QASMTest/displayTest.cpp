#include "displayTest.h"

void Phase_Int::display() const
{
    std::cout << "Phase_Int display function with lambda: " << lambda << std::endl;
}
void Zgate_Int::display() const
{
    //Phase_Int::display(); // explicitly call Phase_Int's display function
    std::cout << "Zgate_Int display function." << std::endl;
}

int main() {
    Zgate_Int z_gate("q0", 1);

    // Call the display function
    z_gate.display();  // Output: Phase_Int display function... Zgate_Int display function...

    // Use a Phase_Int pointer to point to a Zgate_Int instance
    Phase_Int* phase_ptr = &z_gate;
    phase_ptr->display(); // Output: Phase_Int display function... Zgate_Int display function...

    // Use a GateBase pointer to point to a Zgate_Int instance
    GateBase* base_ptr = &z_gate;
    base_ptr->display(); // Output: Phase_Int display function... Zgate_Int display function...

    return 0;
}

