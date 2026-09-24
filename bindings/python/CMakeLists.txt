# Thin Python bindings for the qram_simulator package (moved here from the
# QRAM-Simulator core repo). pybind11 is found by the root CMakeLists.
#
# The target is renamed to qram_simulator_core to avoid clashing with
# PySparQ's _core target; the module FILE keeps the name _core so the
# Python package layout stays `qram_simulator/_core*.pyd`.

pybind11_add_module(qram_simulator_core core_binding.cpp)
set_target_properties(qram_simulator_core PROPERTIES OUTPUT_NAME _core)
target_compile_definitions(qram_simulator_core PRIVATE QRAM_Release)
target_link_libraries(qram_simulator_core PRIVATE SparQ)

# The install directory is the output (wheel) directory
install(TARGETS qram_simulator_core DESTINATION qram_simulator)
