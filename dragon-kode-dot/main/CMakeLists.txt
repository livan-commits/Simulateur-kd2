idf_component_register(
    SRCS "app_main.c" "dragon_ao.c" "dragon_model.c" "dragon_draw.c" "dragon_render.c"
    INCLUDE_DIRS "."
    REQUIRES kode
    PRIV_REQUIRES kscreen
)
