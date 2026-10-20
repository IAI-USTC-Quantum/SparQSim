project(Experiment_CKS)


add_executable(${PROJECT_NAME}_Test HamiltonianSimulationTest.cpp)
target_link_libraries(${PROJECT_NAME}_Test SparQ)


add_executable(${PROJECT_NAME}_Noisy HamiltonianSimulationTest_Noisy.cpp)
target_link_libraries(${PROJECT_NAME}_Noisy SparQ)


add_executable(${PROJECT_NAME}_TridiagonalProbe CKSTridiagonalProbe.cpp)
target_link_libraries(${PROJECT_NAME}_TridiagonalProbe SparQ)


if (CUDA_FOUND)
	add_executable(${PROJECT_NAME}_cutest cks_cutest.cu)
	target_link_libraries(${PROJECT_NAME}_cutest PRIVATE SparQ_cuda)

	add_executable(${PROJECT_NAME}_PoiseuilleBench_cuda CKSPoiseuilleBench.cu)
	target_link_libraries(${PROJECT_NAME}_PoiseuilleBench_cuda PRIVATE SparQ_cuda SparQ)

	# Target configuration is provided uniformly by sparq_setup_cuda_target in the submodule's cmake/CUDA.cmake.
	include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
	sparq_setup_cuda_target(${PROJECT_NAME}_cutest)
	sparq_setup_cuda_target(${PROJECT_NAME}_PoiseuilleBench_cuda)
endif()