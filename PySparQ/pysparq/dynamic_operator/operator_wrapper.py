"""
Dynamic operator wrapper module

Calls C++ operators in dynamic libraries via ctypes and creates Python proxy classes.
"""

import ctypes
import os
import warnings
import weakref
from typing import Any, Callable, List, Tuple, Type

# Import the base class from pysparq (optional, used for type hints)
try:
    from pysparq import SparseState
except ImportError:
    # When pysparq is not built, define a placeholder
    SparseState = None


class DynamicOperatorError(Exception):
    """Dynamic operator error."""
    pass


class DynamicOperatorLoadError(DynamicOperatorError):
    """Dynamic library load error."""
    pass


class DynamicOperatorFactoryError(DynamicOperatorError):
    """Factory function invocation error."""
    pass


# Store active instances for cleanup
_active_instances = {}
_instance_counter = [0]


def _register_instance(instance):
    """Register an instance for tracking."""
    instance_id = _instance_counter[0]
    _instance_counter[0] += 1
    _active_instances[instance_id] = weakref.ref(instance)
    return instance_id


def _unregister_instance(instance_id):
    """Unregister an instance."""
    if instance_id in _active_instances:
        del _active_instances[instance_id]


class CppOperatorWrapper:
    """
    C++ operator wrapper
    
    Loads the dynamic library and invokes the factory functions to create/destroy C++ operator objects
    """
    
    def __init__(self, lib_path: str):
        """
        Initialize the wrapper
        
        Args:
            lib_path: Dynamic library path
        """
        self.lib_path = lib_path
        self._handle = None
        self._create_func = None
        self._destroy_func = None
        self._get_name_func = None
        self._get_base_class_func = None
        self._apply_func = None
        self._apply_dag_func = None
        self._arg_types = []
        
    def load(self, arg_types: List[str] = None):
        """
        Load the dynamic library
        
        Args:
            arg_types: List of constructor argument types
            
        Raises:
            DynamicOperatorLoadError: Load failed
        """
        if not os.path.exists(self.lib_path):
            raise DynamicOperatorLoadError(f"Dynamic library does not exist: {self.lib_path}")
        
        try:
            # Use RTLD_GLOBAL so that symbols can be resolved
            self._handle = ctypes.CDLL(self.lib_path, mode=ctypes.RTLD_GLOBAL)
        except OSError as e:
            raise DynamicOperatorLoadError(f"Failed to load dynamic library: {e}")
        
        # Obtain the factory functions
        try:
            self._create_func = self._handle.create_operator
            self._destroy_func = self._handle.destroy_operator
            self._get_name_func = self._handle.get_operator_name
            self._get_base_class_func = self._handle.get_base_class
            
            # Python-enhanced functions - obtain the C++ SparseState* pointer via state._cpp_ptr()
            try:
                self._apply_func = self._handle.apply_operator
                self._apply_dag_func = self._handle.apply_operator_dag
            except AttributeError:
                pass  # Older template versions lack these functions
                
        except AttributeError as e:
            raise DynamicOperatorLoadError(f"Required factory function not found: {e}")
        
        # Set the argument types
        if arg_types:
            self._arg_types = arg_types
            self._setup_arg_types()
    
    def _setup_arg_types(self):
        """Set up function argument types."""
        if not self._create_func:
            return
            
        # Set argtypes according to the argument types
        type_mapping = {
            'int': ctypes.c_int,
            'size_t': ctypes.c_size_t,
            'unsigned int': ctypes.c_uint,
            'unsigned long': ctypes.c_ulong,
            'unsigned long long': ctypes.c_ulonglong,
            'long': ctypes.c_long,
            'long long': ctypes.c_longlong,
            'float': ctypes.c_float,
            'double': ctypes.c_double,
            'bool': ctypes.c_bool,
            'char': ctypes.c_char,
            'char*': ctypes.c_char_p,
            'const char*': ctypes.c_char_p,
        }
        
        argtypes = []
        for arg_type in self._arg_types:
            ctype = type_mapping.get(arg_type)
            if ctype is None:
                # Default to size_t
                ctype = ctypes.c_size_t
            argtypes.append(ctype)
        
        self._create_func.argtypes = argtypes
        self._create_func.restype = ctypes.c_void_p
        
        self._destroy_func.argtypes = [ctypes.c_void_p]
        self._destroy_func.restype = None
        
        if self._get_name_func:
            self._get_name_func.argtypes = []
            self._get_name_func.restype = ctypes.c_char_p
        
        if self._get_base_class_func:
            self._get_base_class_func.argtypes = []
            self._get_base_class_func.restype = ctypes.c_char_p
            
        if self._apply_func:
            # SparseState* argument: ctypes.c_void_p passes the pointer value
            self._apply_func.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
            self._apply_func.restype = None

        if self._apply_dag_func:
            self._apply_dag_func.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
            self._apply_dag_func.restype = None
    
    def create(self, *args) -> int:
        """
        Create a C++ operator instance
        
        Args:
            *args: Constructor arguments
            
        Returns:
            C++ object address (as a Python int)
        """
        if not self._create_func:
            raise DynamicOperatorFactoryError("Factory function not loaded")
        
        try:
            ptr = self._create_func(*args)
            return ptr
        except Exception as e:
            raise DynamicOperatorFactoryError(f"Failed to create operator: {e}")
    
    def destroy(self, ptr: int):
        """
        Destroy a C++ operator instance
        
        Args:
            ptr: C++ object address
        """
        if self._destroy_func and ptr:
            try:
                self._destroy_func(ptr)
            except Exception as e:
                warnings.warn(f"Failed to destroy C++ object at {ptr}: {e}")
                raise
    
    def close(self):
        """
        Close the dynamic library and release resources
        
        Note: on Windows, all C++ objects must already be destroyed
        before the dynamic library file can be deleted successfully
        """
        # Clear function references to help garbage collection
        self._create_func = None
        self._destroy_func = None
        self._get_name_func = None
        self._get_base_class_func = None
        self._apply_func = None
        self._apply_dag_func = None
        
        # Release the dynamic library handle
        if self._handle is not None:
            # On Windows, a forced garbage collection is needed to ensure the handle is released
            import gc
            gc.collect()
            
            # Drop the handle reference so ctypes releases the library
            handle = self._handle
            self._handle = None
            
            # Windows specific: force-release the library handle
            if os.name == 'nt':
                try:
                    import ctypes
                    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
                    # Get the module handle and free it
                    hmodule = ctypes.c_void_p(handle._handle)
                    if hmodule:
                        kernel32.FreeLibrary(hmodule)
                except Exception as e:
                    warnings.warn(f"Failed to FreeLibrary: {e}")
                    raise
            
            # Delete the handle object
            del handle
            
            # Force garbage collection again
            gc.collect()
    
    def get_name(self) -> str:
        """Get the operator name."""
        if self._get_name_func:
            result = self._get_name_func()
            if result:
                return result.decode('utf-8')
        return ""
    
    def get_base_class(self) -> str:
        """Get the base class name."""
        if self._get_base_class_func:
            result = self._get_base_class_func()
            if result:
                return result.decode('utf-8')
        return "BaseOperator"
    
    def apply(self, ptr: int, state_cpp_ptr: int):
        """
        Apply the operator to a SparseState

        Args:
            ptr: Operator object address
            state_cpp_ptr: C++ SparseState* pointer (obtained via state._cpp_ptr())
        """
        if self._apply_func and ptr and state_cpp_ptr:
            self._apply_func(ptr, state_cpp_ptr)

    def apply_dag(self, ptr: int, state_cpp_ptr: int):
        """
        Apply the dagger to a SparseState

        Args:
            ptr: Operator object address
            state_cpp_ptr: C++ SparseState* pointer (obtained via state._cpp_ptr())
        """
        if self._apply_dag_func and ptr and state_cpp_ptr:
            self._apply_dag_func(ptr, state_cpp_ptr)



