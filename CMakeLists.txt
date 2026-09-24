# SparQSim — the pysparq Python package.
#
# The C++ core is consumed from the qram-simulator git submodule (relative
# URL ../qram-simulator.git, resolves on both Gitea and GitHub). scikit-build
# core drives this file when building the wheel; the core is embedded via
# add_subdirectory so everything compiles in one CMake invocation, exactly
# like the former monorepo build.
cmake_minimum_required(VERSION 3.18)

project(SparQSim LANGUAGES CXX)

# Turn off everything the core repo builds for its own development; these
# must be cache FORCE so option() inside the core does not re-default them.
set(QRAM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QRAM_BUILD_EXPERIMENTS OFF CACHE BOOL "" FORCE)
set(QRAM_BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/qram-simulator)
add_subdirectory(PySparQ)
