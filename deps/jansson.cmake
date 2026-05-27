include(FetchContent)

FetchContent_Declare(jansson
	GIT_REPOSITORY https://github.com/akheron/jansson.git
	GIT_TAG v2.15.0
)

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
	pkg_check_modules(Jansson QUIET jansson>=2.10)
	if (Jansson_FOUND)
		target_link_libraries(${PROJECT_NAME} PRIVATE ${Jansson_LIBRARIES})
		target_include_directories(${PROJECT_NAME} PRIVATE ${Jansson_INCLUDE_DIRS})
		target_compile_options(${PROJECT_NAME} PRIVATE ${Jansson_CFLAGS_OTHER})
	endif ()
endif ()

if (NOT Jansson_FOUND)
	set(JANSSON_EXAMPLES OFF)
	set(JANSSON_BUILD_DOCS OFF)
	set(JANSSON_WITHOUT_TESTS ON)
	set(JANSSON_INSTALL OFF)

	message(STATUS "Downloading jansson")
	FetchContent_MakeAvailable(jansson)

	target_link_libraries(${PROJECT_NAME} PRIVATE jansson)
endif ()
