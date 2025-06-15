/**
 * @file myEncoderUI.cpp
 * @brief 旋转编码器UI界面相关实现
 * @author watermelon6uice
 * @details
 * 本文件实现了旋转编码器与TFT界面联动的相关功能，包括电压设置值的显示、颜色变化提示、DAC输出电压的计算与限制等。
 * @date 2025-05-30
 */


#include "myEncoder.h"
#include "myTFT.h"
#include "../generated/gui_guider.h" // 添加GUI引用，包含guider_ui结构体定义
#include "../generated/events_init.h" // 添加事件初始化引用
#include "../myDAC/myDAC.h" // 添加DAC库引用
extern lv_ui guider_ui; // 引用外部声明的guider_ui变量

// 声明全局DAC输出电压变量（定义在main.cpp中）
extern float g_dacOutputVoltage;

// U_SET显示回调函数 - 处理电压设置值显示、颜色变化和DAC输出设置
void updateUSetDisplay(float value, bool confirmed, bool isFineStep, void* encoderPtr) {
    static bool lastStepMode = true; // 记录上一次的步进模式
    
    // 更新U_SET显示值
    char u_set_buf[16];
    sprintf(u_set_buf, "%.2f", value);
    lv_label_set_text(guider_ui.screen_U_SET, u_set_buf);    // 当电压值被确认时，更新全局DAC输出电压变量
    if (confirmed) {
        // 使用DAC库的计算函数
        g_dacOutputVoltage = MyDAC::calculateDACOutputVoltage(value); 
        
        // 调试输出
        Serial.print("设置电压: ");
        Serial.print(value);
        Serial.print("V, 对应的DAC输出电压: ");
        Serial.print(g_dacOutputVoltage);
        Serial.println("V");
    }
    
    // 根据状态设置颜色
    if (!confirmed) {
        // 未确认状态 - 黄色
        lv_obj_set_style_text_color(guider_ui.screen_U_SET, lv_color_hex(0xffff00), LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(guider_ui.screen_V_label_set, lv_color_hex(0xffff00), LV_PART_MAIN|LV_STATE_DEFAULT);
    } else {
        // 确认状态 - 白色
        lv_obj_set_style_text_color(guider_ui.screen_U_SET, lv_color_hex(0xe0e0e0), LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(guider_ui.screen_V_label_set, lv_color_hex(0xe0e0e0), LV_PART_MAIN|LV_STATE_DEFAULT);
    }
    
    // 只在步进模式发生变化时才更新颜色和创建延时任务
    if (lastStepMode != isFineStep) {
        lastStepMode = isFineStep;
        
        if (isFineStep) {
            // 细调模式 - 蓝色提示
            lv_obj_set_style_text_color(guider_ui.screen_V_label_set, lv_color_hex(0x00ffff), LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            // 粗调模式 - 橙色提示
            lv_obj_set_style_text_color(guider_ui.screen_V_label_set, lv_color_hex(0xff8000), LV_PART_MAIN|LV_STATE_DEFAULT);
        }
        
        // 创建一次性延时任务来恢复颜色（2秒后）
        xTaskCreate(
            [](void* taskParam) {
                // 等待2秒
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                // 恢复标签颜色 - 如果是未确认状态，则保持黄色，否则恢复白色
                bool u_confirmed = ((myEncoder*)taskParam)->isUSetConfirmed();
                
                if (!u_confirmed) {
                    lv_obj_set_style_text_color(guider_ui.screen_V_label_set, lv_color_hex(0xffff00), LV_PART_MAIN|LV_STATE_DEFAULT);
                } else {
                    lv_obj_set_style_text_color(guider_ui.screen_V_label_set, lv_color_hex(0xe0e0e0), LV_PART_MAIN|LV_STATE_DEFAULT);
                }
                
                // 请求UI更新
                EventGroupHandle_t* systemEventsPtr = ((myEncoder*)taskParam)->getSystemEventsPtr();
                if (systemEventsPtr) {
                    xEventGroupSetBits(*systemEventsPtr, (1 << 0)); // UI_UPDATE_EVENT
                }
                
                vTaskDelete(NULL); // 删除任务
            },
            "RestoreColor",   // 任务名称
            2048,             // 堆栈大小
            encoderPtr,       // 任务参数 - 传递encoder对象
            2,                // 任务优先级
            NULL              // 任务句柄
        );
    }
    
    // 设置UI更新事件，请求刷新
    EventGroupHandle_t* systemEventsPtr = ((myEncoder*)encoderPtr)->getSystemEventsPtr();
    if (systemEventsPtr) {
        xEventGroupSetBits(*systemEventsPtr, (1 << 0)); // UI_UPDATE_EVENT
    }
}
