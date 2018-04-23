include_directories(Include ${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCE_FILES
    domainloader.cpp
    InitializeHooks.cpp
    main.cpp
)

add_library(dll SHARED ${SOURCE_FILES})

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(dll PROPERTIES OUTPUT_NAME "wowreeb64")
else()
    set_target_properties(dll PROPERTIES OUTPUT_NAME "wowreeb32")
endif()

install(TARGETS dll RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
