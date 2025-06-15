/**
 * @file myStateButton.cpp
 * @brief 使用按钮控制程序的ON/OFF状态。OFF状态下ESP32进入低功耗轻睡眠模式。
 * @author watermelon6uice
 * @details 
 * 本库实现了基于物理按钮的状态控制，适用于ESP32平台。按钮用于切换系统的ON/OFF状态：
 * - ON状态下，系统正常运行；
 * - OFF状态下，系统自动进入低功耗的轻睡眠（Light Sleep）模式，并可通过按钮唤醒。
 * 
 * 主要特性与实现细节如下：
 * 1. 支持按钮中断和FreeRTOS任务结合，保证按钮响应的实时性和可靠性；
 * 2. 采用消息队列和任务优先级机制，确保按钮事件处理及时且不会阻塞主循环；
 * 3. 内置防抖逻辑，结合中断和任务内多次状态确认，极大降低误触发概率；
 * 4. 支持通过回调函数通知UI或其他模块状态变化，便于界面或业务逻辑同步；
 * 5. 进入轻睡眠前自动延时，确保UI刷新和系统同步，唤醒后自动恢复运行状态；
 * 6. 支持外部互斥量、事件组等RTOS资源的注入，便于与主系统任务协作；
 * 7. 具备异常状态自检和恢复机制，提升系统健壮性；
 * 8. 代码结构清晰，便于扩展和维护，适合低功耗物联网场景下的状态控制需求。
 * 
 * 使用本库可显著简化ESP32低功耗应用中按钮控制与状态管理的开发流程。
 * 
 * @date 2025-05-15
 */


#include "myStateButton.h"

// 静态成员初始化
QueueHandle_t MyStateButton::_displayQueue = nullptr;
MyStateButton* MyStateButton::_instance = nullptr;

// 基本构造函数
MyStateButton::MyStateButton(uint8_t pin) : 
    _pin(pin), 
    _state(true),  // 默认状态为ON
    _buttonPressed(false),
    _buttonReleaseHandled(true),
    _lastDebounceTime(0),
    _inSleepMode(false),
    _wakeupButtonRelease(false),
    _stateChangeCallback(nullptr),
    _uiUpdateCallback(nullptr),
    _systemEventsPtr(nullptr),
    _dataMutexPtr(nullptr),
    _taskControlMutexPtr(nullptr),
    _dataTaskRunningPtr(nullptr)
{
    _instance = this; // 设置静态实例指针
}

// 增强版构造函数
MyStateButton::MyStateButton(uint8_t pin, EventGroupHandle_t* eventGroupPtr) : 
    _pin(pin), 
    _state(true),  // 默认状态为ON
    _buttonPressed(false),
    _buttonReleaseHandled(true),
    _lastDebounceTime(0),
    _inSleepMode(false),
    _wakeupButtonRelease(false),
    _stateChangeCallback(nullptr),
    _uiUpdateCallback(nullptr),
    _systemEventsPtr(eventGroupPtr),
    _dataMutexPtr(nullptr),
    _taskControlMutexPtr(nullptr),
    _dataTaskRunningPtr(nullptr)
{
    _instance = this; // 设置静态实例指针
}

// 初始化按钮
void MyStateButton::begin() {
    // 创建消息队列
    _displayQueue = xQueueCreate(10, sizeof(uint32_t));
    
    // 设置GPIO引脚
    pinMode(_pin, INPUT_PULLDOWN);
      // 检查重启原因 - 只做记录，不再需要设置重启标志
    esp_reset_reason_t reset_reason = esp_reset_reason();
    Serial.print("系统重启原因: ");
    Serial.println(reset_reason);
    
    // 配置轻睡眠模式，允许SPI外设在睡眠时保持活动
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
      // 创建按钮监控任务，使用更高优先级
    // 注意：在ESP-IDF FreeRTOS中，数字越小优先级越高
    xTaskCreate(
        _buttonTask,         // 任务函数
        "ButtonTask",        // 任务名称
        4096,               // 堆栈大小
        this,               // 任务参数
        1,                  // 高优先级(1是最高优先级)
        &_buttonTaskHandle  // 任务句柄
    );
    
    // 设置按钮中断，监测边沿变化（按下和释放），增强可靠性
    pinMode(_pin, INPUT_PULLDOWN);  // 再次确认引脚模式，防止初始化问题
    attachInterrupt(digitalPinToInterrupt(_pin), _buttonISR, CHANGE);
    
    // 输出按钮设置信息
    Serial.print("按钮引脚已设置为中断模式，引脚号: ");
    Serial.println(_pin);
    Serial.println("按钮状态监控初始化完成，任务优先级已优化");
}

