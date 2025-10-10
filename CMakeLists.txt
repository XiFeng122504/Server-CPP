cmake_minimum_required(VERSION 3.20)

project(VimUsing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(./include)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(VimUsing ${SRC_FILES})

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/*.cpp)
if (TEST_SOURCES)
    add_executable(protocol_tests ${TEST_SOURCES})
    target_sources(protocol_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/protocol/Message.cpp)
    target_include_directories(protocol_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
    enable_testing()
    add_test(NAME protocol_tests COMMAND protocol_tests)
endif()
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_FILES)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/handlers SRC_HANDLER_FILES)
add_executable(VimUsing ${SRC_FILES} ${SRC_HANDLER_FILES})

enable_testing()

set(TEST_SOURCES
    tests/LoginHandlerTests.cpp
    src/Connection.cpp
    src/Epoll.cpp
    src/handlers/LoginHandler.cpp
    src/handlers/EchoHandler.cpp
    src/handlers/FileUploadHandler.cpp
)

add_executable(SessionTests ${TEST_SOURCES})
target_include_directories(SessionTests PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME SessionTests COMMAND SessionTests)
