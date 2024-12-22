set(TARGET_NAME ${PROJECT_NAME})

add_executable(${TARGET_NAME} main.cpp)

target_include_directories(${TARGET_NAME} PUBLIC
    ${CPUID_INCLUDE_DIR}
)

target_link_libraries(${TARGET_NAME}
    PUBLIC ftxui::screen
    PUBLIC ftxui::dom
    PUBLIC ftxui::component
)
