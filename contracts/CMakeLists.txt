cmake_minimum_required( VERSION 3.5 )

project(contracts)

include(${CONTRACT_VERSION_FILE})
message(STATUS "Contract Version: ${CONTRACT_VERSION}")

set(AMAX_WASM_OLD_BEHAVIOR "Off")
find_package(amax.cdt)

set(ICON_BASE_URL "http://127.0.0.1/ricardian_assets/apollo.contracts/icons")

# add_subdirectory(apollo.bill)
 add_subdirectory(apollo.mart)
# add_subdirectory(apollo.sttle)
# add_subdirectory(apollo.token)
add_subdirectory(apollo.vcoin)
