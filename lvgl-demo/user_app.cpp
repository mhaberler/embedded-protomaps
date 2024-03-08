#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
extern "C" {

void lv_demo_vector_graphic(void);
} /* extern "C" */



void user_app(void) {

    if (lvgl_port_lock()) {
        lv_demo_vector_graphic();
        lvgl_port_unlock();
    }
    //*/

    // Or you can create your own lvgl app here
    /*
    if (lvgl_port_lock()) {
        static lv_obj_t* label;

        static lv_obj_t* btn1 = lv_btn_create(lv_scr_act());
        lv_obj_align(btn1, LV_ALIGN_CENTER, 0, 0);

        label = lv_label_create(btn1);
        lv_label_set_text(label, "Button");
        lv_obj_center(label);

        lvgl_port_unlock();
    }
    //*/
}
