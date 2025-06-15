//导入用户库
#include <Arduino.h>
#include "myTFT.h"
#include "myStateButton.h" // 引入按钮状态管理库
#include "myEncoder.h" // 引入编码器库
#include "myEncoderUI.h" // 引入编码器UI回调函数
#include "myDAC.h"  // 添加DAC库头文件
#include "myADC.h"  // 添加ADC库头文件

// FreeRTOS相关头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

// 添加标准库头文件
#include <stdint.h> 


//函数声明
void updateDisplay();
void updateButtonState(bool is_on); // 按钮状态更新回调
void uiUpdateTask(void* parameter); // 合并的UI更新和LVGL刷新任务
void dataSamplingTask(void* parameter); // 数据采样任务
void initTaskControl(); // 初始化任务控制互斥量函数声明
void encoderTask(void* parameter); // 编码器任务声明
void dacUpdateTask(void* parameter); // DAC更新任务声明

// 定义GPIO引脚
#define BUTTON_STATE_PIN 19  // 按钮状态引脚
#define ENCODER_PIN_A 16  // 编码器A相引脚
#define ENCODER_PIN_B 17  // 编码器B相引脚
#define CONFIRM_BUTTON_PIN 21  // 确认按钮引脚
#define STEP_SWITCH_PIN 45  // 步进切换按钮引脚

// DAC引脚定义
#define DAC_CS_PIN 5    // DAC片选引脚
#define DAC_MOSI_PIN 7  // DAC数据输入引脚（DI）
#define DAC_SCK_PIN 6   // DAC时钟引脚

// DAC全局变量
float g_dacOutputVoltage = 0.0;   // DAC输出电压全局变量，初始为0V
unsigned long lastDacUpdateTime = 0; // 上次DAC更新时间

// ADC全局实例
MyADC* adc = NULL;

// 初始电压和电流值设置
float voltage = 0.0;
float current = 0.0;
unsigned long previousMillis = 0;
const long interval = 1000;

// 任务句柄
TaskHandle_t uiTaskHandle = NULL;    // UI和LVGL任务的句柄
TaskHandle_t dataTaskHandle = NULL;  // 数据采样任务句柄

// 互斥量用于保护数据访问
SemaphoreHandle_t dataMutex;

// 事件组用于任务同步
EventGroupHandle_t systemEvents;
#define UI_UPDATE_EVENT (1 << 0)  // UI更新事件
#define DATA_READY_EVENT (1 << 1)  // 数据就绪事件
#define CONFIRM_EVENT (1 << 2) // 确认事件
#define STEP_SWITCH_EVENT (1 << 3) // 步进切换事件
#define ENCODER_UPDATE_EVENT (1 << 4) // 编码器更新事件

// 任务优先级 (在ESP-IDF FreeRTOS中，数值越小优先级越高)
#define ENCODER_TASK_PRIORITY 1 // 编码器任务优先级
#define UI_TASK_PRIORITY 2      // UI和LVGL刷新任务优先级
#define DATA_TASK_PRIORITY 3    // 数据采样任务优先级


// 数据采样任务控制变量
static SemaphoreHandle_t taskControlMutex = NULL; // 用于保护任务控制变量的互斥量
static volatile bool g_dataTaskRunning = false; // 控制数据采样任务是否执行数据采集

// 创建状态按钮对象 (传入系统事件组)
MyStateButton stateButton(BUTTON_STATE_PIN);

// 创建旋转编码器对象 (包含电压设置和按钮配置)
myEncoder encoder(
    ENCODER_PIN_A,  // A相引脚
    ENCODER_PIN_B,  // B相引脚
    CONFIRM_BUTTON_PIN,  // 确认按钮引脚
    STEP_SWITCH_PIN,     // 步进切换按钮引脚
    5.00,   // 初始电压设置值
    1.50,   // 最小值
    15.00,  // 最大值
    0.10,   // 细调步进值
    1.00,   // 粗调步进值
    5000    // 确认超时时间（5秒）
);

// 创建DAC实例
MyDAC* dac = NULL;

