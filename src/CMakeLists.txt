set(SOURCES
        vips-gmic.cpp
        )

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME}
        PRIVATE
            ${VIPS_CPP_INCLUDE_DIRS}
        )

target_link_libraries(${PROJECT_NAME}
        PRIVATE
            ${VIPS_CPP_LDFLAGS}
            #libgmic
            libgmicstatic
        )

set_target_properties(${PROJECT_NAME}
        PROPERTIES
            PREFIX ""
            SUFFIX ".plg" # libvips plugins needs .plg as suffix
        )

string(REPLACE "." ";" VERSION_LIST ${VIPS_CPP_VERSION})
list(GET VERSION_LIST 0 VIPS_CPP_MAJOR)
list(GET VERSION_LIST 1 VIPS_CPP_MINOR)

install(TARGETS ${PROJECT_NAME}
        RUNTIME
		    DESTINATION ${CMAKE_INSTALL_LIBDIR}/vips-plugins-${VIPS_CPP_MAJOR}.${VIPS_CPP_MINOR}
		    COMPONENT plugin
        LIBRARY
		    DESTINATION ${CMAKE_INSTALL_LIBDIR}/vips-plugins-${VIPS_CPP_MAJOR}.${VIPS_CPP_MINOR}
		    COMPONENT plugin
        )

add_custom_target(plugin-install
        DEPENDS ${PROJECT_NAME}
        COMMAND
            "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT=plugin -P ./cmake_install.cmake
        WORKING_DIRECTORY
            "${CMAKE_BINARY_DIR}"
        )
