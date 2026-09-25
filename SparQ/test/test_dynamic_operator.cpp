/**
 * @file test_dynamic_operator.cpp
 * @brief Unit tests for the dynamic operator loader
 *
 * @section design_rationale Design Background
 *
 * This test file verifies the low-level mechanism for dynamically compiling C++ operators.
 * The dynamic operator feature allows users to write custom C++ code at runtime,
 * compile it into a shared library, and load and execute it in the simulator.
 *
 * @section why_cpp_tests Why C++ Tests Are Needed
 *
 * Although the dynamic operator feature primarily serves Python users (via the pysparq.dynamic_operator module),
 * C++ tests are retained for the following reasons:
 *
 * 1. **Unit test layering principle**: The Python tests (test_dynamic_operator.py) test the complete feature chain,
 *    while the C++ tests focus on the low-level compile/load mechanism, which makes issue localization easier.
 *
 * 2. **Development debugging tool**: When a problem appears at the Python layer, the C++ tests can verify
 *    whether the low level works correctly.
 *
 * @section why_disabled Why Disabled by Default
 *
 * These tests are disabled by default (the environment variable ENABLE_DYNAMIC_OPERATOR_TEST=1 must be set to run them),
 * for the following reasons:
 *
 * 1. **Windows ABI incompatibility**: The main program is compiled with MSVC, while dynamic compilation uses MinGW g++,
 *    and the two C++ ABIs are incompatible, which leads to runtime crashes (SEGFAULT).
 *
 * 2. **CI environment differences**: Compiler versions and standard library versions may differ across CI environments,
 *    causing compiled artifacts to be incompatible with the main program.
 *
 * 3. **Functionality already covered by Python tests**: The Python test test_dynamic_operator.py
 *    already fully verifies user-level functionality; the C++ tests are mainly for low-level debugging.
 *
 * @section test_cases Test Contents
 *
 * - Simple SelfAdjointOperator extension
 * - BaseOperator extension with parameters
 * - Compilation error handling
 * - Cache mechanism
 * - dagger operation correctness
 */

// Windows compatibility
#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>

// Platform-specific dynamic library headers
#ifndef _WIN32
#include <dlfcn.h>
#define POPEN popen
#define PCLOSE pclose
#else
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#endif

// We directly test the dynamic_operator_loader implementation
#include "basic_components.h"
#include "basic_gates.h"
using namespace qram_simulator;

// ============ Helper functions ============

/**
 * @brief Create a temporary C++ source file
 * @details Uses the process ID as a prefix to avoid file name conflicts in parallel tests
 */
std::string create_temp_source_file(const std::string& code, const std::string& filename) {
    std::string temp_dir = std::filesystem::temp_directory_path().string();
    // Use the process ID as a prefix to avoid conflicts between parallel tests
    static std::string pid_prefix;
    if (pid_prefix.empty()) {
#ifndef _WIN32
        pid_prefix = std::to_string(getpid()) + "_";
#else
        pid_prefix = std::to_string(GetCurrentProcessId()) + "_";
#endif
    }
    std::string filepath = temp_dir + "/" + pid_prefix + filename;
    std::ofstream file(filepath);
    file << code;
    file.close();
    return filepath;
}

/**
 * @brief Find the project root directory
 * @details In CI environments, the test executable may run inside the build/ directory,
 *          so we need to search upward until the project root directory is found
 */
