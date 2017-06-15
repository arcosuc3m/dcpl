ADD_EXECUTABLE(transform transform.cpp)
TARGET_LINK_LIBRARIES(transform crypto)

ADD_EXECUTABLE(transform_complejo transform_complejo.cpp)
TARGET_LINK_LIBRARIES(transform_complejo crypto)


ADD_EXECUTABLE(reduce reduce.cpp)
TARGET_LINK_LIBRARIES(reduce crypto)

ADD_EXECUTABLE(get get.cpp)
TARGET_LINK_LIBRARIES(get crypto)

ADD_EXECUTABLE(io io.cpp)
TARGET_LINK_LIBRARIES(io crypto)