void setup()
{    
    Serial.begin(115200); 
    
    // 创建互斥量和事件组
    dataMutex = xSemaphoreCreateMutex();
    systemEvents = xEventGroupCreate();
    
    // 初始化TFT和LVGL
    tft_init();
    lvgl_setup();    // 初始化ADC并设置高级校正参数
    adc = new MyADC(&guider_ui);
    adc->begin();
    
    // 设置各通道的校正公式
    // 校正公式: 校正值 = 原始值 × 线性系数 + 偏移量 + 二次项系数 × 原始值²
    
    // 输入电压校正 (通道0: U_IN)
    adc->setAdvancedCalibration(0, 15.9034, 0, 0.0);
    
    // 输入电流校正 (通道1: I_IN)  
    adc->setAdvancedCalibration(1, 48.25, 0, 0.0);
    
    // 输出电压校正 (通道2: U_OUT)
    adc->setAdvancedCalibration(2, 5.0744, 0.1966, 0.0);
    
    // 输出电流校正 (通道3: I_OUT)
    adc->setAdvancedCalibration(3, 465, 0, 0.0);

    // 初始化DAC
    dac = new MyDAC(DAC_CS_PIN, DAC_MOSI_PIN, DAC_SCK_PIN);
    dac->begin();
    
    // 创建DAC任务
    createDACTask(dac, 1, 0);  // 优先级1，在核心0上运行
      // 设置初始DAC输出电压（使用DAC库的计算函数）
    float initialSetVoltage = 5.0; // 初始设置电压为5.0V
    g_dacOutputVoltage = MyDAC::calculateDACOutputVoltage(initialSetVoltage); 
    Serial.print("初始设置电压: ");
    Serial.print(initialSetVoltage);
    Serial.print("V, 对应的DAC输出电压: ");
    Serial.print(g_dacOutputVoltage);
    Serial.println("V");
    setDACVoltage(g_dacOutputVoltage);
    
    // 创建DAC更新任务，运行在核心0上
    xTaskCreatePinnedToCore(
        dacUpdateTask,    // 任务函数
        "DAC_Update",     // 任务名称
        2048,             // 堆栈大小
        NULL,             // 任务参数
        1,                // 优先级
        NULL,             // 任务句柄
        0                 // 在核心0上运行
    );

    // 初始化UI
    init_gui(&guider_ui);
    
    // 初始化任务控制互斥量
    initTaskControl();    // 配置编码器和按钮之间的关系
    encoder.setSystemEvents(&systemEvents); // 设置系统事件组
    encoder.setUSetDisplayCallback(updateUSetDisplay); // 设置电压值显示回调函数，使用myEncoderUI.h中定义的函数
    
    // 配置按钮状态和UI回调
    stateButton.setSystemEvents(&systemEvents); // 设置系统事件组
    stateButton.setDataMutex(&dataMutex); // 设置数据互斥量
    stateButton.setTaskControl(&taskControlMutex, &g_dataTaskRunning); // 设置任务控制
    stateButton.setStateChangeCallback(updateButtonState); // 设置状态变化回调函数
    
    // 初始化按钮状态库
    stateButton.begin();
    
    // 初始化编码器（包含步进按钮和确认按钮的设置）
    encoder.begin();
    
    // 如果编码器方向相反，取消下面一行的注释
    encoder.reverseDirection();
    
    // 初始化UI显示状态
    lv_label_set_text(guider_ui.screen_STATE, "ON");
    // 设置ON状态为绿色
    lv_obj_set_style_text_color(guider_ui.screen_STATE, lv_color_hex(0x00ff00), LV_PART_MAIN|LV_STATE_DEFAULT);
    
    // 根据按钮初始状态设置全局任务运行标志（在创建任务前）
    g_dataTaskRunning = stateButton.getState();
    
    // 初始化界面状态（包括待机标签的显示/隐藏）
    updateButtonState(stateButton.getState());
    
    // 初始化U_SET显示
    char u_set_buf[16];
    sprintf(u_set_buf, "%.2f", encoder.getUSet());
    lv_label_set_text(guider_ui.screen_U_SET, u_set_buf);
    
    // 创建FreeRTOS任务
    xTaskCreate(
        uiUpdateTask,        // 任务函数
        "UI_LVGL_Task",      // 任务名称
        4096,               // 堆栈大小
        NULL,               // 任务参数
        UI_TASK_PRIORITY,   // 任务优先级
        &uiTaskHandle       // 任务句柄
    );
      // 创建数据采样任务
    xTaskCreate(
        dataSamplingTask,    // 任务函数
        "Data_Sampling",     // 任务名称
        4096,                // 堆栈大小
        NULL,                // 任务参数
        DATA_TASK_PRIORITY,  // 任务优先级
        &dataTaskHandle      // 任务句柄
    );
      // 创建编码器任务
    xTaskCreate(
        encoderTask,           // 任务函数
        "Encoder_Task",        // 任务名称
        4096,                  // 堆栈大小
        NULL,                  // 任务参数
        ENCODER_TASK_PRIORITY, // 任务优先级
        NULL                   // 任务句柄
    );
}

