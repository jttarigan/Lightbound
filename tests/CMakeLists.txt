add_executable(lb_tests
  test_main.cpp
  test_rng.cpp
  test_math.cpp
  test_arena.cpp
  test_fixed_vector.cpp
  test_jobs.cpp
  test_layouts.cpp
  test_cli.cpp
  test_micro.cpp
  test_procgen.cpp)
target_link_libraries(lb_tests PRIVATE lb_app lb_bench lb_procgen doctest::doctest)  # lb_core etc. come transitively
target_include_directories(lb_tests PRIVATE "${CMAKE_SOURCE_DIR}/src" "${LB_GEN_DIR}")
target_compile_definitions(lb_tests PRIVATE LB_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
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

# Hot-GPU cells (DECISIONS #32): idle and hot cells of two paths with the background load on
# the second queue, validation on. On a device without a second queue the hot cells are skipped
# with a warning (the run still has to verify and exit 0).
add_test(NAME microbench_smoke_hot
  COMMAND lightbound --mode=microbench --frames=30 --warmup=5 --micro-paths=empty,s2_direct
          --micro-payloads=4K --micro-gpuload=both --validation=on --log=warn
          --out=${CMAKE_BINARY_DIR}/test_micro_smoke_hot.csv)
set_tests_properties(microbench_smoke_hot PROPERTIES LABELS gpu TIMEOUT 300)
