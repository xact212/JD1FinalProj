idf_component_register(SRCS "JDProj.c"
                    INCLUDE_DIRS "."
                    REQUIRES VL53L0X_API)


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