std::string find_project_root() {
    // Method 1: check the environment variable (highest priority)
    const char* env_root = std::getenv("PROJECT_ROOT");
    if (env_root) {
        std::string root(env_root);
        if (std::filesystem::exists(root + "/SparQ/include")) {
            std::cerr << "[find_project_root] Using PROJECT_ROOT: " << root << std::endl;
            return root;
        }
    }

    // Method 2: check the GITHUB_WORKSPACE environment variable (CI environment)
    const char* github_workspace = std::getenv("GITHUB_WORKSPACE");
    if (github_workspace) {
        std::string root(github_workspace);
        if (std::filesystem::exists(root + "/SparQ/include")) {
            std::cerr << "[find_project_root] Using GITHUB_WORKSPACE: " << root << std::endl;
            return root;
        }
    }
    
    // Method 2b: check the source directory in the CI environment (github.workspace)
    const char* ci_workspace = std::getenv("CI_WORKSPACE");
    if (ci_workspace) {
        std::string root(ci_workspace);
        if (std::filesystem::exists(root + "/SparQ/include")) {
            std::cerr << "[find_project_root] Using CI_WORKSPACE: " << root << std::endl;
            return root;
        }
    }

    // Method 3: search upward from the current executable path
    std::filesystem::path current = std::filesystem::current_path();
    
    // Try searching upward from the current directory for at most 5 levels
    for (int i = 0; i < 5; ++i) {
        // Check whether this is the project root directory (contains SparQ/include and Common/include)
        if (std::filesystem::exists(current / "SparQ" / "include") &&
            std::filesystem::exists(current / "Common" / "include")) {
            return current.string();
        }
        
        // Check whether this is a build directory (inside a subdirectory such as build/ or build/Release/)
        // Search upward until we find a directory containing SparQ/CMakeLists.txt or pyproject.toml
        if (std::filesystem::exists(current / "SparQ" / "CMakeLists.txt") ||
            std::filesystem::exists(current / "CMakeLists.txt")) {
            // Go up one more level; this may go from build/ to the project root directory
            if (current.has_parent_path()) {
                auto parent = current.parent_path();
                if (std::filesystem::exists(parent / "SparQ" / "include")) {
                    return parent.string();
                }
            }
        }
        
        // Check for source-directory signature files
        if (std::filesystem::exists(current / "pyproject.toml") &&
            std::filesystem::exists(current / "SparQ")) {
            return current.string();
        }
        
        // Move up one level
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    // Method 4: try a reverse lookup based on source-directory signatures
    // Check whether we are in a structure like build/SparQ/test/
    current = std::filesystem::current_path();
    std::filesystem::path candidate = current;
    
    // Strip the known build-directory levels
    while (candidate.has_parent_path()) {
        std::string filename = candidate.filename().string();
        // If this is a known build directory, check the parent directory
        if (filename == "test" || filename == "SparQ" || filename == "build" ||
            filename == "Release" || filename == "Debug") {
            candidate = candidate.parent_path();
            if (std::filesystem::exists(candidate / "SparQ" / "include")) {
                return candidate.string();
            }
            continue;
        }
        break;
    }

    // Default: return the current directory, but log a warning
    std::cerr << "[find_project_root] Warning: Could not find project root!" << std::endl;
    std::cerr << "[find_project_root] Current directory: " << std::filesystem::current_path() << std::endl;
    std::cerr << "[find_project_root] Checking for SparQ/include: " 
              << std::filesystem::exists(std::filesystem::current_path() / "SparQ" / "include") << std::endl;
    
    // List the current directory contents for debugging
    std::cerr << "[find_project_root] Directory contents:" << std::endl;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
        std::cerr << "  - " << entry.path().filename().string() << std::endl;
    }
    
    return ".";
}

/**
 * @brief Check whether a compiler is available
 */
bool is_compiler_available() {
#ifdef _WIN32
    // Windows: check for g++ (MinGW)
    FILE* pipe = POPEN("g++ --version 2>&1", "r");
    if (pipe) {
        PCLOSE(pipe);
        return true;
    }
    return false;
#else
    // Unix: check for g++
    FILE* pipe = POPEN("g++ --version 2>&1", "r");
    if (pipe) {
        PCLOSE(pipe);
        return true;
    }
    return false;
#endif
}

/**
 * @brief Compile C++ code into a shared library
 * @details Uses the process ID as a prefix to avoid file name conflicts in parallel tests
 */