// 更新内部状态
void MyStateButton::update() {
    // 这个函数在主循环中调用，添加按钮状态一致性检查
    static unsigned long lastCheckTime = 0;
    unsigned long currentMillis = millis();
    
    // 每500ms检查一次按钮状态的一致性
    if (currentMillis - lastCheckTime > 500) {
        lastCheckTime = currentMillis;
        
        // 检查按钮物理状态与记录状态是否一致
        bool currentButtonState = digitalRead(_pin);
        
        // 如果按钮处于被按下状态超过1秒，但程序状态未更新，则重置按钮状态
        if (_buttonPressed && currentButtonState == LOW && (currentMillis - _lastDebounceTime > 1000)) {
            // 按钮实际已释放但程序没有检测到，重置状态
            Serial.println("检测到按钮状态不一致，进行重置");
            resetButtonState();
        }
    }
}

// 重置按钮状态
void MyStateButton::resetButtonState() {
    _buttonPressed = false;
    _buttonReleaseHandled = true;
    _wakeupButtonRelease = false;
}

// 获取当前状态
bool MyStateButton::getState() const {
    return _state;
}

// 设置状态
void MyStateButton::setState(bool newState) {
    if (_state != newState) {
        _state = newState;
          // 如果有任务控制互斥量和标志，更新数据任务运行标志
        if (_taskControlMutexPtr && _dataTaskRunningPtr) {
            if (xSemaphoreTake(*_taskControlMutexPtr, pdMS_TO_TICKS(100)) == pdTRUE) {
                *_dataTaskRunningPtr = newState;
                Serial.print("状态切换：更新数据任务运行标志为 ");
                Serial.println(newState ? "true (ON)" : "false (OFF)");
                xSemaphoreGive(*_taskControlMutexPtr);
            } else {
                Serial.println("警告：无法获取互斥量以更新数据任务运行标志");
            }
        } else {
            Serial.println("警告：数据任务控制指针未设置");
        }
        
        // 注意：不能在这里使用taskENTER_CRITICAL，因为回调可能调用FreeRTOS API
        // 使用互斥量或信号量来保护共享资源更合适
        
        // 先调用状态变化回调，让UI有时间更新
        if (_stateChangeCallback) {
            _stateChangeCallback(_state);
        }        // 如果有UI更新回调，调用它
        if (_uiUpdateCallback) {
            _uiUpdateCallback(_state);
        }
        
        // 如果有系统事件组，设置UI更新事件
        if (_systemEventsPtr) {
            xEventGroupSetBits(*_systemEventsPtr, UI_UPDATE_EVENT);
        }
          if (_state) {
            // 确保退出睡眠模式
            if (_inSleepMode) {
                exitLightSleep();
                _inSleepMode = false;
                
                // 等待退出睡眠模式后的系统稳定
                vTaskDelay(100 / portTICK_PERIOD_MS);
                  // 从OFF状态恢复到ON状态时，确保数据任务运行标志设置为ON
                if (_taskControlMutexPtr && _dataTaskRunningPtr) {
                    if (xSemaphoreTake(*_taskControlMutexPtr, pdMS_TO_TICKS(100)) == pdTRUE) {
                        *_dataTaskRunningPtr = true;
                        Serial.println("从睡眠模式恢复：再次确认设置数据任务运行标志为 true (ON)");
                        xSemaphoreGive(*_taskControlMutexPtr);
                    }
                }
                
                // 触发一次数据更新事件
                if (_systemEventsPtr) {
                    Serial.println("从睡眠模式恢复：触发数据更新事件");
                    xEventGroupSetBits(*_systemEventsPtr, DATA_READY_EVENT);
                } else {
                    Serial.println("警告：系统事件组指针未设置，无法触发数据更新事件");
                }
            }
        } else {
            // 进入轻睡眠模式(如果尚未在睡眠模式)
            if (!_inSleepMode) {
                _inSleepMode = true;
                
                // 延迟进入睡眠，让UI有时间更新
                // 使用更长的延时确保UI更新完成
                Serial.println("状态已切换到OFF，100ms后将进入轻睡眠模式...");
                vTaskDelay(100 / portTICK_PERIOD_MS);
                
                // 进入睡眠模式
                _enterLightSleepInternal();
            }
        }
    }
}

// 设置状态变化回调函数
void MyStateButton::setStateChangeCallback(StateChangeCallback callback) {
    _stateChangeCallback = callback;
}

// 检查是否处于睡眠模式
bool MyStateButton::isInSleepMode() const {
    return _inSleepMode;
}

// 进入轻睡眠模式
void MyStateButton::enterLightSleep() {
    if (!_inSleepMode) {
        _inSleepMode = true;
        _enterLightSleepInternal();
    }
}

