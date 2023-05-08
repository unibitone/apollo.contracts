add_contract(apollo.save amax.savetwo ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.savetwo.cpp)

target_include_directories(amax.savetwo
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.savetwo
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( amax.savetwo PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