std::string compile_to_shared_lib(const std::string& source_path, const std::string& lib_name) {
    // First check whether a compiler is available
    if (!is_compiler_available()) {
        std::cerr << "No suitable C++ compiler found for dynamic operator test" << std::endl;
        return "";
    }

    // Use the process ID as a prefix to avoid conflicts between parallel tests
    static std::string pid_prefix;
    if (pid_prefix.empty()) {
#ifndef _WIN32
        pid_prefix = std::to_string(getpid()) + "_";
#else
        pid_prefix = std::to_string(GetCurrentProcessId()) + "_";
#endif
    }

    std::string temp_dir = std::filesystem::temp_directory_path().string();
    std::string lib_path = temp_dir + "/" + pid_prefix + lib_name;

    // Find the project root directory
    std::string project_root = find_project_root();

    // Build the compilation command
    std::string cmd;
#ifdef _WIN32
    // Windows (MinGW): need .dll extension and different flags
    lib_path += ".dll";
    cmd = "g++ -std=c++17 -O2 -shared ";
    cmd += "-I\"" + project_root + "/SparQ/include\" ";
    cmd += "-I\"" + project_root + "/Common/include\" ";
    cmd += "-I\"" + project_root + "/QRAM/include\" ";
    cmd += "-I\"" + project_root + "/ThirdParty/eigen-3.4.0\" ";
    cmd += "-I\"" + project_root + "/ThirdParty/fmt/include\" ";
    cmd += "-o \"" + lib_path + "\" \"" + source_path + "\" 2>&1";
#else
    // Unix/Linux/macOS
    cmd = "g++ -std=c++17 -O2 -fPIC -shared ";
    cmd += "-I" + project_root + "/SparQ/include ";
    cmd += "-I" + project_root + "/Common/include ";
    cmd += "-I" + project_root + "/QRAM/include ";
    cmd += "-I" + project_root + "/ThirdParty/eigen-3.4.0 ";
    cmd += "-I" + project_root + "/ThirdParty/fmt/include ";
    cmd += "-o " + lib_path + " " + source_path + " 2>&1";
#endif

    // Execute the compilation
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        return "";
    }

    char buffer[128];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    PCLOSE(pipe);

    if (!std::filesystem::exists(lib_path)) {
        std::cerr << "Compilation failed: " << output << std::endl;
        return "";
    }

    return lib_path;
}

// Cross-platform dynamic library loading
class TestDynamicLoader {
public:
    void* handle_ = nullptr;
    std::string lib_path_;
    
    explicit TestDynamicLoader(const std::string& lib_path) : lib_path_(lib_path) {
#ifdef _WIN32
        handle_ = static_cast<void*>(LoadLibraryA(lib_path.c_str()));
#else
        handle_ = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    }
    
    ~TestDynamicLoader() {
        if (handle_) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle_));
#else
            dlclose(handle_);
#endif
        }
    }
    
    bool is_valid() const { return handle_ != nullptr; }
    
    void* get_symbol(const std::string& name) {
        if (!handle_) return nullptr;
#ifdef _WIN32
        HMODULE hModule = static_cast<HMODULE>(handle_);
        FARPROC proc = GetProcAddress(hModule, name.c_str());
        return reinterpret_cast<void*>(proc);
#else
        return dlsym(handle_, name.c_str());
#endif
    }
};

// ============ Test fixture ============

class DynamicOperatorTest : public ::testing::Test {
protected:
    // Get the PID prefix of the current process (only clean up files created by this process)
    static std::string get_pid_prefix() {
        static std::string prefix;
        if (prefix.empty()) {
#ifndef _WIN32
            prefix = std::to_string(getpid()) + "_";
#else
            prefix = std::to_string(GetCurrentProcessId()) + "_";
#endif
        }
        return prefix;
    }

