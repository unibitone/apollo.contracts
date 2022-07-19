add_contract(apollo.ev apollo.ev ${CMAKE_CURRENT_SOURCE_DIR}/src/apollo.ev.cpp)

target_include_directories(apollo.ev
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(apollo.ev
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/apollo.ev.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/apollo.ev.contracts.md @ONLY )

target_compile_options( apollo.ev PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
