/**
 * @file BindUtils.h
 * @brief Common utilities for the PySparQ binding layer
 * @details Defines shared Python docstring constants (the pysparq_docs
 *          namespace) and registration macros reused by the pybind11 binding
 *          module (core.cpp): batch binding of controllable methods
 *          (BIND_CONTROLLABLE_METHODS), operator class registration
 *          (BIND_BASE_OPERATOR / BIND_SELF_ADJOINT_OPERATOR /
 *          BIND_BASE_OPERATOR_SUBNAME), and dagger method binding
 *          (BIND_DAG_METHODS)
 */

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/complex.h"
#include "pybind11/functional.h"
#include "pybind11/operators.h"
#include "pybind11/numpy.h"

#include "sparse_state_simulator.h"

using namespace pybind11::literals;
namespace py = pybind11;
using namespace qram_simulator;

using namespace std;

// ============================================================================
// Shared docstrings for controllable methods (used by 38+ operators)
// ============================================================================
/**
 * @brief Namespace of shared Python docstring constants
 * @details Centrally stores docstring text shared by dozens of operator
 *          classes (e.g. the controllable methods), referenced by the binding
 *          macros (BIND_CONTROLLABLE_METHODS / BIND_DAG_METHODS), avoiding
 *          repeated inline strings in many places
 */
namespace pysparq_docs {

/** @brief Shared docstring for the conditioned_by_nonzeros overloads (set the control condition on registers with nonzero values) */
inline constexpr const char* DOC_CONDITIONED_BY_NONZEROS =
    "Condition this operation on registers with nonzero values.\n\n"
    "Calling this method replaces prior nonzero-register conditions. Pass the\n"
    "list overload to require several registers simultaneously; conditions of\n"
    "different kinds are combined with logical AND.\n\n"
    "Args:\n"
    "    cond: Register name (str) or ID (int) to condition on.\n"
    "    conds: List of register names or IDs for multi-condition.\n\n"
    "Returns:\n"
    "    Self, for method chaining.\n\n"
    "Example:\n"
    "    op.conditioned_by_nonzeros('control_reg')(state)";

/** @brief Shared docstring for the conditioned_by_all_ones overloads (set the control condition on registers that are all ones) */
inline constexpr const char* DOC_CONDITIONED_BY_ALL_ONES =
    "Condition this operation on registers where all bits are 1.\n\n"
    "Calling this method replaces prior all-ones conditions. Pass the list\n"
    "overload to require several registers simultaneously; conditions of\n"
    "different kinds are combined with logical AND.\n\n"
    "Args:\n"
    "    cond: Register name (str) or ID (int) to condition on.\n"
    "    conds: List of register names or IDs for multi-condition.\n\n"
    "Returns:\n"
    "    Self, for method chaining.";

/** @brief Shared docstring for the conditioned_by_bit overloads (set the control condition on a specific bit position) */
inline constexpr const char* DOC_CONDITIONED_BY_BIT =
    "Condition this operation on a specific bit position.\n\n"
    "Calling this method replaces prior bit conditions. Pass the list-of-pairs\n"
    "overload to require several bits simultaneously; conditions of different\n"
    "kinds are combined with logical AND.\n\n"
    "Args:\n"
    "    cond: Register name (str) or ID (int).\n"
    "    pos: Bit position to check (0-indexed).\n"
    "    conds: List of (register, position) pairs.\n\n"
    "Returns:\n"
    "    Self, for method chaining.";

/** @brief Shared docstring for the conditioned_by_value overloads (set the control condition on registers holding a specific value) */
inline constexpr const char* DOC_CONDITIONED_BY_VALUE =
    "Condition this operation on registers holding a specific value.\n\n"
    "Calling this method replaces prior value conditions. Pass the list-of-pairs\n"
    "overload to require several values simultaneously; conditions of different\n"
    "kinds are combined with logical AND.\n\n"
    "Args:\n"
    "    cond: Register name (str) or ID (int).\n"
    "    pos: Value to match.\n"
    "    conds: List of (register, value) pairs.\n\n"
    "Returns:\n"
    "    Self, for method chaining.";

/** @brief Shared docstring for the dag method (adjoint/inverse operation) */
inline constexpr const char* DOC_DAG =
    "Apply the adjoint (inverse) of this operation.\n\n"
    "Args:\n"
    "    state: The quantum state to operate on.\n\n"
    "Note: Only available for self-adjoint operators.";

/** @brief Shared docstring for the clear_control_* family (clear control conditions of the specified type) */
inline constexpr const char* DOC_CLEAR_CONTROL =
    "Clear all control conditions of the specified type.";

} // namespace pysparq_docs

// Bind ClassControllable classes extra methods and attributes
/**
 * @brief Batch-bind the conditional control methods and condition-variable
 *        attributes of a controllable (ClassControllable) operator class
 * @details Expands to a single .def(...) chain containing: the four
 *          condition_variable_* read-only attributes, the four groups of
 *          overloaded condition-setting methods conditioned_by_nonzeros /
 *          conditioned_by_all_ones / conditioned_by_bit / conditioned_by_value
 *          (docstrings taken from the pysparq_docs namespace), and the
 *          corresponding clear_control_* clearing methods
 * @param CLASS_NAME Name of the operator class whose controllable methods are to be bound
 */