void loop()
{
    // 主循环只处理按钮状态更新
    stateButton.update();
    vTaskDelay(10 / portTICK_PERIOD_MS);
}



// UI更新和LVGL刷新合并任务 
void uiUpdateTask(void* parameter) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(33); // 每33ms刷新一次(约30fps)
    
    // 初始化最后唤醒时间
    xLastWakeTime = xTaskGetTickCount();
    
    // 设置任务的优先级，确保它能及时响应
    vTaskPrioritySet(NULL, UI_TASK_PRIORITY);
      while (true) {        // 等待各种事件，包括新增的编码器事件
        EventBits_t bits = xEventGroupWaitBits(
            systemEvents,                 // 事件组句柄
            DATA_READY_EVENT | UI_UPDATE_EVENT | CONFIRM_EVENT | STEP_SWITCH_EVENT | ENCODER_UPDATE_EVENT, // 等待的事件位
            pdTRUE,                       // 清除事件位
            pdFALSE,                      // 任一事件均可唤醒
            0                             // 不等待，立即返回
        );
        
        bool needRefresh = false;  // 跟踪是否需要特殊刷新
        
        // 处理数据更新 - 只有在系统ON状态且有新数据时才更新显示
        if (g_dataTaskRunning && (bits & DATA_READY_EVENT)) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                updateDisplay();
                xSemaphoreGive(dataMutex);
                needRefresh = true;
            }
        }
        
        // 处理UI强制更新事件
        if (bits & UI_UPDATE_EVENT) {
            needRefresh = true;
        }
        
        // 处理确认事件
        if (bits & CONFIRM_EVENT) {
            Serial.println("UI任务接收到确认事件");
            encoder.confirmUSet();
            needRefresh = true;
        }
        
        // 处理步进切换事件
        if (bits & STEP_SWITCH_EVENT) {
            Serial.println("UI任务接收到步进切换事件，准备执行切换...");
            encoder.toggleStepSize();
            needRefresh = true;
            Serial.println("步进切换事件处理完成");
        }
        
        // 处理编码器更新事件
        if (bits & ENCODER_UPDATE_EVENT) {
            needRefresh = true;
        }
        // 处理LVGL任务，刷新屏幕
        handle_lvgl_tasks();
        
        // 如果需要额外刷新（如状态切换或数据更新后），再多刷新一次
        if (needRefresh) {
            vTaskDelay(5 / portTICK_PERIOD_MS);  // 短暂延时
            handle_lvgl_tasks();  // 额外的一次刷新
        }
        
        // 确保任务以固定频率运行
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


// 初始化任务控制互斥量 - 简单的辅助函数
void initTaskControl() {
    if (taskControlMutex == NULL) {
        taskControlMutex = xSemaphoreCreateMutex();
    }
}

