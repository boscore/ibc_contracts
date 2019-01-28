add_contract(ibc.token ibc.token src/ibc.token.cpp)
target_include_directories(ibc.token.wasm
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ibc.chain/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ibc.chain/src)

set_target_properties(ibc.token.wasm
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
