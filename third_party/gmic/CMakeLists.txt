FetchContent_Declare(gmic
        GIT_REPOSITORY https://github.com/dtschump/gmic.git
        GIT_SHALLOW TRUE
        GIT_TAG 07d7ff0a3d6f718fe284eee575dfed4f80d66eca
        )

set(BUILD_LIB OFF CACHE BOOL "Disable shared libraries" FORCE)
#set(BUILD_LIB_STATIC OFF CACHE BOOL "Disable static libraries" FORCE)
set(BUILD_CLI OFF CACHE BOOL "Disable CLI" FORCE)
set(BUILD_MAN OFF CACHE BOOL "Disable man pages" FORCE)
set(BUILD_BASH_COMPLETION OFF CACHE BOOL "Disable Bash completion" FORCE)

# Disable all loaders, we use libvips for that
set(ENABLE_CURL OFF CACHE BOOL "Disable curl" FORCE)
set(ENABLE_X OFF CACHE BOOL "Disable X11" FORCE)
set(ENABLE_XSHM OFF CACHE BOOL "Disable X11 Xshm extension" FORCE)
set(ENABLE_FFMPEG OFF CACHE BOOL "Disable FFMpeg" FORCE)
set(ENABLE_FFTW OFF CACHE BOOL "Disable FFTW" FORCE)
set(ENABLE_GRAPHICSMAGICK OFF CACHE BOOL "Disable GraphicsMagick" FORCE)
set(ENABLE_JPEG OFF CACHE BOOL "Disable libjpeg(-turbo)" FORCE)
set(ENABLE_OPENCV OFF CACHE BOOL "Disable OpenCV" FORCE)
set(ENABLE_OPENEXR OFF CACHE BOOL "Disable OpenEXR" FORCE)
set(ENABLE_OPENMP OFF CACHE BOOL "Disable OpenMP" FORCE)
set(ENABLE_PNG OFF CACHE BOOL "Disable libpng" FORCE)
set(ENABLE_TIFF OFF CACHE BOOL "Disable libtiff" FORCE)

# Note: the standard library of gmic needs zlib
set(ENABLE_ZLIB OFF CACHE BOOL "Disable zlib" FORCE)

# -Ofast may cause unexpected results and non-IEEE compliant code
set(CUSTOM_CFLAGS ON CACHE BOOL "Disable -Ofast" FORCE)

# CMake target: libgmic or libgmicstatic
FetchContent_MakeAvailable(gmic)

# Swap the removed -Ofast with -O3
if (CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(libgmicstatic PUBLIC "$<$<CONFIG:RELEASE>:-O3>")
endif()

# Disable VT100 color and display capabilities
# Disable threading support, we rely on libvips' thread pool
# Disable abort support, libvips handles interrupts
target_compile_options(libgmicstatic PUBLIC -Dcimg_display=0 -Ucimg_use_vt100 -Ugmic_is_parallel -Ucimg_use_abort)
