include(FetchContent)

FetchContent_Declare(jansson
	GIT_REPOSITORY https://github.com/akheron/jansson.git
	GIT_TAG v2.15.0
)

set(JANSSON_EXAMPLES OFF)
set(JANSSON_BUILD_DOCS OFF)
set(JANSSON_WITHOUT_TESTS ON)
set(JANSSON_INSTALL OFF)

message(STATUS "Downloading jansson")
FetchContent_MakeAvailable(jansson)

target_link_libraries(${PROJECT_NAME} PRIVATE jansson)
