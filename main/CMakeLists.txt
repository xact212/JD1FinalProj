idf_component_register(SRCS "JDProj.c"
                    INCLUDE_DIRS "."
                    REQUIRES VL53L0X_API esp32-i2c-lcd1602)


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
