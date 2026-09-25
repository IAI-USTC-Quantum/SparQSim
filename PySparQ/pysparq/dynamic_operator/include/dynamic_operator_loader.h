/**
 * @file dynamic_operator_loader.h
 * @brief C++ dynamic operator loader
 * @details Loads compiled dynamic libraries (.so/.dll) at runtime and retrieves operator factory functions
 */

#pragma once

#include <string>
#include <functional>
#include <memory>

namespace pysparq {

/**
 * @brief Dynamic library loader
 * @details Wraps platform-specific dynamic library loading, supporting Linux (dlopen), Windows (LoadLibrary), and macOS
 */
class DynamicOperatorLoader {
public:
    /**
     * @brief Constructor, loads the specified dynamic library
     * @param lib_path Dynamic library file path (.so/.dll/.dylib)
     */
    explicit DynamicOperatorLoader(const std::string& lib_path);

    /**
     * @brief Destructor, automatically unloads the dynamic library
     */
    ~DynamicOperatorLoader();

    // Copying is forbidden (dynamic library handles are not copyable)
    DynamicOperatorLoader(const DynamicOperatorLoader&) = delete;
    DynamicOperatorLoader& operator=(const DynamicOperatorLoader&) = delete;

    // Moving is allowed
    DynamicOperatorLoader(DynamicOperatorLoader&& other) noexcept;
    DynamicOperatorLoader& operator=(DynamicOperatorLoader&& other) noexcept;

    /**
     * @brief Retrieves a symbol (factory function) from the dynamic library
     * @param name Symbol name (e.g. "create_operator")
     * @return Pointer to the symbol, or nullptr on failure
     */
    void* get_symbol(const std::string& name);

    /**
     * @brief Checks whether the dynamic library was loaded successfully
     * @return true if loading succeeded, false if it failed
     */
    bool is_valid() const;

    /**
     * @brief Retrieves the most recent error message
     * @return Error description string, or an empty string when there is no error
     */
    std::string get_error() const;

    /**
     * @brief Retrieves the dynamic library path
     * @return Library file path
     */
    const std::string& get_lib_path() const;

private:
    void* handle_;              ///< Dynamic library handle (platform-specific)
    std::string lib_path_;      ///< Library file path
    std::string error_msg_;     ///< Error message

    /**
     * @brief Clears the current error message
     */
    void clear_error();

    /**
     * @brief Sets the error message (obtained from the system)
     */
    void set_error_from_system();

    /**
     * @brief Closes the dynamic library (internal implementation)
     */
    void close_library();
};

/**
 * @brief Factory function type alias
 * @details Signature of a factory function that creates BaseOperator-derived objects
 */
using CreateOperatorFunc = void* (*)();

/**
 * @brief Typed symbol retrieval helper
 * @tparam FuncType Function pointer type
 * @param loader Dynamic library loader
 * @param name Symbol name
 * @return Typed function pointer, or nullptr on failure
 */
template<typename FuncType>
FuncType get_typed_symbol(DynamicOperatorLoader& loader, const std::string& name) {
    void* symbol = loader.get_symbol(name);
    if (symbol == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<FuncType>(symbol);
}

} // namespace pysparq