def create_operator_class(
    name: str,
    lib_path: str,
    base_class: str = "BaseOperator",
    constructor_args: List[Tuple[str, str]] = None
) -> Type:
    """
    Create a dynamic operator Python class
    
    Args:
        name: Operator class name
        lib_path: Dynamic library path
        base_class: Base class name ("BaseOperator" or "SelfAdjointOperator")
        constructor_args: List of constructor arguments [(type, name), ...]
        
    Returns:
        The dynamically created operator class
    """
    constructor_args = constructor_args or []
    
    # Create the C++ wrapper
    wrapper = CppOperatorWrapper(lib_path)
    arg_types = [arg[0] for arg in constructor_args]
    wrapper.load(arg_types)
    
    # Validate the base class
    detected_base = wrapper.get_base_class()
    if detected_base and detected_base != base_class:
        import warnings
        warnings.warn(f"Detected base class is {detected_base}, but {base_class} was specified")
        base_class = detected_base
    
    def custom_init(self, **kwargs):
        """
        Dynamic operator constructor

        Args:
            **kwargs: Constructor arguments (passed by name)
        """
        # Collect the argument values
        args = []
        for arg_type, arg_name in constructor_args:
            if arg_name not in kwargs:
                raise TypeError(f"Missing required argument: {arg_name}")
            args.append(kwargs[arg_name])

        # Store the arguments for dag
        self._args = tuple(args)
        # Store the wrapper and base_class on the class (not on the instance),
        # so all instances share the same wrapper, avoiding mistakenly closing the dynamic library when __del__ runs.
        self._wrapper = DynamicOpClass._wrapper
        self._base_class = DynamicOpClass._base_class
        self._instance_id = _register_instance(self)

        # Create the C++ operator instance
        self._cpp_ptr = self._wrapper.create(*args)
    
    def call_method(self, state):
        """
        Invoke the operator

        Args:
            state: SparseState object

        Returns:
            Returns the input state (supports chaining)
        """
        if not self._cpp_ptr:
            raise RuntimeError("Operator not initialized or already destroyed")

        # Obtain the C++ SparseState* pointer via state._cpp_ptr() (exposed in pysparq._core.SparseState)
        state_cpp_ptr = state._cpp_ptr()
        self._wrapper.apply(self._cpp_ptr, state_cpp_ptr)

        return state

    def dag_method(self, state):
        """
        Invoke the dagger operation

        Args:
            state: SparseState object

        Returns:
            Returns the input state
        """
        if not self._cpp_ptr:
            raise RuntimeError("Operator not initialized or already destroyed")

        state_cpp_ptr = state._cpp_ptr()

        if base_class == "SelfAdjointOperator":
            # For a self-adjoint operator, dagger equals itself
            self._wrapper.apply(self._cpp_ptr, state_cpp_ptr)
        else:
            # BaseOperator uses the dagger helper function
            self._wrapper.apply_dag(self._cpp_ptr, state_cpp_ptr)

        return state
    
    def repr_method(self) -> str:
        """String representation."""
        arg_str = ", ".join(f"{arg_name}={repr(val)}" for (arg_type, arg_name), val in zip(
            constructor_args, self._args
        ))
        return f"{name}({arg_str})"
    
    def del_method(self):
        """Destructor."""
        if hasattr(self, '_cpp_ptr') and self._cpp_ptr:
            self._wrapper.destroy(self._cpp_ptr)
            self._cpp_ptr = 0
        if hasattr(self, '_instance_id'):
            _unregister_instance(self._instance_id)
        # Note: do not close the wrapper here, because the wrapper is managed at the class level and shared across all instances.
        # Closing the dynamic library is the responsibility of cleanup_all_instances() or an explicit call.
    
    # Create the dynamic class
    DynamicOpClass = type(
        name,
        (object,),
        {
            '__init__': custom_init,
            '__call__': call_method,
            'dag': dag_method,
            '__repr__': repr_method,
            '__del__': del_method,
            '_is_dynamic_operator': True,
            '_base_class': base_class,
            '_lib_path': lib_path,
        }
    )
    
    # Store the wrapper and base_class at class level (shared across instances)
    DynamicOpClass._wrapper = wrapper
    DynamicOpClass._base_class = base_class

    # Add the docstring
    arg_docs = "\n".join(f"        {arg_name} ({arg_type})" for arg_type, arg_name in constructor_args) if constructor_args else "        (none)"
    DynamicOpClass.__doc__ = f"""
Dynamically generated operator class: {name}

Base class: {base_class}

Constructor arguments:
{arg_docs}

Usage example:
    >>> op = {name}({', '.join(f"{arg_name}=..." for _, arg_name in constructor_args) if constructor_args else ''})
    >>> state = op(state)
"""
    
    return DynamicOpClass


def cleanup_all_instances():
    """Clean up all active dynamic operator instances"""
    import gc
    
    # Clean up instances that still exist
    for instance_id, ref in list(_active_instances.items()):
        instance = ref()
        if instance is not None:
            try:
                # Destroy the C++ object first
                if hasattr(instance, '_cpp_ptr') and instance._cpp_ptr:
                    instance._wrapper.destroy(instance._cpp_ptr)
                    instance._cpp_ptr = 0
                # Close the dynamic library handle
                if hasattr(instance, '_wrapper'):
                    instance._wrapper.close()
            except Exception as e:
                warnings.warn(f"Cleanup failed during _cleanup_active_instances: {e}")
                raise
    
    _active_instances.clear()
    
    # Force garbage collection to ensure resources are released
    gc.collect()
