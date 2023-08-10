add_contract(amaxnft.mine amaxnft.mine ${CMAKE_CURRENT_SOURCE_DIR}/src/amaxnft.mine.cpp)

target_include_directories(amaxnft.mine
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amaxnft.mine
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( amaxnft.mine PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
