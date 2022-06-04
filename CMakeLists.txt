project (libsig)
cmake_minimum_required (VERSION 3.2)

add_library (sig INTERFACE)
target_include_directories (sig INTERFACE "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features (sig INTERFACE cxx_std_11)

if (BUILD_TESTING)
	enable_testing ()
	option (BENCHMARK_ENABLE_GTEST_TESTS "" OFF)
	option (BENCHMARK_ENABLE_TESTING "" OFF)
	add_subdirectory (ext/benchmark)
	add_executable (libsig-test test.cc)
	target_link_libraries (libsig-test PRIVATE sig)
	add_test (NAME test-libsig COMMAND $<TARGET_FILE:libsig-test>)
	add_executable (libsig-bench bench.cc)
	target_link_libraries (libsig-bench PRIVATE benchmark sig)
	target_include_directories (libsig-bench PRIVATE ext/benchmark/include)

	target_compile_options (libsig-test PRIVATE
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
	)
	target_compile_options (libsig-bench PRIVATE
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
	)
endif ()
