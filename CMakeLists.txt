cmake_minimum_required(VERSION 4.0)
project(SampleAgent)

set(CMAKE_CXX_STANDARD 20)

add_executable(SampleAgent main.cpp)

include(FetchContent)
FetchContent_Declare(
        agentic_framework
        GIT_REPOSITORY https://github.com/utkarsh-cpu/CPP-Agentic-Framework.git
        GIT_TAG master # or specific commit/tag for stability
)

FetchContent_MakeAvailable(agentic_framework)

find_package(Boost CONFIG COMPONENTS json filesystem REQUIRED)
find_package(OpenSSL REQUIRED)

# Link the library
target_link_libraries(${PROJECT_NAME} PRIVATE agentic_framework)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::json)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::container)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::filesystem)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::system)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Include directories (your current CMake needs this)
target_include_directories(${PROJECT_NAME} PRIVATE
        ${agentic_framework_SOURCE_DIR}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
