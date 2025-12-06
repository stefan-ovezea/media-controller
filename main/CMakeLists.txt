idf_component_register(SRCS "main.c"
                            "display/display_driver.c"
                            "display/lvgl_setup.c"
                            "ui/ui_manager.c"
                            "ui/ui_hello.c"
                    INCLUDE_DIRS "." "display" "ui")
