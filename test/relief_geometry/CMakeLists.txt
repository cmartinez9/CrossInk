add_executable(ReliefGeometryTest
  ReliefGeometryTest.cpp
)

target_include_directories(ReliefGeometryTest PRIVATE
  ${REPO_ROOT}/src/components/themes/relief
)

target_link_libraries(ReliefGeometryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReliefGeometryTest)
