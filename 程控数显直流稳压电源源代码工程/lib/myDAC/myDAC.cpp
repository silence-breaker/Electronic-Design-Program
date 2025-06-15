/**
 * @file myDAC.cpp
 * @brief ESP32驱动TLC5615 DAC的库实现文件
 * @author watermelon6uice
 * @date 2025-05-25
 * @details
 * 本库封装了TLC5615数模转换器（DAC）的驱动，适用于ESP32平台。主要功能包括：
 * - 通过SPI接口控制TLC5615 DAC输出电压
 * - 提供电压与DAC值的转换
 * - 支持FreeRTOS任务与队列机制，实现异步/多任务环境下的DAC电压控制
 * - 提供DAC输出值、电压的获取与设置接口
 * - 支持通过队列异步设置DAC输出电压，适合实时控制场景
 * 
 * 主要类与函数说明：
 * - MyDAC类：负责DAC的初始化、数值/电压设置与获取、SPI通信等
 * - createDACTask/stopDACTask：创建与停止DAC控制任务
 * - setDACVoltage：通过队列异步设置DAC输出电压
 * 
 * 适用芯片：ESP32
 * 适用DAC型号：TLC5615
 * 
 * 使用前请确保已正确连接硬件，并在工程中包含本库的头文件与源文件。
 * @date 2025-05-30
 */


 
#include "myDAC.h"

// FreeRTOS相关变量
TaskHandle_t dacTaskHandle = NULL;
QueueHandle_t dacVoltageQueue = NULL;
static MyDAC* globalDacInstance = NULL;

// 构造函数
MyDAC::MyDAC(uint8_t csPin, uint8_t mosiPin, uint8_t sckPin) : 
    _csPin(csPin),
    _mosiPin(mosiPin),
    _sckPin(sckPin),
    _currentValue(0)
{
    // 创建新的HSPI实例
    _spi = new SPIClass(HSPI);
}

// 初始化DAC
void MyDAC::begin() {
    // 配置引脚
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);  // 默认CS为高电平（未选中）
    
    // 初始化SPI
    _spi->begin(_sckPin, -1, _mosiPin, -1); // MISO pin不需要
    
    // 初始化输出为0V
    setValue(0);
    
    Serial.println("TLC5615 DAC 初始化完成");
}

// 将电压值转换为DAC值
uint16_t MyDAC::voltageToDAC(float voltage) {
    // 限制电压范围在0-4.096V之间
    if (voltage < 0) voltage = 0;
    if (voltage > DAC_MAX_VOLTAGE) voltage = DAC_MAX_VOLTAGE;
    
    // 计算DAC值：voltage / DAC_MAX_VOLTAGE * DAC_MAX_VALUE
    return (uint16_t)((voltage * DAC_MAX_VALUE) / DAC_MAX_VOLTAGE);
}

// 设置DAC输出值（0-1023）
void MyDAC::setValue(uint16_t value) {
    // 限制在10位范围内
    value &= 0x3FF; // 确保值在10位范围内
    
    _currentValue = value;
    
    // TLC5615要求数据左移2位，参考reference.cpp的实现
    uint16_t data = value << 2;
    
    // 发送数据
    digitalWrite(_csPin, LOW);  // 拉低CS，开始传输
    _spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    
    // 使用transfer16直接发送16位数据，而不是分两次发送
    _spi->transfer16(data);
    
    _spi->endTransaction();
    digitalWrite(_csPin, HIGH);  // 拉高CS，结束传输
}

// 设置DAC输出电压（0-4.096V）
void MyDAC::setVoltage(float voltage) {
    uint16_t value = voltageToDAC(voltage);
    setValue(value);
    
    #ifdef DAC_DEBUG
    Serial.print("设置DAC电压: ");
    Serial.print(voltage);
    Serial.print("V (DAC值: ");
    Serial.print(value);
    Serial.println(")");
    #endif
}

// 获取当前DAC值（0-1023）
uint16_t MyDAC::getCurrentValue() {
    return _currentValue;
}

// 获取当前电压值（0-4.096V）
float MyDAC::getCurrentVoltage() {
    return (_currentValue * DAC_MAX_VOLTAGE) / DAC_MAX_VALUE;
}

