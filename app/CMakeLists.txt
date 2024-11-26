set(APP_NAME ${PROJECT_NAME})

add_executable(${APP_NAME} main.cpp)

target_include_directories(${APP_NAME} PUBLIC
    ${CPUID_INCLUDE_DIR}
)
