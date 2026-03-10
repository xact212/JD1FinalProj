file(GLOB_RECURSE SENSOR_SOURCES
  "core/src/*.c"
  "platform/src/*.c"
)

idf_component_register(
  SRCS ${SENSOR_SOURCES}
  INCLUDE_DIRS
    "core/inc"
    "platform/inc"
  REQUIRES
    driver
    freertos
    log
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
