/**
 * @file myStateButton.cpp
 * @brief 使用按钮控制程序的ON/OFF状态。OFF状态下ESP32进入低功耗轻睡眠模式。
 * @author watermelon6uice
 * @date 2025-05-15
 */

#ifndef MY_STATE_BUTTON_H
#define MY_STATE_BUTTON_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

class MyStateButton {
public:
    // 构造函数
    MyStateButton(uint8_t pin);
    
    // 增强版构造函数（允许设置系统事件组）
    MyStateButton(uint8_t pin, EventGroupHandle_t* eventGroupPtr);
    
    // 初始化按钮
    void begin();
    
    // 更新内部状态（在loop中调用）
    void update();
    
    // 获取当前状态
    bool getState() const;
    
    // 设置状态
    void setState(bool newState);
    
    // 注册状态变化回调函数
    typedef void (*StateChangeCallback)(bool newState);
    void setStateChangeCallback(StateChangeCallback callback);
    
    // 设置数据互斥量（可选，用于数据保护）
    void setDataMutex(SemaphoreHandle_t* dataMutexPtr);
    
    // 设置系统事件组（可选，用于事件通知）
    void setSystemEvents(EventGroupHandle_t* eventGroupPtr);
    
    // 设置任务控制互斥量和标志（用于控制数据采样任务）
    void setTaskControl(SemaphoreHandle_t* mutexPtr, volatile bool* runningFlagPtr);
    
    // 进入轻睡眠模式
    void enterLightSleep();
    
    // 退出轻睡眠模式
    void exitLightSleep();
    
    // 检查是否处于睡眠模式
    bool isInSleepMode() const;
    
    // UI回调函数
    typedef void (*UIUpdateCallback)(bool isOn);
    void setUIUpdateCallback(UIUpdateCallback callback);

private:
    // GPIO引脚编号
    uint8_t _pin;
    
    // 状态跟踪
    bool _state;
    
    // 进入轻睡眠的步骤
    void _enterLightSleepInternal();
      // 按钮任务的静态方法
    static void _buttonTask(void* parameter);
    
    // 按钮中断服务例程的静态方法
    static void IRAM_ATTR _buttonISR();
    
    // 重置按钮状态（在状态混乱时使用）
    void resetButtonState();
    
    // FreeRTOS任务句柄
    TaskHandle_t _buttonTaskHandle;
    
    // FreeRTOS队列
    static QueueHandle_t _displayQueue;
    
    // 静态实例指针，用于ISR回调
    static MyStateButton* _instance;
    
    // 按钮状态跟踪变量
    bool _buttonPressed;
    bool _buttonReleaseHandled;
      // 按钮防抖动变量
    unsigned long _lastDebounceTime;
    static const unsigned long _debounceDelay = 30; // 减少防抖时间提高响应性
    
    // 睡眠模式标志
    bool _inSleepMode;
    
    // 用于跟踪从睡眠唤醒后的按钮释放动作
    bool _wakeupButtonRelease;
    
    // 状态变化回调
    StateChangeCallback _stateChangeCallback;
    
    // UI更新回调
    UIUpdateCallback _uiUpdateCallback;
    
    // 系统事件组指针
    EventGroupHandle_t* _systemEventsPtr;
    
    // 数据互斥量指针
    SemaphoreHandle_t* _dataMutexPtr;
    
    // 任务控制互斥量和标志指针
    SemaphoreHandle_t* _taskControlMutexPtr;
    volatile bool* _dataTaskRunningPtr;
      // 事件定义
    static const uint32_t UI_UPDATE_EVENT = (1 << 0);
    static const uint32_t DATA_READY_EVENT = (1 << 1);
    
    // 消息类型定义
    static const int MSG_BUTTON_PRESSED = 1;
    static const int MSG_BUTTON_RELEASED = 2;
};

#endif // MY_STATE_BUTTON_H
