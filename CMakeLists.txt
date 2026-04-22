# filepath: gorshok_distortion\CMakeLists.txt
# =============================================================================
# Gorshok Distortion - CMake Build Configuration
# Cross-platform build system for VST3-style audio plugin
# =============================================================================

cmake_minimum_required(VERSION 3.15)

# Enable ASM_NASM language for proper NASM support
enable_language(ASM_NASM)

project(GorshokDistortion VERSION 1.0.0 LANGUAGES C)

# =============================================================================
# Build Options
# =============================================================================

option(BUILD_SHARED_LIBS "Build as shared library (DLL/VST3)" ON)
option(ENABLE_AVX "Enable AVX optimizations (faster on modern CPUs)" ON)
option(ENABLE_SSE "Enable SSE optimizations (compatible)" ON)
option(BUILD_TEST "Build test executable" ON)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =============================================================================
# Find NASM (Assembly Compiler)
# =============================================================================

find_program(NASM_EXECUTABLE nasm)
if(NOT NASM_EXECUTABLE)
    message(WARNING "NASM not found. Assembly files will be skipped.")
endif()

# =============================================================================
# Compiler Flags
# =============================================================================

# MSVC-specific flags
if(MSVC)
    # Enable SSE/AVX
    add_compile_options(/arch:SSE2)
    if(ENABLE_AVX)
        add_compile_options(/arch:AVX)
    endif()
    
    # Disable warnings for external code
    add_compile_options(/W3)
    
    # Release optimizations
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2 /Oi /Ot")
    
    # Export symbols for DLL
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /DLGORSHOK_EXPORTS")
endif()

# GCC/Clang-specific flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Enable SSE/AVX
    if(ENABLE_SSE)
        add_compile_options(-msse -msse2)
    endif()
    if(ENABLE_AVX)
        add_compile_options(-mavx -mavx2)
    endif()
    
    # Optimization flags
    add_compile_options(-O3 -ffast-math -funroll-loops)
    
    # Position independent code for shared library
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# =============================================================================
# Source Files
# =============================================================================

# C source files - use absolute path from source directory
set(C_SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.c
)

# Assembly source files - use absolute path from source directory
set(ASM_SOURCES
    ${CMAKE_SOURCE_DIR}/src/dsp_core.asm
)

# =============================================================================
# Process Assembly Files
# =============================================================================

if(NASM_EXECUTABLE)
    # Determine object file format based on platform
    if(WIN32)
        set(ASM_FORMAT win64)
        set(ASM_FLAGS -f win64)
    elseif(UNIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            set(ASM_FORMAT elf64)
            set(ASM_FLAGS -f elf64)
        endif()
    endif()
    
    # Custom command to assemble .asm files
    foreach(ASM_FILE ${ASM_SOURCES})
        get_filename_component(ASM_NAME ${ASM_FILE} NAME_WE)
        set(OBJ_FILE ${CMAKE_BINARY_DIR}/${ASM_NAME}.obj)
        
        add_custom_command(
            OUTPUT ${OBJ_FILE}
            COMMAND ${NASM_EXECUTABLE} ${ASM_FLAGS} -o ${OBJ_FILE} ${ASM_FILE}
            DEPENDS ${ASM_FILE}
            COMMENT "Assembling ${ASM_FILE}"
            VERBATIM
        )
        
        list(APPEND ASM_OBJECTS ${OBJ_FILE})
    endforeach()
    
    message(STATUS "NASM found: ${NASM_EXECUTABLE}")
    message(STATUS "Assembly format: ${ASM_FORMAT}")
else()
    message(WARNING "NASM not found - building without assembly optimization")
    set(ASM_OBJECTS "")
endif()

# =============================================================================
# Create Shared Library (Plugin)
# =============================================================================

add_library(GorshokDistortion SHARED ${C_SOURCES} ${ASM_OBJECTS})

# Library name
set_target_properties(GorshokDistortion PROPERTIES
    OUTPUT_NAME "GorshokDistortion"
    PREFIX ""
)

# Platform-specific settings
if(WIN32)
    set_target_properties(GorshokDistortion PROPERTIES
        SUFFIX ".dll"
    )
    
    # Export symbols
    target_compile_definitions(GorshokDistortion PRIVATE
        GORSHOK_EXPORTS
    )
    
    # Link libraries (Windows)
    target_link_libraries(GorshokDistortion PRIVATE)
elseif(UNIX)
    set_target_properties(GorshokDistortion PROPERTIES
        SUFFIX ".so"
    )
    
    # Linux-specific linker flags
    set_target_properties(GorshokDistortion PROPERTIES
        LINK_FLAGS "-shared -fPIC"
    )
    
    # Link math library for sin/cos functions
    target_link_libraries(GorshokDistortion PRIVATE m)
endif()

# Include directories
target_include_directories(GorshokDistortion PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# =============================================================================
# Test Executable
# =============================================================================

if(BUILD_TEST)
    add_executable(gorshok_test 
        ${CMAKE_SOURCE_DIR}/src/main.c 
        ${ASM_OBJECTS}
    )
    
    target_compile_definitions(gorshok_test PRIVATE
        GORSHOK_TEST
    )
    
    # Link math library for sin/cos functions
    if(UNIX)
        target_link_libraries(gorshok_test PRIVATE m)
    endif()
    
    # Copy test to output directory
    add_custom_command(TARGET gorshok_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy 
            $<TARGET_FILE:gorshok_test> 
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    )
endif()

# =============================================================================
# Installation
# =============================================================================

install(TARGETS GorshokDistortion
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== Gorshok Distortion Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "NASM: ${NASM_EXECUTABLE}")
message(STATUS "AVX: ${ENABLE_AVX}")
message(STATUS "SSE: ${ENABLE_SSE}")
message(STATUS "Shared Library: ${BUILD_SHARED_LIBS}")
message(STATUS "Test Build: ${BUILD_TEST}")
message(STATUS "===============================================")
message(STATUS "")