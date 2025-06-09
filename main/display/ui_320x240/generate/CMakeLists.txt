SET(SOURCES screens/ui_ScreenMain.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_1590408947.c
    images/ui_img_image_sleep_png.c
    images/ui_img_2129536719.c
    images/ui_img_363796412.c
    images/ui_img_image_user2_png.c
    images/ui_img_382539590.c
    fonts/ui_font_CN16.c
    fonts/ui_font_IconBat24.c
    fonts/ui_font_IconWiFi24.c)

add_library(ui ${SOURCES})
