add_contract(apollo.vcoin apollo.vcoin ${CMAKE_CURRENT_SOURCE_DIR}/src/apollo.vcoin.cpp)

target_include_directories(apollo.vcoin
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(apollo.vcoin
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/apollo.vcoin.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/apollo.vcoin.contracts.md @ONLY )

target_compile_options( apollo.vcoin PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
