add_contract(ibc.hub ibc.hub src/ibc.hub.cpp)
target_include_directories(ibc.hub.wasm
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ibc.chain/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ibc.token/include )

set_target_properties(ibc.hub.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
