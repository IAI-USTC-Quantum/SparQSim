project(Experiment_QDA)

add_subdirectory(QDA_Simple)
add_subdirectory(QDA_Poiseuille)
add_subdirectory(QDATest)

if (CUDA_FOUND)
	add_executable(${PROJECT_NAME}_cutest qda_cutest.cu)
	target_link_libraries(${PROJECT_NAME}_cutest PRIVATE SparQ_cuda)

	# Target configuration is provided uniformly by sparq_setup_cuda_target in the submodule's cmake/CUDA.cmake.
	include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
	sparq_setup_cuda_target(${PROJECT_NAME}_cutest)
endif()