void dataSamplingTask(void* parameter) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(500); // 每500ms采样一次数据
    
    // 初始化最后唤醒时间
    xLastWakeTime = xTaskGetTickCount();
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
      while (true) {
        // 只有在全局运行标志为true时才进行数据采样
        if (g_dataTaskRunning && adc != NULL) {
            // 更新ADC读数，只进行数据采集，不更新UI
            adc->update(); 
            
            // 通知UI任务进行更新
            xEventGroupSetBits(systemEvents, DATA_READY_EVENT);

            // 调试输出：确认数据采样正在运行
            static unsigned long lastDebugTime = 0;
            unsigned long currentTime = millis();
        } else if (!g_dataTaskRunning) {            // 调试输出：数据采样任务暂停
            static bool pausedMsgPrinted = false;
            if (!pausedMsgPrinted) {
                Serial.println("数据采样任务已暂停 (g_dataTaskRunning = false)");
                pausedMsgPrinted = true;
            }
        } else {
            static bool pausedMsgPrinted = false;
            pausedMsgPrinted = false;
        }
        
        // 确保任务以固定频率运行，使用较长的延时来减少CPU负载
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// DAC更新任务 - 监控全局电压变量并更新DAC输出
void dacUpdateTask(void* parameter) {
    Serial.println("DAC更新任务已启动，监控全局DAC电压变量");
    
    while (1) {
        // 如果系统状态为ON（运行状态），则更新DAC输出电压
        if (g_dataTaskRunning) {
            // 将全局电压变量发送到DAC
            setDACVoltage(g_dacOutputVoltage);
            
            // 可以在调试时打印电压信息
            #ifdef DAC_DEBUG
            Serial.print("\nDAC输出电压: ");
            Serial.print(g_dacOutputVoltage);
            Serial.println("V");
            #endif
        }
        
        // 延时100ms，避免过于频繁的更新
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// 编码器任务函数 - 改为事件驱动
void encoderTask(void* parameter) {
    Serial.println("编码器任务已启动，事件驱动模式");
    
    // 设置较低的超时等待事件，确保定期检查超时
    const TickType_t maxWait = pdMS_TO_TICKS(500); // 最长等待500ms
    
    while (true) {
        // 等待编码器更新事件
        EventBits_t bits = xEventGroupWaitBits(
            systemEvents,         // 事件组句柄
            ENCODER_UPDATE_EVENT, // 等待的事件位
            pdTRUE,               // 清除事件位
            pdFALSE,              // 任一事件均可唤醒
            maxWait               // 最长等待时间
        );
        
        // 处理编码器事件
        if (bits & ENCODER_UPDATE_EVENT) {
            Serial.println("[编码器任务] 收到编码器更新事件");
            encoder.updateUSetFromEncoder();
        }
        
        // 定期检查超时，确保即使没有编码器操作，超时检查也能执行
        encoder.checkConfirmTimeout();
    }
}

// 这些功能现在由myStateButton库处理

// 更新显示函数
void updateDisplay() {
    if (adc == NULL) {
        return;
    }
    
    adc->updateUI();
    
}

// 按钮状态回调函数 - 处理ON/OFF状态切换
void updateButtonState(bool is_on) {
    Serial.print("按钮状态变更回调: 状态设置为 ");
    Serial.println(is_on ? "ON" : "OFF");
    
    // 直接设置g_dataTaskRunning标志，确保与按钮状态同步
    if (taskControlMutex != NULL) {
        if (xSemaphoreTake(taskControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_dataTaskRunning = is_on;
            Serial.print("updateButtonState中更新g_dataTaskRunning为：");
            Serial.println(is_on ? "true" : "false");
            xSemaphoreGive(taskControlMutex);
        }
    }
    
    // 使用互斥量保护数据访问
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        if (is_on) {
            // ON状态 - 绿色
            lv_label_set_text(guider_ui.screen_STATE, "ON");
            lv_obj_set_style_text_color(guider_ui.screen_STATE, lv_color_hex(0x0dff00), LV_PART_MAIN|LV_STATE_DEFAULT);
            
            // 隐藏待机提示标签
            lv_obj_add_flag(guider_ui.screen_standby_label1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(guider_ui.screen_standby_label2, LV_OBJ_FLAG_HIDDEN);
            
            // 提前执行一次LVGL刷新，确保待机标签隐藏变更立即生效
            handle_lvgl_tasks();
            
            // 恢复显示
            updateDisplay();
        } else {
            // OFF状态 - 红色
            lv_label_set_text(guider_ui.screen_STATE, "OFF");
            lv_obj_set_style_text_color(guider_ui.screen_STATE, lv_color_hex(0xff0027), LV_PART_MAIN|LV_STATE_DEFAULT);
            
            // 显示待机提示标签
            lv_obj_clear_flag(guider_ui.screen_standby_label1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(guider_ui.screen_standby_label2, LV_OBJ_FLAG_HIDDEN);
        }
        xSemaphoreGive(dataMutex);
    }        // 检查g_dataTaskRunning的状态
        Serial.print("按钮状态回调函数中，g_dataTaskRunning = ");
        Serial.println(g_dataTaskRunning ? "true" : "false");
        
        // 执行一次强制刷新，确保立即显示更改
        handle_lvgl_tasks();
        
        // 等待适当时间确保状态切换完全生效
        vTaskDelay(is_on ? 80 : 50 / portTICK_PERIOD_MS);
    
    // 再次调用LVGL刷新，确保所有UI变更都已应用
    handle_lvgl_tasks();
}

