# PySparQ full-featured bindings.
#
# Added via add_subdirectory() from the SparQSim root CMakeLists. The C++
# core comes from the QRAM-Simulator submodule (target SparQ + global
# include directories wired by its root CMakeLists).

pybind11_add_module(_core core.cpp)
target_compile_definitions(_core PRIVATE QRAM_Release)
target_link_libraries(_core PRIVATE SparQ)

# The dynamic-operator loader is a self-contained dlopen/LoadLibrary wrapper;
# it only needs its own header (the generated operator code it loads at
# runtime is compiled against the installed headers by compiler.py).
pybind11_add_module(_dynamic_operator pysparq/dynamic_operator/src/dynamic_operator_loader.cpp)
target_include_directories(_dynamic_operator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pysparq/dynamic_operator/include
)

# The install directory is the output (wheel) directory
install(TARGETS _core DESTINATION pysparq)
install(TARGETS _dynamic_operator DESTINATION pysparq)
