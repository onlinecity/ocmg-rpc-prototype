SET(headers
    rpc/rpc.h
)
SET(sources
    rpc/rpc.cc
)

add_library(compiled OBJECT ${sources} ${headers})
#add_executable(ocmg-rpc-test $<TARGET_OBJECTS:compiled> main.cc)
#target_link_libraries(ocmg-rpc-test ${link_libs} )
