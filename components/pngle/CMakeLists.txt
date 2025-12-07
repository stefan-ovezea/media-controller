# Pngle component for ESP-IDF - Lightweight PNG decoder
idf_component_register(
    SRCS "pngle.c"
         "miniz.c"
    INCLUDE_DIRS "."
)

# Optimize pngle for low memory usage on ESP32-C6
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    PNGLE_NO_GAMMA_CORRECTION=1    # Save ~256 bytes gamma table
)

