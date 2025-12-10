cmake_minimum_required(VERSION 3.13)
project(PCO_LAB06 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core QUIET)

if (Qt6_FOUND)
    set(QT_LIBS Qt6::Core)
else()
    find_package(Qt5 COMPONENTS Core REQUIRED)
    set(QT_LIBS Qt5::Core)
endif()

find_library(GTEST_LIB gtest REQUIRED)
find_library(PCOSYNCHRO_LIB pcosynchro REQUIRED)

set(SOURCES
    test/main.cpp
)

set(HEADERS
    src/abstractmatrixmultiplier.h
    src/matrix.h
    src/simplematrixmultiplier.h
    src/threadedmatrixmultiplier.h
    test/multipliertester.h
    test/multiplierthreadedtester.h
)

include_directories(src test)

add_executable(pco_matrices
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(pco_matrices
    ${QT_LIBS}
    ${GTEST_LIB}
    ${PCOSYNCHRO_LIB}
    pthread
)


