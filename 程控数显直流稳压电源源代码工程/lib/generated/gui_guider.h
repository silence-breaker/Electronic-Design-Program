/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *screen;
	bool screen_del;
	lv_obj_t *screen_img_1;
	lv_obj_t *screen_Uout;
	lv_obj_t *screen_Iout;
	lv_obj_t *screen_Pout;
	lv_obj_t *screen_STATE;
	lv_obj_t *screen_U_SET;
	lv_obj_t *screen_U_SET_label;
	lv_obj_t *screen_Efficiency;
	lv_obj_t *screen_PERCENT_label;
	lv_obj_t *screen_Efficiency_label;
	lv_obj_t *screen_W_label;
	lv_obj_t *screen_A_label;
	lv_obj_t *screen_V_label;
	lv_obj_t *screen_V_label_set;
	lv_obj_t *screen_U_IN;
	lv_obj_t *screen_V_label_IN;
	lv_obj_t *screen_U_IN_label;
	lv_obj_t *screen_I_IN_label;
	lv_obj_t *screen_I_IN;
	lv_obj_t *screen_A_label_IN;
	lv_obj_t *screen_P_IN;
	lv_obj_t *screen_Pin_W_label;
	lv_obj_t *screen_P_IN_label;
	lv_obj_t *screen_standby_label1;
	lv_obj_t *screen_standby_label2;
	lv_obj_t *screen_background;
	lv_obj_t *screen_note_1;
	lv_obj_t *screen_note_2;
	lv_obj_t *screen_note_3;
	lv_obj_t *screen_note_4;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_scr_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                       uint16_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                       lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_ui(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_screen(lv_ui *ui);
LV_IMG_DECLARE(_UI_frame_alpha_260x227);
LV_IMG_DECLARE(_BG_1_alpha_320x24);

LV_FONT_DECLARE(lv_font_Alatsi_Regular_55)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_18)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_12)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_10)
LV_FONT_DECLARE(lv_font_Alatsi_Regular_9)
LV_FONT_DECLARE(lv_font_montserratMedium_9)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_10)


#ifdef __cplusplus
}
#endif
#endif