    /**
     * @brief Check whether the dynamic compilation tests can run
     *
     * @return true if the environment variable ENABLE_DYNAMIC_OPERATOR_TEST=1 is set and a compiler is available
     * @return false returned by default (the tests are skipped)
     *
     * @section skip_reason Skip Reasons
     *
     * Dynamic compilation tests have serious compatibility problems in different environments:
     *
     * 1. **Windows ABI incompatibility**:
     *    - The main program is compiled with MSVC (the Windows build job in CI)
     *    - Dynamic compilation uses MinGW g++ (the compiler available on the system)
     *    - The MSVC and MinGW C++ ABIs are incompatible, which causes:
     *      - Mismatched memory layouts
     *      - Different exception handling mechanisms
     *      - Different run-time type information (RTTI) formats
     *    - Result: a SEGFAULT occurs when calling functions after loading the DLL
     *
     * 2. **Linux/Unix environment differences**:
     *    - The g++ version in the CI environment may differ from the one used to compile the main program
     *    - libstdc++ version differences may cause symbol resolution failures
     *
     * 3. **Solution**:
     *    - Skip these tests by default to keep CI stable
     *    - Python tests (test_dynamic_operator.py) already cover user-facing functionality
     *    - Developers can explicitly enable them locally for debugging
     *
     * @section usage How to Enable Locally
     *
     * When debugging locally in a Linux environment, set the environment variable to enable them:
     * @code
     * export ENABLE_DYNAMIC_OPERATOR_TEST=1
     * ctest -R DynamicOperator
     * @endcode
     */
    static bool can_run_dynamic_compile_test() {
        const char* enable_dynamic_test = std::getenv("ENABLE_DYNAMIC_OPERATOR_TEST");
        if (enable_dynamic_test && std::string(enable_dynamic_test) == "1") {
            return is_compiler_available();
        }
        return false;
    }

    void SetUp() override {
        System::clear();
    }

    void TearDown() override {
        System::clear();
        // Only clean up temporary files created by the current process to avoid interfering with parallel tests
        cleanup_temp_files();
    }

    void cleanup_temp_files() {
        std::string temp_dir = std::filesystem::temp_directory_path().string();
        std::string pid_prefix = get_pid_prefix();
        try {
            for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
                std::string name = entry.path().filename().string();
                // Only clean up files prefixed with the current process PID
                if (name.find(pid_prefix + "test_op_") == 0 ||
                    name.find(pid_prefix + "test_dynamic_") == 0) {
                    std::error_code ec;
                    std::filesystem::remove(entry.path(), ec);
                    // Ignore removal failures (files may be locked on Windows)
                }
            }
        } catch (const std::exception& e) {
            // Ignore cleanup exceptions
        }
    }
};

// ============ Test cases ============

/**
 * @brief Test a simple SelfAdjointOperator extension
 */
TEST_F(DynamicOperatorTest, SelfAdjointOperatorExtension) {
    // Windows: MSVC and MinGW ABIs are incompatible; skip the test
    if (!can_run_dynamic_compile_test()) {
        GTEST_SKIP() << "Skipped: Dynamic compilation tests disabled by default (set ENABLE_DYNAMIC_OPERATOR_TEST=1 to enable)";
    }

    // Create a simple flip operator
    std::string cpp_code = R"(
#include "basic_components.h"
#include <vector>

using namespace qram_simulator;

class TestFlipOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestFlipOp(size_t r) : reg_id(r) {}
    
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value ^= 1;
        }
    }
};

extern "C" BaseOperator* create_operator(size_t reg_id) {
    return new TestFlipOp(reg_id);
}

extern "C" void destroy_operator(BaseOperator* op) {
    delete op;
}

extern "C" const char* get_operator_name() {
    return "TestFlipOp";
}