// 内部进入轻睡眠的实现
void MyStateButton::_enterLightSleepInternal() {
    Serial.println("准备进入轻睡眠模式...");
    
    // 通知所有任务准备进入睡眠模式
    // 这里可以添加与其他任务的同步逻辑
    
    // 配置按钮引脚为RTC IO，用于唤醒
    esp_sleep_enable_ext0_wakeup((gpio_num_t)_pin, 1); // 高电平唤醒
    
    // 进入轻睡眠模式
    Serial.println("进入轻睡眠模式，等待UI完成刷新...");
    
    // 确保有一些延时以便消息能够打印出来，并让UI有足够的时间更新
    // 增加延时确保所有任务都有时间处理完当前操作
    for (int i = 0; i < 3; i++) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        Serial.print(".");
    }
    Serial.println("");
    
    // 开始轻睡眠
    esp_light_sleep_start();
    
    // 下面的代码会在唤醒后执行
    Serial.println("已从轻睡眠模式唤醒");
      if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("由按钮唤醒，切换到ON状态");
        _state = true;
        _inSleepMode = false; // 重置睡眠模式标志
        
        // 立即更新数据任务运行标志为true
        if (_taskControlMutexPtr && _dataTaskRunningPtr) {
            if (xSemaphoreTake(*_taskControlMutexPtr, pdMS_TO_TICKS(100)) == pdTRUE) {
                *_dataTaskRunningPtr = true;
                Serial.println("按钮唤醒时：直接设置数据任务运行标志为 true (ON)");
                xSemaphoreGive(*_taskControlMutexPtr);
                
                // 触发数据更新事件
                if (_systemEventsPtr) {
                    xEventGroupSetBits(*_systemEventsPtr, DATA_READY_EVENT);
                }
            }
        }
        
        // 禁用睡眠唤醒源
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        
        // 重置按钮状态，并标记这次是从睡眠唤醒
        _buttonPressed = true; // 标记为按下，因为此时按钮确实是按下的
        _buttonReleaseHandled = false;
        _wakeupButtonRelease = true; // 标记这是一次从睡眠唤醒的事件
        
        Serial.println("睡眠模式标志已重置，等待按钮释放");
        
        // 调用状态变化回调
        if (_stateChangeCallback) {
            _stateChangeCallback(_state);
        }
    }
}

// 退出轻睡眠模式
void MyStateButton::exitLightSleep() {
    // 禁用所有唤醒源
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    // 通知系统已退出睡眠模式
    Serial.println("正在退出轻睡眠模式...");
    
    // 在这里确保数据任务运行标志被设置为true
    if (_taskControlMutexPtr && _dataTaskRunningPtr) {
        if (xSemaphoreTake(*_taskControlMutexPtr, pdMS_TO_TICKS(100)) == pdTRUE) {
            *_dataTaskRunningPtr = true; // 强制设置为运行状态
            Serial.println("exitLightSleep: 设置数据任务运行标志为 true");
            xSemaphoreGive(*_taskControlMutexPtr);
        } else {
            Serial.println("exitLightSleep: 无法获取互斥量以更新数据任务运行标志");
        }
    } else {
        Serial.println("exitLightSleep: 任务控制指针未设置");
    }
    
    // 触发数据更新事件，确保UI立即刷新
    if (_systemEventsPtr) {
        Serial.println("exitLightSleep: 触发数据更新事件");
        xEventGroupSetBits(*_systemEventsPtr, DATA_READY_EVENT);
    }
    
    // 延时确保系统稳定
    vTaskDelay(50 / portTICK_PERIOD_MS);
    
    // 可以在这里添加唤醒其他任务的代码
    // 例如恢复被暂停的任务等
    
    Serial.println("已完全退出轻睡眠模式，系统恢复正常运行");
}

