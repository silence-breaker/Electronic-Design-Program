#ifndef MY_DAC_H
#define MY_DAC_H

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// TLC5615 DAC参数定义
#define DAC_MAX_VALUE 1023      // TLC5615是10位DAC，范围是0-1023
#define DAC_MAX_VOLTAGE 4.096f  // TLC5615最大输出电压为4.096V

class MyDAC {
private:
    uint8_t _csPin;
    uint8_t _mosiPin;
    uint8_t _sckPin;
    uint16_t _currentValue;
    SPIClass* _spi;
    
    // 电压转换为DAC值 (0-1023)
    uint16_t voltageToDAC(float voltage);

public:
    // 构造函数，支持完整的SPI引脚配置
    MyDAC(uint8_t csPin, uint8_t mosiPin, uint8_t sckPin);
    void begin();
    
    // 设置DAC输出值 (0-1023)
    void setValue(uint16_t value);
    
    // 设置DAC输出电压 (0-4.096V)
    void setVoltage(float voltage);
    
    // 获取当前DAC值 (0-1023)
    uint16_t getCurrentValue();
      // 获取当前电压值 (0-4.096V)
    float getCurrentVoltage();
    
    // 电压设置值到DAC输出电压的转换函数
    static float calculateDACOutputVoltage(float setVoltage);
    
    // 设置电压值并自动计算DAC输出电压
    void setVoltageFromSetValue(float setVoltage);
};

// FreeRTOS DAC任务相关
extern TaskHandle_t dacTaskHandle;
extern QueueHandle_t dacVoltageQueue;   // 电压值队列

// FreeRTOS任务函数和控制函数
void createDACTask(MyDAC* dac, uint8_t priority = 1, uint8_t core = 1);
void stopDACTask();
void setDACVoltage(float voltage);  // 通过队列设置电压

// 通过队列设置电压值（自动转换为DAC输出电压）
void setDACVoltageFromSetValue(float setVoltage);

#endif // MY_DAC_H