// 电压设置值到DAC输出电压的转换函数
float MyDAC::calculateDACOutputVoltage(float setVoltage) {
    float dacOutputVoltage = setVoltage * -0.1994f + 4.2162f;
    
    // 确保DAC输出电压在有效范围内（0-4.096V）
    if (dacOutputVoltage < 0.0f) {
        Serial.println("警告: DAC输出电压被限制为最小值0V");
        dacOutputVoltage = 0.0f;
    } else if (dacOutputVoltage > DAC_MAX_VOLTAGE) {
        Serial.println("警告: DAC输出电压被限制为最大值4.096V");
        dacOutputVoltage = DAC_MAX_VOLTAGE;
    }
    
    return dacOutputVoltage;
}

// 设置电压值并自动计算DAC输出电压
void MyDAC::setVoltageFromSetValue(float setVoltage) {
    float dacOutputVoltage = calculateDACOutputVoltage(setVoltage);
    
    Serial.print("设置电压: ");
    Serial.print(setVoltage);
    Serial.print("V, 对应的DAC输出电压: ");
    Serial.print(dacOutputVoltage);
    Serial.println("V");
    
    this->setVoltage(dacOutputVoltage);
}

// DAC任务函数 - 从队列接收电压值并设置DAC输出
void dacTask(void * parameter) {
    float voltage = 0.0;
    
    // 检查全局DAC实例和队列
    if (globalDacInstance == NULL || dacVoltageQueue == NULL) {
        Serial.println("DAC任务错误: DAC实例或队列未初始化");
        vTaskDelete(NULL);
        return;
    }
    
    Serial.println("DAC任务已启动");
    
    while(1) {
        // 从队列接收电压值，阻塞等待直到有新值
        if (xQueueReceive(dacVoltageQueue, &voltage, portMAX_DELAY) == pdTRUE) {
            globalDacInstance->setVoltage(voltage);
        }
        
        // 短暂延时以允许其他任务运行
        vTaskDelay(1);
    }
}

// 创建DAC任务
void createDACTask(MyDAC* dac, uint8_t priority, uint8_t core) {
    // 如果任务已经存在，先停止
    if (dacTaskHandle != NULL) {
        stopDACTask();
    }
    
    // 保存DAC实例到全局变量
    globalDacInstance = dac;
    
    // 创建电压队列
    if (dacVoltageQueue == NULL) {
        dacVoltageQueue = xQueueCreate(10, sizeof(float));
        if (dacVoltageQueue == NULL) {
            Serial.println("错误: 无法创建DAC电压队列");
            return;
        }
    }
    
    // 创建DAC任务
    BaseType_t result = xTaskCreatePinnedToCore(
        dacTask,         // 任务函数
        "DAC_Task",      // 任务名称
        2048,            // 任务堆栈大小
        NULL,            // 任务参数
        priority,        // 任务优先级
        &dacTaskHandle,  // 任务句柄
        core             // 运行核心
    );
    
    if (result != pdPASS) {
        Serial.println("错误: 无法创建DAC任务");
        dacTaskHandle = NULL;
    } else {
        Serial.printf("DAC任务已创建: 优先级=%d, 核心=%d\n", priority, core);
    }
}

// 停止DAC任务
void stopDACTask() {
    if (dacTaskHandle != NULL) {
        vTaskDelete(dacTaskHandle);
        dacTaskHandle = NULL;
        Serial.println("DAC任务已停止");
    }
    
    // 清空队列
    if (dacVoltageQueue != NULL) {
        xQueueReset(dacVoltageQueue);
    }
}

// 通过队列设置DAC电压
void setDACVoltage(float voltage) {
    if (dacVoltageQueue != NULL) {
        // 将电压值发送到队列，最大等待时间为10个时钟周期
        if (xQueueSend(dacVoltageQueue, &voltage, 10) != pdTRUE) {
            Serial.println("警告: DAC电压队列已满");
        }
    } else {
        Serial.println("错误: DAC电压队列未创建");
    }
}

// 通过队列设置电压值（自动转换为DAC输出电压）
void setDACVoltageFromSetValue(float setVoltage) {
    float dacOutputVoltage = MyDAC::calculateDACOutputVoltage(setVoltage);
    setDACVoltage(dacOutputVoltage);
}