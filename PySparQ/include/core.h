/**
 * @file core.h
 * @brief Auxiliary macro definitions for the PySparQ binding layer
 * @details Defines registration macros reused by the pybind11 binding
 *          module (core.cpp): batch binding of controllable methods
 *          (BIND_CONTROLLABLE_METHODS), operator class registration
 *          (BIND_BASE_OPERATOR / BIND_SELF_ADJOINT_OPERATOR / BIND_BASE_OPERATOR_SUBNAME),
 *          and dagger method binding (BIND_DAG_METHODS)
 */

#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/complex.h"
#include "pybind11/functional.h"
#include "pybind11/operators.h"
#include "pybind11/eigen.h"
#include "sparse_state_simulator.h"


using namespace pybind11::literals;
namespace py = pybind11;
using namespace qram_simulator;

using namespace std;

// Bind ClassControllable classes extra methods and attributes
/**
 * @brief Batch-bind the conditional control methods and condition-variable
 *        attributes of a controllable (ClassControllable) operator class
 * @details Expands to a single .def(...) chain containing: the four
 *          condition_variable_* read-only attributes, the four groups of
 *          overloaded condition-setting methods conditioned_by_nonzeros /
 *          conditioned_by_all_ones / conditioned_by_bit / conditioned_by_value,
 *          and the corresponding clear_control_* clearing methods
 * @param CLASS_NAME Name of the operator class whose controllable methods are to be bound
 */
#define BIND_CONTROLLABLE_METHODS(CLASS_NAME)                                                                                                                               \
    .def_readonly("condition_variable_nonzeros", &CLASS_NAME::condition_variable_nonzeros)                                                                                  \
        .def_readonly("condition_variable_all_ones", &CLASS_NAME::condition_variable_all_ones)                                                                              \
        .def_readonly("condition_variable_by_value", &CLASS_NAME::condition_variable_by_value)                                                                              \
        .def_readonly("condition_variable_by_bit", &CLASS_NAME::condition_variable_by_bit) /* conditioned_by_nonzeros */                                                    \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view)>(&CLASS_NAME::conditioned_by_nonzeros),                                   \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::string_view> &)>(&CLASS_NAME::conditioned_by_nonzeros),              \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t)>(&CLASS_NAME::conditioned_by_nonzeros),                                             \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<size_t> &)>(&CLASS_NAME::conditioned_by_nonzeros),                        \
             py::arg("conds"), py::return_value_policy::reference_internal) /* conditioned_by_all_ones */                                                                   \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view)>(&CLASS_NAME::conditioned_by_all_ones),                                   \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::string_view> &)>(&CLASS_NAME::conditioned_by_all_ones),              \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t)>(&CLASS_NAME::conditioned_by_all_ones),                                             \
             py::arg("cond"), py::return_value_policy::reference_internal)                                                                                                  \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<size_t> &)>(&CLASS_NAME::conditioned_by_all_ones),                        \
             py::arg("conds"), py::return_value_policy::reference_internal) /* conditioned_by_bit */                                                                        \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view, size_t)>(&CLASS_NAME::conditioned_by_bit),                                     \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<std::string_view, size_t>> &)>(&CLASS_NAME::conditioned_by_bit),     \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t, size_t)>(&CLASS_NAME::conditioned_by_bit),                                               \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<size_t, size_t>> &)>(&CLASS_NAME::conditioned_by_bit),               \
             py::arg("conds"), py::return_value_policy::reference_internal) /* conditioned_by_value */                                                                      \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view, size_t)>(&CLASS_NAME::conditioned_by_value),                                 \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<std::string_view, size_t>> &)>(&CLASS_NAME::conditioned_by_value), \
             py::arg("conds"), py::return_value_policy::reference_internal)                                                                                                 \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t, size_t)>(&CLASS_NAME::conditioned_by_value),                                           \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal)                                                                                  \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<size_t, size_t>> &)>(&CLASS_NAME::conditioned_by_value),           \
             py::arg("conds"), py::return_value_policy::reference_internal) /* clear methods */                                                                             \
        .def("clear_control_nonzeros", &CLASS_NAME::clear_control_nonzeros)                                                                                                 \
        .def("clear_control_all_ones", &CLASS_NAME::clear_control_all_ones)                                                                                                 \
        .def("clear_control_by_bit", &CLASS_NAME::clear_control_by_bit)                                                                                                     \
        .def("clear_control_by_value", &CLASS_NAME::clear_control_by_value)

/**
 * @brief Register an operator class derived from BaseOperator
 * @param NAME C++ class name (also used as the exported Python class name)
 */
#define BIND_BASE_OPERATOR(NAME) \
    py::class_<NAME, BaseOperator>(m, #NAME)

/* Workaround for binding inherited operator() of SelfAdjointOperator
*
    Author: Agony5757
    Date:   2025/2/26

   This is used to solve the issue for Windows platform which has a trouble if
   calling operator() in Python. However, the underlying mechanism is unclear
   to me, so I just use a workaround to avoid the issue. */
/**
 * @brief Register a self-adjoint operator class derived from SelfAdjointOperator
 * @details On Windows, additionally binds __call__ (operator()) explicitly to
 *          work around the issue with calling the inherited operator() from
 *          Python (see the English note above)
 * @param NAME C++ class name (also used as the exported Python class name)
 */
#ifdef _WIN32
#define BIND_SELF_ADJOINT_OPERATOR(NAME)            \
    py::class_<NAME, SelfAdjointOperator>(m, #NAME) \
        .def("__call__", (void (NAME::*)(SparseState &) const) & NAME::operator(), py::arg("state"))
#else
#define BIND_SELF_ADJOINT_OPERATOR(NAME) \
    py::class_<NAME, SelfAdjointOperator>(m, #NAME)
#endif

/**
 * @brief Register an operator class derived from BaseOperator (with a custom Python class name)
 * @param NAME C++ class name
 * @param PYNAME Exported Python class name (may differ from the C++ class name)
 */
#define BIND_BASE_OPERATOR_SUBNAME(NAME, PYNAME) \
    py::class_<NAME, BaseOperator>(m, PYNAME)

/**
 * @brief Bind the dagger (adjoint/inverse) method dag(state) for an operator class
 * @param NAME C++ class name
 */
#define BIND_DAG_METHODS(NAME) \
    .def("dag", (void (NAME::*)(SparseState &) const) & NAME::dag, py::arg("state"))
