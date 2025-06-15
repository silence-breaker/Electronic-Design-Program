/**
 * @file myADC.h
 * @brief ADC功能实现，用于检测输入输出电压和电流
 * @author GitHub Copilot
 * @date 2025-05-27
 */

#ifndef MY_ADC_H
#define MY_ADC_H

#include <Arduino.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "../generated/gui_guider.h"

// ADC引脚定义
#define ADC_U_IN_PIN   ADC1_CHANNEL_0  // GPIO1 - 输入电压检测
#define ADC_I_IN_PIN   ADC1_CHANNEL_1  // GPIO2 - 输入电流检测
#define ADC_U_OUT_PIN  ADC1_CHANNEL_2  // GPIO3 - 输出电压检测
#define ADC_I_OUT_PIN  ADC1_CHANNEL_3  // GPIO4 - 输出电流检测

// 采样参数
#define ADC_SAMPLES_COUNT 10          // 每次测量的采样次数
#define ADC_SAMPLE_INTERVAL 50        // 采样间隔，单位ms
#define ADC_UPDATE_INTERVAL 500       // 更新间隔，单位ms

class MyADC {
public:
    /**
     * @brief 构造函数
     * @param ui 指向UI界面的指针，用于更新显示
     */
    MyADC(lv_ui *ui);
    
    /**
     * @brief 初始化ADC引脚和参数
     */
    void begin();
    
    /**
     * @brief 读取ADC数据并更新UI
     */
    void update();
      /**
     * @brief 设置电压校准系数
     * @param ch1_k 输入电压校准系数
     * @param ch2_k 输入电流校准系数
     * @param ch3_k 输出电压校准系数
     * @param ch4_k 输出电流校准系数
     */
    void setCalibrationFactors(float ch1_k, float ch2_k, float ch3_k, float ch4_k);
    
    /**
     * @brief 输入电压校正函数
     * @param rawVoltage ADC原始电压值(V)
     * @return 校正后的输入电压值(V)
     */
    float correctInputVoltage(float rawVoltage);
    
    /**
     * @brief 输入电流校正函数
     * @param rawCurrent ADC原始电流值(A)
     * @return 校正后的输入电流值(A)
     */
    float correctInputCurrent(float rawCurrent);
    
    /**
     * @brief 输出电压校正函数
     * @param rawVoltage ADC原始电压值(V)
     * @return 校正后的输出电压值(V)
     */
    float correctOutputVoltage(float rawVoltage);
      /**
     * @brief 输出电流校正函数
     * @param rawCurrent ADC原始电流值(A)
     * @return 校正后的输出电流值(A)
     */
    float correctOutputCurrent(float rawCurrent);
    
    /**
     * @brief 设置高级校正参数
     * @param channel 通道号 (0=U_IN, 1=I_IN, 2=U_OUT, 3=I_OUT)
     * @param linearCoeff 线性系数
     * @param offset 偏移量
     * @param quadraticCoeff 二次项系数(可选)
     */
    void setAdvancedCalibration(uint8_t channel, float linearCoeff, float offset, float quadraticCoeff = 0.0f);
    
    /**
     * @brief 获取当前输入电压
     * @return 输入电压值
     */
    float getInputVoltage() const { return u_in; }
    
    /**
     * @brief 获取当前输入电流
     * @return 输入电流值
     */
    float getInputCurrent() const { return i_in; }
    
    /**
     * @brief 获取当前输出电压
     * @return 输出电压值
     */
    float getOutputVoltage() const { return u_out; }
    
    /**
     * @brief 获取当前输出电流
     * @return 输出电流值
     */
    float getOutputCurrent() const { return i_out; }
      /**
     * @brief 获取当前输出功率
     * @return 输出功率值
     */
    float getOutputPower() const { return u_out * i_out; }
    
    /**
     * @brief 更新UI上的数值显示
     */
    void updateUI();

private:
    lv_ui *ui_ptr;                    // 指向UI的指针
    esp_adc_cal_characteristics_t adc_chars; // ADC校准特性
    unsigned long lastUpdateTime;     // 上次更新时间    // 校准系数
    float k_u_in;    // 输入电压校准系数
    float k_i_in;    // 输入电流校准系数
    float k_u_out;   // 输出电压校准系数
    float k_i_out;   // 输出电流校准系数
    
    // 高级校正参数
    float offset_u_in, offset_i_in, offset_u_out, offset_i_out;       // 偏移量
    float quad_u_in, quad_i_in, quad_u_out, quad_i_out;               // 二次项系数

    // 测量值
    float u_in;      // 输入电压
    float i_in;      // 输入电流
    float u_out;     // 输出电压
    float i_out;     // 输出电流

    /**
     * @brief 读取ADC通道的原始值，并进行多次采样平均
     * @param channel ADC通道
     * @return 转换后的电压值（mV）
     */
    uint32_t readADCRaw(adc1_channel_t channel);
};

#endif // MY_ADC_H
