set(screen_capture_lite_static_FOUND 1)

set(screen_capture_lite_static_VERSION_COUNT 2)
set(screen_capture_lite_static_VERSION_MAJOR "1")
set(screen_capture_lite_static_VERSION_MINOR "0")

set(screen_capture_lite_static_INCLUDE_DIR "C:/Program Files (x86)/SmartScreenCapture/include")
set(screen_capture_lite_static_INCLUDE_DIRS ${screen_capture_lite_static_INCLUDE_DIR})

set(screen_capture_lite_static_LIBRARY_DIR "C:/Program Files (x86)/SmartScreenCapture/lib")

find_library(screen_capture_lite_static_LIBRARY screen_capture_lite_static HINTS ${screen_capture_lite_static_LIBRARY_DIR})

set(screen_capture_lite_static_LIBS ${screen_capture_lite_static_LIBRARY})
set(screen_capture_lite_static_LIBRARIES ${screen_capture_lite_static_LIBS})
