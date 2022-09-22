add_contract(amax.save amax.save ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.save.cpp)

target_include_directories(amax.save
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.save
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.save.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.save.contracts.md @ONLY )

target_compile_options( amax.save PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
