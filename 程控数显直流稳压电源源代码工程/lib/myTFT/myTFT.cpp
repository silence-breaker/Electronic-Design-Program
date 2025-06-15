/**
 * @file myTFT.cpp
 * @brief 在TFT屏幕上调用UI界面
 * @author watermelon6uice
 * @details 基于LVGL和TFT_eSPI库，调用Gui-Guider生成的UI界面并显示。代码参考自:https://www.cnblogs.com/kyo413/p/16609733.html。触摸参数在85行。
 * @date 2025-05-10
 */

#include "myTFT.h"

// 定义分辨率
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
// 定义缓冲
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

// 全局变量
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */
lv_ui guider_ui; // 结构体包含所有屏幕与部件

#if LV_USE_LOG != 0
/* Serial debugging 串口调试用*/
void my_print(const char *buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* Display flushing 显示填充 与LCD驱动关联*/
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
/*输入设备，读取触摸板*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;

    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX + 25;
        data->point.y = touchY - 75;

        Serial.print("Data x ");
        Serial.println(touchX);

        Serial.print("Data y ");
        Serial.println(touchY);
    }
}

void tft_init()
{
    tft.begin();          /* TFT init TFT初始化*/
    tft.setRotation(3); /* Landscape orientation, flipped 设置方向*/

    /*Set the touchscreen calibration data,
     the actual data for your display can be acquired using
     the Generic -> Touch_calibrate example from the TFT_eSPI library*/

    /*设置触摸屏校准数据，
      可以使用获取显示的实际数据
      TFT_eSPI 库中的 Generic -> Touch_calibrate 示例*/
    uint16_t calData[5] = {353, 3534, 244, 3575, 7};
    tft.setTouch(calData);
}

void lvgl_setup()
{
    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging 注册打印功能以进行调试*/
#endif

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

    /*Initialize the display*/
    /*初始化显示*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    /*将以下行更改为您的显示分辨率*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    /*初始化（虚拟）输入设备驱动程序*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

void init_gui(lv_ui *ui)
{
    setup_ui(ui);      // 设置UI界面
    events_init(ui);   // 事件初始化
    custom_init(ui);   // 运行自定义代码，例如将实际输出跟gui显示的数值关联起来
}

void handle_lvgl_tasks()
{
    lv_timer_handler(); /* let the GUI do its work 让GUI完成它的工作 */
    delay(5);
}