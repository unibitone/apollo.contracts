add_contract(nftonesave12 nftone.save ${CMAKE_CURRENT_SOURCE_DIR}/src/nftone.save.cpp)

target_include_directories(nftone.save
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(nftone.save
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( nftone.save PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
