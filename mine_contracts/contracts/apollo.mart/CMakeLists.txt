add_contract(apollo.mart apollo.mart ${CMAKE_CURRENT_SOURCE_DIR}/src/apollo.mart.cpp)

target_include_directories(apollo.mart
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../apollo.token/include
)

set_target_properties(apollo.mart
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/apollo.mart.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/apollo.mart.contracts.md @ONLY )

target_compile_options( apollo.mart PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
