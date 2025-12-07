# Pngle component for ESP-IDF - Lightweight PNG decoder
idf_component_register(
    SRCS "pngle.c"
         "miniz.c"
    INCLUDE_DIRS "."
)

