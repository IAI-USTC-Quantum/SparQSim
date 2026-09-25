/**
 * @file BindUtils.h
 * @brief PySparQ 绑定层公共工具定义
 * @details 定义 pybind11 绑定模块（core.cpp）复用的共享 Python docstring
 *          常量（pysparq_docs 命名空间）与注册宏：可控方法批量绑定
 *          （BIND_CONTROLLABLE_METHODS）、算子类注册（BIND_BASE_OPERATOR /
 *          BIND_SELF_ADJOINT_OPERATOR / BIND_BASE_OPERATOR_SUBNAME）
 *          以及 dagger 方法绑定（BIND_DAG_METHODS）
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
 * @brief 共享 Python docstring 常量命名空间
 * @details 集中存放可控方法等被数十个算子类共用的 docstring 文本，
 *          供各绑定宏（BIND_CONTROLLABLE_METHODS / BIND_DAG_METHODS）引用，
 *          避免在多处重复内联字符串
 */
namespace pysparq_docs {

/** @brief conditioned_by_nonzeros 系列重载的共享 docstring（按寄存器非零值设置控制条件） */
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

/** @brief conditioned_by_all_ones 系列重载的共享 docstring（按寄存器全 1 设置控制条件） */
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

/** @brief conditioned_by_bit 系列重载的共享 docstring（按指定位位置设置控制条件） */
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

/** @brief conditioned_by_value 系列重载的共享 docstring（按寄存器取值设置控制条件） */
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

/** @brief dag 方法（伴随/逆操作）的共享 docstring */
inline constexpr const char* DOC_DAG =
    "Apply the adjoint (inverse) of this operation.\n\n"
    "Args:\n"
    "    state: The quantum state to operate on.\n\n"
    "Note: Only available for self-adjoint operators.";

/** @brief clear_control_* 系列（清除指定类型控制条件）的共享 docstring */
inline constexpr const char* DOC_CLEAR_CONTROL =
    "Clear all control conditions of the specified type.";

} // namespace pysparq_docs

// Bind ClassControllable classes extra methods and attributes
/**
 * @brief 批量绑定可控（ClassControllable）算子类的条件控制方法与条件变量属性
 * @details 展开为一条 .def(...) 链，包含：
 *          四个 condition_variable_* 只读属性、conditioned_by_nonzeros /
 *          conditioned_by_all_ones / conditioned_by_bit / conditioned_by_value
 *          四组多载条件设置方法（docstring 取自 pysparq_docs 命名空间），
 *          以及对应的 clear_control_* 清除方法
 * @param CLASS_NAME 待绑定可控方法的算子类名
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
 * @brief 注册一个派生自 BaseOperator 的算子类
 * @param NAME C++ 类名（同时作为 Python 侧导出的类名）
 * @param ... 可选的类级 docstring（透传给 py::class_ 构造）
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
 * @brief 注册一个派生自 SelfAdjointOperator 的自伴算子类
 * @details Windows 平台下额外显式绑定 __call__（operator()），
 *          以绕开继承的 operator() 在 Python 调用时出现的问题（见上方英文说明）
 * @param NAME C++ 类名（同时作为 Python 侧导出的类名）
 * @param ... 可选的类级 docstring（透传给 py::class_ 构造）
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
 * @brief 注册一个派生自 BaseOperator 的算子类（自定义 Python 类名）
 * @param NAME C++ 类名
 * @param PYNAME Python 侧导出的类名（可与 C++ 类名不同）
 * @param ... 可选的类级 docstring（透传给 py::class_ 构造）
 */
#define BIND_BASE_OPERATOR_SUBNAME(NAME, PYNAME, ...) \
    py::class_<NAME, BaseOperator>(m, #PYNAME, ##__VA_ARGS__)

/**
 * @brief 为算子类绑定 dagger（伴随/逆）方法 dag(state)
 * @param NAME C++ 类名
 */
#define BIND_DAG_METHODS(NAME) \
    .def("dag", (void (NAME::*)(SparseState &) const) & NAME::dag, py::arg("state"), pysparq_docs::DOC_DAG)
