// ESP32旋转编码器库 - 增强版，包含U_SET相关功能和步进按钮控制
#ifndef MY_ENCODER_H
#define MY_ENCODER_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

// 取消注释以启用编码器详细调试
// #define DEBUG_ENCODER

class myEncoder {
private:    // 旋转编码器引脚定义
    int pinA; // 编码器A相引脚，通常连接到GPIO
    int pinB; // 编码器B相引脚，通常连接到GPIO
    int confirmPin; // 确认按钮引脚
    int stepSwitchPin; // 步进切换按钮引脚
    
    volatile int16_t encoderCount; // 编码器计数值
    bool lastPinAState; // A相上一次状态
    bool lastPinBState; // B相上一次状态
    
    // 四状态解码相关
    volatile uint8_t encoderState;  // 当前编码器状态
    volatile uint8_t lastEncoderState;  // 上一次编码器状态
    
    // 消抖相关变量
    unsigned long lastDebounceTime; // 上次抖动时间
    unsigned long debounceDelay;    // 消抖延时(毫秒)
    
    // U_SET相关变量
    float u_set; // 电压设置值
    float orig_u_set; // 原始电压设置值，用于在超时时恢复
    float last_u_set;  // 添加实例变量来跟踪上一次的设定值
    float u_set_min; // 最小允许值
    float u_set_max; // 最大允许值
    float u_set_step_fine; // 细调步进值
    float u_set_step_coarse; // 粗调步进值
    bool use_fine_step; // 是否使用细调步进
    bool u_set_confirmed; // 电压设置是否已确认
    unsigned long last_adjustment_time; // 最后一次调整时间
    unsigned long confirm_timeout; // 确认超时时间（毫秒）
      // 步进按钮相关
    static bool stepButtonPressed;
    static bool stepButtonReleaseHandled;
    static unsigned long lastStepDebounceTime;
    static const unsigned long STEP_DEBOUNCE_DELAY = 30;    // 编码器状态追踪
    volatile int8_t lastValidDirection; // 记录上一次有效的旋转方向
    volatile unsigned long lastValidRotationTime; // 记录上一次有效旋转的时间
    volatile uint8_t stepSequence; // 用于追踪一个完整步进的状态序列
    volatile int8_t stepDirection; // 当前步进方向

    // 互斥锁，用于保护中断和读取之间的共享资源
    static portMUX_TYPE mux;
    SemaphoreHandle_t dataMutex; // 用于保护 U_SET 值的访问
    EventGroupHandle_t* systemEventsPtr; // 指向系统事件组的指针
      // 事件定义
    static const uint32_t CONFIRM_EVENT = (1 << 2);
    static const uint32_t STEP_SWITCH_EVENT = (1 << 3);
    static const uint32_t ENCODER_UPDATE_EVENT = (1 << 4);  // 新增：编码器更新事件
    
    // 中断服务程序
    static myEncoder* instance; // 静态实例指针(用于中断回调)
    static void IRAM_ATTR isrA(); // A相中断处理函数
    static void IRAM_ATTR isrB(); // B相中断处理函数
    static void IRAM_ATTR confirmButtonISR(); // 确认按钮中断处理函数
    static void IRAM_ATTR stepSwitchISR(); // 步进切换按钮中断处理函数
    
    // 步进按钮消息队列
    QueueHandle_t stepSwitchQueue;
    static const uint32_t MSG_STEP_BUTTON_PRESSED = 1;
    static const uint32_t MSG_STEP_BUTTON_RELEASED = 2;
    
    void createStepButtonTask(); // 创建步进按钮任务
    
public:
    // 基本构造函数 (保持与原版兼容)
    myEncoder(int pinA, int pinB);
      // 扩展的构造函数，包含全部配置参数
    myEncoder(
        int pinA, 
        int pinB, 
        int confirmPin, 
        int stepSwitchPin,
        float initialUSet = 5.00,
        float minUSet = 1.50,
        float maxUSet = 15.00,
        float fineStep = 0.10,
        float coarseStep = 1.00,
        unsigned long confirmTimeoutMs = 5000
    );
    
    void begin(); // 初始化编码器
    int16_t read(); // 获取并重置编码器计数值
      // U_SET 相关功能
    void setSystemEvents(EventGroupHandle_t* eventGroupHandle); // 设置系统事件组
    EventGroupHandle_t* getSystemEventsPtr(); // 获取系统事件组指针
    float getUSet(); // 获取当前电压设置值
    bool isUSetConfirmed(); // 检查电压设置是否已确认
    void confirmUSet(); // 确认当前电压设置
    void resetUSet(); // 重置电压设置为原始值
    bool toggleStepSize(); // 切换步进大小，返回当前状态 (true=细调, false=粗调)
    float getCurrentStepSize(); // 获取当前步进值
    void checkConfirmTimeout(); // 检查确认超时
    void updateUSetFromEncoder(); // 从编码器读取并更新U_SET值
    void reverseDirection(); // 反转编码器方向
    
    // UI回调函数
    typedef void (*USetDisplayCallback)(float value, bool confirmed, bool isFineStep, void* encoderPtr);
    void setUSetDisplayCallback(USetDisplayCallback callback);
      // 内部使用的中断处理程序
    void handleIsrA(); // 处理A相中断
    void handleIsrB(); // 处理B相中断
    void handleEncoderInterrupt(); // 统一的编码器中断处理函数
    static void stepButtonTask(void* parameter); // 步进按钮任务
    TaskHandle_t stepButtonTaskHandle; // 步进按钮任务句柄
    
private:
    // UI回调
    USetDisplayCallback _uSetDisplayCallback;
    volatile int8_t lastDirection;  // 用于跟踪最后的旋转方向
};

#endif // MY_ENCODER_H