// 按钮中断服务例程
void IRAM_ATTR MyStateButton::_buttonISR() {
    if (_instance) {
        // 根据按钮状态发送不同消息
        uint32_t msg;
        if (digitalRead(_instance->_pin) == HIGH) {
            // 按钮按下
            msg = MSG_BUTTON_PRESSED;
        } else {
            // 按钮释放
            msg = MSG_BUTTON_RELEASED;
        }
        
        // 添加高优先级任务唤醒标志
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // 发送消息到队列，带有任务唤醒功能
        xQueueSendFromISR(_displayQueue, &msg, &xHigherPriorityTaskWoken);
        
        // 如果有更高优先级的任务被唤醒，请求上下文切换
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

// 按钮监控任务
void MyStateButton::_buttonTask(void* parameter) {
    MyStateButton* button = static_cast<MyStateButton*>(parameter);
    uint32_t msg;
      // 设置任务优先级较高，确保按钮响应及时
    // 注意：在ESP-IDF FreeRTOS中，数字越小优先级越高
    vTaskPrioritySet(NULL, 1);
    
    while (1) {
        // 等待队列消息
        if (xQueueReceive(_displayQueue, &msg, portMAX_DELAY)) {
            // 消息处理
            unsigned long currentMillis = millis();
            
            // 按钮按下消息
            if (msg == MSG_BUTTON_PRESSED && !button->_buttonPressed) {
                if (currentMillis - button->_lastDebounceTime > _debounceDelay) {                    button->_lastDebounceTime = currentMillis;
                    // 确认按钮按下，增强防抖检测
                    if (digitalRead(button->_pin) == HIGH) {
                        // 短暂等待再次检查，增强防抖可靠性
                        ets_delay_us(1000); // 微秒级延迟，不影响任务调度
                        
                        // 再次确认按钮状态
                        if (digitalRead(button->_pin) == HIGH) {
                            button->_buttonPressed = true;
                            button->_buttonReleaseHandled = false;
                            Serial.print("按钮已按下，当前状态：");
                            Serial.print(button->_state ? "ON" : "OFF");
                            Serial.print("，睡眠模式：");
                            Serial.println(button->_inSleepMode ? "是" : "否");
                        }
                    }
                }
            }
            
            // 按钮释放消息
            else if (msg == MSG_BUTTON_RELEASED && button->_buttonPressed && !button->_buttonReleaseHandled) {
                if (currentMillis - button->_lastDebounceTime > _debounceDelay) {                    button->_lastDebounceTime = currentMillis;
                    // 确认按钮释放，增强防抖检测
                    if (digitalRead(button->_pin) == LOW) {
                        // 短暂等待再次检查，增强防抖可靠性
                        ets_delay_us(1000); // 微秒级延迟，不影响任务调度
                        
                        // 再次确认按钮状态  
                        if (digitalRead(button->_pin) == LOW) {
                            button->_buttonPressed = false;
                            button->_buttonReleaseHandled = true;                              if (button->_wakeupButtonRelease) {
                                // 这是从睡眠唤醒后的按钮释放，不切换状态
                                Serial.println("忽略从睡眠唤醒后的按钮释放事件");
                                button->_wakeupButtonRelease = false; // 重置标志
                                Serial.println("已重置唤醒按钮标志");
                                
                                // 确保数据采样任务在唤醒按钮释放后被重新启动
                                if (button->_taskControlMutexPtr && button->_dataTaskRunningPtr) {
                                    if (xSemaphoreTake(*(button->_taskControlMutexPtr), pdMS_TO_TICKS(100)) == pdTRUE) {
                                        *(button->_dataTaskRunningPtr) = true; // 确保设置为运行状态
                                        Serial.println("唤醒按钮释放: 重置后设置数据任务运行标志为 true");
                                        xSemaphoreGive(*(button->_taskControlMutexPtr));
                                        
                                        // 触发数据更新事件
                                        if (button->_systemEventsPtr) {
                                            xEventGroupSetBits(*(button->_systemEventsPtr), DATA_READY_EVENT);
                                        }
                                    }
                                }
                            } else {
                                // 常规按钮释放，执行状态切换
                                Serial.println("按钮已释放，切换状态");

                                // 切换状态                                
                                button->setState(!button->_state);

                                // 状态变化后添加标记而不是直接延时
                                uint32_t stateChangeTime = millis();
                                Serial.print("状态切换时间戳: ");
                                Serial.println(stateChangeTime);
                            }
                        }
                    } else {
                        // 按钮读取状态与中断不一致
                        Serial.println("警告：按钮状态校验失败，跳过处理");
                    }
                }
            }
        }
        
        // 任务延时(减少CPU使用，但保持较快响应)
        vTaskDelay(3 / portTICK_PERIOD_MS);
    }
}

// 设置数据互斥量
void MyStateButton::setDataMutex(SemaphoreHandle_t* dataMutexPtr) {
    _dataMutexPtr = dataMutexPtr;
}

// 设置系统事件组
void MyStateButton::setSystemEvents(EventGroupHandle_t* eventGroupPtr) {
    _systemEventsPtr = eventGroupPtr;
}

// 设置任务控制互斥量和标志
void MyStateButton::setTaskControl(SemaphoreHandle_t* mutexPtr, volatile bool* runningFlagPtr) {
    _taskControlMutexPtr = mutexPtr;
    _dataTaskRunningPtr = runningFlagPtr;
    
    // 如果有数据任务标志，立即更新它以匹配当前按钮状态
    if (_taskControlMutexPtr && _dataTaskRunningPtr) {
        if (xSemaphoreTake(*_taskControlMutexPtr, pdMS_TO_TICKS(100)) == pdTRUE) {
            *_dataTaskRunningPtr = _state;
            xSemaphoreGive(*_taskControlMutexPtr);
        }
    }
}

// 设置UI更新回调函数
void MyStateButton::setUIUpdateCallback(UIUpdateCallback callback) {
    _uiUpdateCallback = callback;
}
