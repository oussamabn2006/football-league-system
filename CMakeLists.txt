
cmake_minimum_required(VERSION 3.20)

project(FootballLeagueSystem
    VERSION 1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(CJSON REQUIRED IMPORTED_TARGET libcjson)

enable_testing()

# Main application
add_executable(football_system
    src/main.c
    src/app.c
    src/cli.c
    src/match_display.c
    src/match_query.c
    src/api_client.c
    src/match_parser.c
    src/match_service.c
    src/standings.c
    src/match_validation.c
    src/match_storage.c
)

target_include_directories(football_system
    PRIVATE include
)

target_link_libraries(football_system
    PRIVATE
    CURL::libcurl
    PkgConfig::CJSON
)

add_executable(test_app
    tests/tests/test_app.c
    src/app.c
    src/cli.c
    src/match_query.c
    src/match_display.c
    src/standings.c
    src/match_storage.c
    src/match_validation.c
)

target_include_directories(test_app PRIVATE include)

add_test(NAME app COMMAND test_app)

# Separate storage test
add_executable(storage_test
    src/storage_test.c
    src/match_storage.c
    src/match_validation.c
)

target_include_directories(storage_test PRIVATE include)

add_executable(test_storage
    tests/tests/test_storage.c
    src/match_storage.c
    src/match_validation.c
)

target_include_directories(test_storage PRIVATE include)

add_test(NAME storage
    COMMAND test_storage ${CMAKE_SOURCE_DIR}/data/matches_epl_2026.csv
)

add_executable(test_validation
    tests/tests/test_validation.c
    src/match_validation.c
)

target_include_directories(test_validation PRIVATE include)

add_test(NAME validation COMMAND test_validation)

add_executable(test_standings
    tests/tests/test_standings.c
    src/standings.c
    src/match_storage.c
    src/match_validation.c
)

target_include_directories(test_standings PRIVATE include)

add_test(NAME standings
    COMMAND test_standings ${CMAKE_SOURCE_DIR}/data/matches_epl_2026.csv
)

# Separate HTTP test
add_executable(api_test
    src/api_test.c
)

target_link_libraries(api_test
    PRIVATE CURL::libcurl
)
add_executable(test_query
    tests/tests/test_query.c
    src/match_query.c
    src/standings.c
)
target_include_directories(test_query PRIVATE include tests)
add_test(NAME query COMMAND test_query)