extern "C" const char* get_base_class() {
    return "SelfAdjointOperator";
}
)";

    std::string source_path = create_temp_source_file(cpp_code, "test_op_flip.cpp");
    std::string lib_path = compile_to_shared_lib(source_path, "test_op_flip.so");
    
    ASSERT_FALSE(lib_path.empty()) << "Failed to compile dynamic operator";
    ASSERT_TRUE(std::filesystem::exists(lib_path));
    
    // Test dynamic loading
    TestDynamicLoader loader(lib_path);
    EXPECT_TRUE(loader.is_valid());
    
    // Test symbol retrieval
    auto* create_func = reinterpret_cast<BaseOperator* (*)(size_t)>(loader.get_symbol("create_operator"));
    auto* destroy_func = reinterpret_cast<void (*)(BaseOperator*)>(loader.get_symbol("destroy_operator"));
    auto* get_name_func = reinterpret_cast<const char* (*)()>(loader.get_symbol("get_operator_name"));
    
    ASSERT_NE(create_func, nullptr);
    ASSERT_NE(destroy_func, nullptr);
    ASSERT_NE(get_name_func, nullptr);
    
    // Verify the operator name
    EXPECT_STREQ(get_name_func(), "TestFlipOp");
    
    // Create a register and test the operator
    auto q = System::add_register("q", Boolean, 1);
    std::vector<System> state;
    state.emplace_back();  // |0>
    
    // Create an operator instance
    BaseOperator* op = create_func(q);
    ASSERT_NE(op, nullptr);
    
    // Test the operator functionality: flip |0> -> |1>
    (*op)(state);
    EXPECT_EQ(state[0].get(q).value, 1);
    
    // Flip again: |1> -> |0>
    (*op)(state);
    EXPECT_EQ(state[0].get(q).value, 0);
    
    // Test dagger (for a SelfAdjointOperator it should be the same as itself)
    state[0].get(q).value = 1;
    op->dag(state);
    EXPECT_EQ(state[0].get(q).value, 0);
    
    destroy_func(op);
}

/**
 * @brief Test a BaseOperator extension with parameters
 */
TEST_F(DynamicOperatorTest, BaseOperatorWithParams) {
    // Windows: MSVC and MinGW ABIs are incompatible; skip the test
    if (!can_run_dynamic_compile_test()) {
        GTEST_SKIP() << "Skipped: Dynamic compilation tests disabled by default (set ENABLE_DYNAMIC_OPERATOR_TEST=1 to enable)";
    }

    // Create a parameterized phase operator
    std::string cpp_code = R"(
#include "basic_components.h"
#include <vector>
#include <cmath>

using namespace qram_simulator;

class TestPhaseOp : public BaseOperator {
    size_t reg_id;
    double phase;
public:
    TestPhaseOp(size_t r, double p) : reg_id(r), phase(p) {}
    
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            if (s.get(reg_id).value != 0) {
                s.amplitude *= std::exp(std::complex<double>(0, phase));
            }
        }
    }
    
    void dag(std::vector<System>& state) const override {
        for (auto& s : state) {
            if (s.get(reg_id).value != 0) {
                s.amplitude *= std::exp(std::complex<double>(0, -phase));
            }
        }
    }
};

extern "C" BaseOperator* create_operator(size_t reg_id, double phase) {
    return new TestPhaseOp(reg_id, phase);
}

extern "C" void destroy_operator(BaseOperator* op) {
    delete op;
}

extern "C" const char* get_operator_name() {
    return "TestPhaseOp";
}
)";

    std::string source_path = create_temp_source_file(cpp_code, "test_op_phase.cpp");
    std::string lib_path = compile_to_shared_lib(source_path, "test_op_phase.so");
    
    ASSERT_FALSE(lib_path.empty()) << "Failed to compile dynamic operator";
    
    TestDynamicLoader loader(lib_path);
    EXPECT_TRUE(loader.is_valid());
    
    auto* create_func = reinterpret_cast<BaseOperator* (*)(size_t, double)>(loader.get_symbol("create_operator"));
    ASSERT_NE(create_func, nullptr);
    
    auto q = System::add_register("q", Boolean, 1);
    std::vector<System> state;
    state.emplace_back();
    state.emplace_back();
    Init_Unsafe("q", 1)(state);  // |1>
    
    // Create a phase operator (π/2 phase)
    BaseOperator* op = create_func(q, M_PI / 2);
    ASSERT_NE(op, nullptr);
    
    // Apply the operator
    (*op)(state);
    
    // Check the phase: |1> should acquire a phase of i
    EXPECT_NEAR(state[0].amplitude.real(), 0.0, 1e-10);
    EXPECT_NEAR(state[0].amplitude.imag(), 1.0, 1e-10);
}

