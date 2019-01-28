add_contract(ibc.chain ibc.chain src/ibc.chain.cpp)
target_include_directories(ibc.chain.wasm
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(ibc.chain.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
