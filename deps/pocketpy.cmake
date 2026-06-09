include(FetchContent)

FetchContent_Declare(pocketpy
	GIT_REPOSITORY https://github.com/pocketpy/pocketpy.git
	GIT_TAG v2.1.8
)

message(STATUS "Downloading pocketpy")

set(PK_ENABLE_OS OFF)
set(PK_ENABLE_THREADS OFF)
set(PK_ENABLE_DLL OFF)
set(PK_ENABLE_DETERMINISM OFF)
set(PK_ENABLE_WATCHDOG OFF)
set(PK_ENABLE_CUSTOM_SNAME OFF)
set(PK_ENABLE_MIMALLOC OFF)

set(PK_BUILD_MODULE_LZ4 OFF)
set(PK_BUILD_MODULE_CUTE_PNG OFF)
set(PK_BUILD_MODULE_MSGPACK OFF)
set(PK_BUILD_MODULE_PERIPHERY OFF)

FetchContent_MakeAvailable(pocketpy)

target_link_libraries(${PROJECT_NAME} PRIVATE pocketpy)
