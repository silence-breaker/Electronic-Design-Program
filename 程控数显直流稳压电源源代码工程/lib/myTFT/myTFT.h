/**
 * @file myTFT.h
 * @brief 在TFT屏幕上调用UI界面
 * @author watermelon6uice
 * @details 基于LVGL和TFT_eSPI库，调用Gui-Guider生成的UI界面并显示。代码参考自:https://www.cnblogs.com/kyo413/p/16609733.html。触摸参数在85行。
 * @date 2025-05-10
 */
#ifndef MY_TFT_H
#define MY_TFT_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "gui_guider.h"
#include "events_init.h"
#include "custom.h"

// 初始化TFT显示屏
void tft_init();

// 设置LVGL
void lvgl_setup();

// 初始化GUI界面
void init_gui(lv_ui *ui);

// 处理LVGL任务
void handle_lvgl_tasks();

extern TFT_eSPI tft;
extern lv_ui guider_ui;

#endif // MY_TFT_H