# Pinned native optimization libraries; never substitutes another office engine.
set(TIGERSHEETS_SOLVER_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/.deps/solvers/prefix" CACHE PATH "Local HiGHS, NLopt and pagmo installation prefix")
list(PREPEND CMAKE_PREFIX_PATH "${TIGERSHEETS_SOLVER_PREFIX}")
set(Boost_NO_BOOST_CMAKE ON)
set(BOOST_LIBRARYDIR "${TIGERSHEETS_SOLVER_PREFIX}/lib")
find_package(highs 1.15.1 EXACT CONFIG REQUIRED)
find_package(NLopt 2.11.0 EXACT CONFIG REQUIRED)
find_package(pagmo 2.20.0 EXACT CONFIG REQUIRED)
add_library(tigersheets_solver_dependencies INTERFACE)
target_link_libraries(tigersheets_solver_dependencies INTERFACE highs::highs NLopt::nlopt Pagmo::pagmo)
# The release package must retain the shared LGPL pagmo runtime and its dependencies.
file(GLOB TIGERSHEETS_SOLVER_RUNTIME_LIBRARIES
 "${TIGERSHEETS_SOLVER_PREFIX}/lib/libpagmo.so*"
 "${TIGERSHEETS_SOLVER_PREFIX}/lib/libboost_serialization.so*"
 "${TIGERSHEETS_SOLVER_PREFIX}/lib/libtbb.so*")
