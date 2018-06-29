include_directories(Include ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})

set(EXECUTABLE_NAME wowreeb)
set(SOURCE_FILES Config.cpp InputWindow.cpp Injector.cpp main.cpp NotifyIcon.cpp NotifyIconMgr.cpp wowreeb.rc ${CMAKE_SOURCE_DIR}/tiny-AES-c/aes.c)

add_definitions(-DAES256)

set_source_files_properties(WoW.ico wowreeb.rc PROPERTIES LANGUAGE RC)

add_executable(${EXECUTABLE_NAME} WIN32 ${SOURCE_FILES})

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(wowreeb PROPERTIES OUTPUT_NAME "wowreeb64")
else()
    set_target_properties(wowreeb PROPERTIES OUTPUT_NAME "wowreeb32")
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${CMAKE_INSTALL_PREFIX}")