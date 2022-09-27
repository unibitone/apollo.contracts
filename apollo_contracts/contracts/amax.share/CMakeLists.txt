add_contract(amax.share amax.share ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.share.cpp)

target_include_directories(amax.share
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.share
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.share.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.share.contracts.md @ONLY )

target_compile_options( amax.share PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
