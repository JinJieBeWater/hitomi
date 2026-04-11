file(GLOB_RECURSE hitomi_runtime_sources
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

idf_component_register(
    SRCS ${hitomi_runtime_sources}
    INCLUDE_DIRS "../include"
    REQUIRES
        framework-arduinoespressif32
        human_face_detect
        human_face_recognition
)