/**
 * @brief Test compilation error handling
 */
TEST_F(DynamicOperatorTest, CompilationError) {
    // Create code with a syntax error
    std::string bad_cpp_code = R"(
#include "basic_components.h"
using namespace qram_simulator;

class BadOp : public BaseOperator {  // missing semicolon
    void operator()(std::vector<System>& state) const override {
        // Syntax error: undefined variable
        undefined_variable = 42;
    }
};
)";

    std::string source_path = create_temp_source_file(bad_cpp_code, "test_op_bad.cpp");
    std::string lib_path = compile_to_shared_lib(source_path, "test_op_bad.so");
    
    // Compilation should fail and the library file should not exist
    EXPECT_TRUE(lib_path.empty() || !std::filesystem::exists(lib_path));
}

/**
 * @brief Test the cache mechanism
 */
TEST_F(DynamicOperatorTest, CacheMechanism) {
    // Windows: MSVC and MinGW ABIs are incompatible; skip the test
    if (!can_run_dynamic_compile_test()) {
        GTEST_SKIP() << "Skipped: Dynamic compilation tests disabled by default (set ENABLE_DYNAMIC_OPERATOR_TEST=1 to enable)";
    }

    // The same code should produce the same library
    std::string cpp_code = R"(
#include "basic_components.h"
#include <vector>

using namespace qram_simulator;

class TestCacheOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestCacheOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.get(reg_id).value ^= 1;
        }
    }
};

extern "C" BaseOperator* create_operator(size_t reg_id) {
    return new TestCacheOp(reg_id);
}
extern "C" void destroy_operator(BaseOperator* op) { delete op; }
)";

    // Remove stale cache files if any (only clean up this process's .so files)
    std::string temp_dir = std::filesystem::temp_directory_path().string();
    std::string pid_prefix = get_pid_prefix();
    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        std::string name = entry.path().filename().string();
        // Only clean up compiled artifacts (.so/.dll), not source files
        if (name.find(pid_prefix + "test_op_cache") == 0 &&
            (name.find(".so") != std::string::npos || name.find(".dll") != std::string::npos)) {
            std::error_code ec;
            std::filesystem::remove(entry.path(), ec);
        }
    }

    // Create the source files (after the cleanup)
    std::string source_path1 = create_temp_source_file(cpp_code, "test_op_cache1.cpp");
    std::string source_path2 = create_temp_source_file(cpp_code, "test_op_cache2.cpp");
    
    // First compilation
    std::string lib_path1 = compile_to_shared_lib(source_path1, "test_op_cache_a.so");
    ASSERT_FALSE(lib_path1.empty());
    
    // Second compilation of the same code
    std::string lib_path2 = compile_to_shared_lib(source_path2, "test_op_cache_b.so");
    ASSERT_FALSE(lib_path2.empty());
    
    // Both libraries should exist and be loadable
    TestDynamicLoader loader1(lib_path1);
    TestDynamicLoader loader2(lib_path2);
    
    EXPECT_TRUE(loader1.is_valid());
    EXPECT_TRUE(loader2.is_valid());
}

/**
 * @brief Test dagger operation correctness
 */
TEST_F(DynamicOperatorTest, DaggerOperation) {
    // Windows: MSVC and MinGW ABIs are incompatible; skip the test
    if (!can_run_dynamic_compile_test()) {
        GTEST_SKIP() << "Skipped: Dynamic compilation tests disabled by default (set ENABLE_DYNAMIC_OPERATOR_TEST=1 to enable)";
    }

    // SelfAdjointOperator: dagger should equal itself
    std::string self_adjoint_code = R"(
#include "basic_components.h"
#include <vector>

using namespace qram_simulator;

class TestSelfAdjointOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    TestSelfAdjointOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            s.amplitude *= -1.0;  // multiply by -1
        }
    }
};