#define BIND_CONTROLLABLE_METHODS(CLASS_NAME)                                                                                                                               \
    .def_readonly("condition_variable_nonzeros", &CLASS_NAME::condition_variable_nonzeros)                                                                                  \
        .def_readonly("condition_variable_all_ones", &CLASS_NAME::condition_variable_all_ones)                                                                              \
        .def_readonly("condition_variable_by_value", &CLASS_NAME::condition_variable_by_value)                                                                              \
        .def_readonly("condition_variable_by_bit", &CLASS_NAME::condition_variable_by_bit) /* conditioned_by_nonzeros */                                                    \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view)>(&CLASS_NAME::conditioned_by_nonzeros),                                   \
             py::arg("cond"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_NONZEROS)                                                       \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::string_view> &)>(&CLASS_NAME::conditioned_by_nonzeros),              \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_NONZEROS)                                                     \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t)>(&CLASS_NAME::conditioned_by_nonzeros),                                             \
             py::arg("cond"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_NONZEROS)                                                      \
        .def("conditioned_by_nonzeros", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<size_t> &)>(&CLASS_NAME::conditioned_by_nonzeros),                        \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_NONZEROS) /* conditioned_by_all_ones */                       \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view)>(&CLASS_NAME::conditioned_by_all_ones),                                   \
             py::arg("cond"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_ALL_ONES)                                                      \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::string_view> &)>(&CLASS_NAME::conditioned_by_all_ones),              \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_ALL_ONES)                                                    \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t)>(&CLASS_NAME::conditioned_by_all_ones),                                             \
             py::arg("cond"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_ALL_ONES)                                                     \
        .def("conditioned_by_all_ones", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<size_t> &)>(&CLASS_NAME::conditioned_by_all_ones),                        \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_ALL_ONES) /* conditioned_by_bit */                           \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view, size_t)>(&CLASS_NAME::conditioned_by_bit),                                     \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_BIT)                                           \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<std::string_view, size_t>> &)>(&CLASS_NAME::conditioned_by_bit),     \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_BIT)                                                         \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t, size_t)>(&CLASS_NAME::conditioned_by_bit),                                               \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_BIT)                                          \
        .def("conditioned_by_bit", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<size_t, size_t>> &)>(&CLASS_NAME::conditioned_by_bit),               \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_BIT) /* conditioned_by_value */                              \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(std::string_view, size_t)>(&CLASS_NAME::conditioned_by_value),                                 \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_VALUE)                                         \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<std::string_view, size_t>> &)>(&CLASS_NAME::conditioned_by_value), \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_VALUE)                                                       \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(size_t, size_t)>(&CLASS_NAME::conditioned_by_value),                                           \
             py::arg("cond"), py::arg("pos"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_VALUE)                                        \
        .def("conditioned_by_value", static_cast<CLASS_NAME &(CLASS_NAME::*)(const std::vector<std::pair<size_t, size_t>> &)>(&CLASS_NAME::conditioned_by_value),           \
             py::arg("conds"), py::return_value_policy::reference_internal, pysparq_docs::DOC_CONDITIONED_BY_VALUE) /* clear methods */                                   \
        .def("clear_control_nonzeros", &CLASS_NAME::clear_control_nonzeros, pysparq_docs::DOC_CLEAR_CONTROL)                                                                \
        .def("clear_control_all_ones", &CLASS_NAME::clear_control_all_ones, pysparq_docs::DOC_CLEAR_CONTROL)                                                                \
        .def("clear_control_by_bit", &CLASS_NAME::clear_control_by_bit, pysparq_docs::DOC_CLEAR_CONTROL)                                                                    \
        .def("clear_control_by_value", &CLASS_NAME::clear_control_by_value, pysparq_docs::DOC_CLEAR_CONTROL)

// Macro with optional class docstring (variadic macro for backward compatibility)
/**
 * @brief Register an operator class derived from BaseOperator
 * @param NAME C++ class name (also used as the exported Python class name)
 * @param ... Optional class-level docstring (forwarded to the py::class_ constructor)
 */
#define BIND_BASE_OPERATOR(NAME, ...) \
    py::class_<NAME, BaseOperator>(m, #NAME, ##__VA_ARGS__)

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
 * @param ... Optional class-level docstring (forwarded to the py::class_ constructor)
 */
#ifdef _WIN32
#define BIND_SELF_ADJOINT_OPERATOR(NAME, ...)                                                                     \
    py::class_<NAME, SelfAdjointOperator>(m, #NAME, ##__VA_ARGS__)                                                \
        .def("__call__", (void (NAME::*)(SparseState &) const) & NAME::operator(), py::arg("state"))
#else
#define BIND_SELF_ADJOINT_OPERATOR(NAME, ...) \
    py::class_<NAME, SelfAdjointOperator>(m, #NAME, ##__VA_ARGS__)
#endif

/**
 * @brief Register an operator class derived from BaseOperator (with a custom Python class name)
 * @param NAME C++ class name
 * @param PYNAME Exported Python class name (may differ from the C++ class name)
 * @param ... Optional class-level docstring (forwarded to the py::class_ constructor)
 */
#define BIND_BASE_OPERATOR_SUBNAME(NAME, PYNAME, ...) \
    py::class_<NAME, BaseOperator>(m, #PYNAME, ##__VA_ARGS__)

/**
 * @brief Bind the dagger (adjoint/inverse) method dag(state) for an operator class
 * @param NAME C++ class name
 */
#define BIND_DAG_METHODS(NAME) \
    .def("dag", (void (NAME::*)(SparseState &) const) & NAME::dag, py::arg("state"), pysparq_docs::DOC_DAG)
