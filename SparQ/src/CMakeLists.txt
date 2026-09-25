project(SparQ_Simulator)

file(GLOB_RECURSE src ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(${PROJECT_NAME} STATIC ${src})

if (CUDA_FOUND)
    file(GLOB_RECURSE cuda_src ${CMAKE_CURRENT_SOURCE_DIR}/cuda/*.cu)

    # All sources are recompiled with nvcc (USE_CUDA=1 switches the data-structure layout);
    # Target configuration is provided uniformly by sparq_setup_cuda_target in the submodule's cmake/CUDA.cmake.
    include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
    add_library(${PROJECT_NAME}_cuda STATIC ${cuda_src} ${src})
    sparq_setup_cuda_target(${PROJECT_NAME}_cuda)
endif()

