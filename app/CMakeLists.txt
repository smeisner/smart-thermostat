cmake_minimum_required(VERSION 3.16.0)
set(EXECUTABLE_COMPONENT_NAME src)
set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/src")
if(MATTER_ENABLED)
    list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/matter_dependencies")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(espidf-test)

if(MATTER_ENABLED)
    set(esp_insights_cert "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp_insights/server_certs/https_server.crt")
    if(EXISTS "${esp_insights_cert}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                "-DDATA_FILE=${esp_insights_cert}"
                "-DSOURCE_FILE=${CMAKE_BINARY_DIR}/https_server.crt.S"
                "-DFILE_TYPE=TEXT"
                -P "$ENV{IDF_PATH}/tools/cmake/scripts/data_file_embed_asm.cmake"
            RESULT_VARIABLE embed_cert_result
        )
        if(NOT embed_cert_result EQUAL 0)
            message(FATAL_ERROR "Failed to generate the ESP Insights HTTPS certificate source")
        endif()
    endif()
endif()