extern "C" BaseOperator* create_operator(size_t reg_id) {
    return new TestSelfAdjointOp(reg_id);
}
extern "C" void destroy_operator(BaseOperator* op) { delete op; }
)";

    std::string source_path = create_temp_source_file(self_adjoint_code, "test_op_dagger.cpp");
    std::string lib_path = compile_to_shared_lib(source_path, "test_op_dagger.so");
    
    ASSERT_FALSE(lib_path.empty());
    
    TestDynamicLoader loader(lib_path);
    auto* create_func = reinterpret_cast<BaseOperator* (*)(size_t)>(loader.get_symbol("create_operator"));
    ASSERT_NE(create_func, nullptr);
    
    auto q = System::add_register("q", Boolean, 1);
    std::vector<System> state;
    state.emplace_back();
    
    BaseOperator* op = create_func(q);
    ASSERT_NE(op, nullptr);
    
    // Initial amplitude is 1
    EXPECT_NEAR(std::abs(state[0].amplitude - complex_t(1.0, 0)), 0.0, 1e-10);
    
    // Apply the operator: 1 -> -1
    (*op)(state);
    EXPECT_NEAR(std::abs(state[0].amplitude - complex_t(-1.0, 0)), 0.0, 1e-10);
    
    // Apply dagger (for a SelfAdjointOperator it should be the same as itself): -1 -> 1
    op->dag(state);
    EXPECT_NEAR(std::abs(state[0].amplitude - complex_t(1.0, 0)), 0.0, 1e-10);
}

/**
 * @brief Test dynamic library load failure
 */
TEST_F(DynamicOperatorTest, InvalidLibraryLoad) {
    TestDynamicLoader loader("/nonexistent/path/to/library.so");
    EXPECT_FALSE(loader.is_valid());
}

/**
 * @brief Test symbol retrieval from a dynamic library
 */
TEST_F(DynamicOperatorTest, SymbolRetrieval) {
    // Windows: MSVC and MinGW ABIs are incompatible; skip the test
    if (!can_run_dynamic_compile_test()) {
        GTEST_SKIP() << "Skipped: Dynamic compilation tests disabled by default (set ENABLE_DYNAMIC_OPERATOR_TEST=1 to enable)";
    }

    std::string cpp_code = R"(
#include "basic_components.h"
#include <vector>

using namespace qram_simulator;

class SymbolTestOp : public SelfAdjointOperator {
    size_t reg_id;
public:
    SymbolTestOp(size_t r) : reg_id(r) {}
    void operator()(std::vector<System>& state) const override {}
};

extern "C" BaseOperator* create_operator(size_t reg_id) {
    return new SymbolTestOp(reg_id);
}
extern "C" void destroy_operator(BaseOperator* op) { delete op; }
extern "C" const char* get_operator_name() { return "SymbolTestOp"; }
extern "C" const char* get_base_class() { return "SelfAdjointOperator"; }
extern "C" int test_function() { return 42; }
)";

    std::string source_path = create_temp_source_file(cpp_code, "test_op_symbol.cpp");
    std::string lib_path = compile_to_shared_lib(source_path, "test_op_symbol.so");
    
    ASSERT_FALSE(lib_path.empty());
    
    TestDynamicLoader loader(lib_path);
    ASSERT_TRUE(loader.is_valid());
    
    // Test symbols that exist
    EXPECT_NE(loader.get_symbol("create_operator"), nullptr);
    EXPECT_NE(loader.get_symbol("destroy_operator"), nullptr);
    EXPECT_NE(loader.get_symbol("get_operator_name"), nullptr);
    EXPECT_NE(loader.get_symbol("get_base_class"), nullptr);
    EXPECT_NE(loader.get_symbol("test_function"), nullptr);
    
    // Test symbols that do not exist
    EXPECT_EQ(loader.get_symbol("nonexistent_symbol"), nullptr);
    EXPECT_EQ(loader.get_symbol(""), nullptr);
}

// ============ Main function ============

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
