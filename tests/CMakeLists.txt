add_executable(lb_tests
  test_main.cpp
  test_rng.cpp
  test_math.cpp
  test_arena.cpp
  test_fixed_vector.cpp
  test_jobs.cpp
  test_layouts.cpp
  test_cli.cpp
  test_micro.cpp)
target_link_libraries(lb_tests PRIVATE lb_app lb_bench doctest::doctest)  # lb_core etc. come transitively
target_include_directories(lb_tests PRIVATE "${CMAKE_SOURCE_DIR}/src" "${LB_GEN_DIR}")
add_dependencies(lb_tests lb_layouts_gen)
set_target_properties(lb_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
lb_set_warnings(lb_tests)

include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
doctest_discover_tests(lb_tests)

# GPU smoke test: every microbench path on the real device, checksums + R7 copy counts
# verified each iteration, API validation on. Needs a GPU, so it is labelled "gpu".
add_test(NAME microbench_smoke
  COMMAND lightbound --mode=microbench --frames=30 --warmup=5 --micro-payloads=4K,256K
          --validation=on --log=warn --out=${CMAKE_BINARY_DIR}/test_micro_smoke.csv)
set_tests_properties(microbench_smoke PROPERTIES LABELS gpu TIMEOUT 300)
