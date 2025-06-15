/**
 * @file myADC.cpp
 * @brief ADC功能实现，用于检测输入输出电压和电流
 * @author watermelon6uice
 * @date 2025-05-27
 */

#include "myADC.h"

// ADC参考电压
#define DEFAULT_VREF    1100        // 使用默认参考电压
#define ADC_ATTEN       ADC_ATTEN_DB_12 // 12dB衰减，量程0-3.3V

MyADC::MyADC(lv_ui *ui) : 
    ui_ptr(ui),
    lastUpdateTime(0),
    k_u_in(1.0f),
    k_i_in(1.0f),
    k_u_out(1.0f),
    k_i_out(1.0f),
    offset_u_in(0.0f),
    offset_i_in(0.0f),
    offset_u_out(0.0f),
    offset_i_out(0.0f),
    quad_u_in(0.0f),
    quad_i_in(0.0f),
    quad_u_out(0.0f),
    quad_i_out(0.0f),
    u_in(0.0f),
    i_in(0.0f),
    u_out(0.0f),
    i_out(0.0f)
{
    // 构造函数初始化
}

void MyADC::begin() {
    // 设置ADC参数
    adc1_config_width(ADC_WIDTH_BIT_12);  // 设置ADC1为12位分辨率
    
    // 配置ADC下拉输入模式
    adc1_config_channel_atten(ADC_U_IN_PIN, ADC_ATTEN); // 输入电压检测
    adc1_config_channel_atten(ADC_I_IN_PIN, ADC_ATTEN); // 输入电流检测
    adc1_config_channel_atten(ADC_U_OUT_PIN, ADC_ATTEN); // 输出电压检测
    adc1_config_channel_atten(ADC_I_OUT_PIN, ADC_ATTEN); // 输出电流检测
    
    // 初始化ADC校准
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);
    
    Serial.println("ADC初始化完成");
}

void MyADC::setCalibrationFactors(float ch1_k, float ch2_k, float ch3_k, float ch4_k) {
    k_u_in = ch1_k;
    k_i_in = ch2_k;
    k_u_out = ch3_k;
    k_i_out = ch4_k;
    
    Serial.println("ADC校准系数已设置");
    Serial.print("U_IN校准系数: "); Serial.println(k_u_in);
    Serial.print("I_IN校准系数: "); Serial.println(k_i_in);
    Serial.print("U_OUT校准系数: "); Serial.println(k_u_out);
    Serial.print("I_OUT校准系数: "); Serial.println(k_i_out);
}

// 输入电压校正函数
float MyADC::correctInputVoltage(float rawVoltage) {
    // 输入电压校正公式: 校正值 = (原始值 * 线性系数 + 偏移量) + 二次项校正
    float correctedValue = rawVoltage * k_u_in + offset_u_in;
    
    // 添加二次项校正
    if (quad_u_in != 0.0f) {
        correctedValue += quad_u_in * rawVoltage * rawVoltage;
    }
    
    return correctedValue;
}

// 输入电流校正函数
float MyADC::correctInputCurrent(float rawCurrent) {
    // 输入电流校正公式: 校正值 = (原始值 * 线性系数 + 偏移量) + 二次项校正
    float correctedValue = rawCurrent * k_i_in + offset_i_in;
    
    // 添加二次项校正
    if (quad_i_in != 0.0f) {
        correctedValue += quad_i_in * rawCurrent * rawCurrent;
    }
    
    // 零点校正 - 小电流时设为0
    if (correctedValue < 0.01f && correctedValue > -0.01f) {
        correctedValue = 0.0f;
    }
    
    return correctedValue;
}

// 输出电压校正函数
float MyADC::correctOutputVoltage(float rawVoltage) {
    // 输出电压校正公式: 校正值 = (原始值 * 线性系数 + 偏移量) + 二次项校正
    float correctedValue = rawVoltage * k_u_out + offset_u_out;
    
    // 添加二次项校正
    if (quad_u_out != 0.0f) {
        correctedValue += quad_u_out * rawVoltage * rawVoltage;
    }
    
    return correctedValue;
}

// 输出电流校正函数
float MyADC::correctOutputCurrent(float rawCurrent) {
    // 输出电流校正公式: 校正值 = (原始值 * 线性系数 + 偏移量) + 二次项校正
    float correctedValue = rawCurrent * k_i_out + offset_i_out;
    
    // 添加二次项校正
    if (quad_i_out != 0.0f) {
        correctedValue += quad_i_out * rawCurrent * rawCurrent;
    }
    
    // 零点校正 - 小电流时设为0
    if (correctedValue < 0.01f && correctedValue > -0.01f) {
        correctedValue = 0.0f;
    }
    
    return correctedValue;
}

// 设置高级校正参数
void MyADC::setAdvancedCalibration(uint8_t channel, float linearCoeff, float offset, float quadraticCoeff) {
    switch (channel) {
        case 0: // U_IN
            k_u_in = linearCoeff;
            offset_u_in = offset;
            quad_u_in = quadraticCoeff;
            Serial.printf("U_IN高级校正: 线性系数=%.4f, 偏移=%.4f, 二次项=%.6f\n", 
                         linearCoeff, offset, quadraticCoeff);
            break;
        case 1: // I_IN
            k_i_in = linearCoeff;
            offset_i_in = offset;
            quad_i_in = quadraticCoeff;
            Serial.printf("I_IN高级校正: 线性系数=%.4f, 偏移=%.4f, 二次项=%.6f\n", 
                         linearCoeff, offset, quadraticCoeff);
            break;
        case 2: // U_OUT
            k_u_out = linearCoeff;
            offset_u_out = offset;
            quad_u_out = quadraticCoeff;
            Serial.printf("U_OUT高级校正: 线性系数=%.4f, 偏移=%.4f, 二次项=%.6f\n", 
                         linearCoeff, offset, quadraticCoeff);
            break;
        case 3: // I_OUT
            k_i_out = linearCoeff;
            offset_i_out = offset;
            quad_i_out = quadraticCoeff;
            Serial.printf("I_OUT高级校正: 线性系数=%.4f, 偏移=%.4f, 二次项=%.6f\n", 
                         linearCoeff, offset, quadraticCoeff);
            break;
        default:
            Serial.println("错误: 无效的通道号，应为0-3");
            break;
    }
}

uint32_t MyADC::readADCRaw(adc1_channel_t channel) {
    uint32_t adc_reading = 0;
    unsigned long startTime = millis();
    int samplesCollected = 0;
    
    // 多次采样取平均值，每次采样间隔一定时间，提高稳定性
    while (samplesCollected < ADC_SAMPLES_COUNT) {
        // 当前时间
        unsigned long currentTime = millis();
        
        // 检查是否达到采样间隔
        if (currentTime - startTime >= samplesCollected * ADC_SAMPLE_INTERVAL) {
            adc_reading += adc1_get_raw(channel);
            samplesCollected++;
            
            // 如果已经采集够了样本数，跳出循环
            if (samplesCollected >= ADC_SAMPLES_COUNT) {
                break;
            }
        }
        
        // 短暂延时，避免过度占用CPU
        delay(1);
    }
    
    // 计算平均值
    adc_reading /= ADC_SAMPLES_COUNT;
    
    // 将ADC原始值转换为电压值(mV)
    uint32_t voltage = 0;
    voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars) * 1.0;
    
    return voltage;
}

void MyADC::update() {
    // 检查是否达到更新间隔
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime < ADC_UPDATE_INTERVAL) {
        // 未达到更新间隔，直接返回
        return;
    }
    
    // 读取各通道的原始电压值(V)
    float u_in_raw = readADCRaw(ADC_U_IN_PIN) / 1000.0f;  // 转换为V
    float i_in_raw = readADCRaw(ADC_I_IN_PIN) / 1000.0f;  // 转换为A
    float u_out_raw = readADCRaw(ADC_U_OUT_PIN) / 1000.0f;  // 转换为V
    float i_out_raw = readADCRaw(ADC_I_OUT_PIN) / 1000.0f;  // 转换为A
    
    // 使用校正函数进行校正
    u_in = correctInputVoltage(u_in_raw);
    i_in = correctInputCurrent(i_in_raw);
    u_out = correctOutputVoltage(u_out_raw);
    i_out = correctOutputCurrent(i_out_raw);
    
    // 记录更新时间
    lastUpdateTime = currentTime;
    
    // 减少打印调试信息，以降低堆栈使用
//#define ADC_DEBUG
#ifdef ADC_DEBUG
    Serial.print("电压: U_in="); Serial.print(u_in);
    Serial.print("V, I_in="); Serial.print(i_in);
    Serial.print("A, U_out="); Serial.print(u_out);
    Serial.print("V, I_out="); Serial.print(i_out);
    Serial.print("A, P_out="); Serial.print(u_out * i_out);
    Serial.println("W");
#endif
}

void MyADC::updateUI() {
    if (ui_ptr == nullptr) {
        return;
    }
    
    // 格式化字符串，显示两位小数
    char u_in_str[16];
    char i_in_str[16];
    char u_out_str[16];
    char i_out_str[16];
    char p_out_str[16];
    char p_in_str[16];
    char efficiency_str[8];

    // 计算效率，避免除以零
    int efficiency = 0;
    float pin = u_in * i_in;
    if (pin > 0.01f) {
        efficiency = static_cast<int>((u_out * i_out) / pin * 100.0f + 0.5f); // 四舍五入
    }
    
    
    // 格式化为两位小数
    snprintf(u_in_str, sizeof(u_in_str), "%.2f", u_in);
    snprintf(i_in_str, sizeof(i_in_str), "%.2f", i_in);
    snprintf(u_out_str, sizeof(u_out_str), "%.2f", u_out);
    snprintf(i_out_str, sizeof(i_out_str), "%.2f", i_out);
    snprintf(p_out_str, sizeof(p_out_str), "%.2f", u_out * i_out);
    snprintf(p_in_str, sizeof(p_in_str), "%.2f", u_in * i_in);
    snprintf(efficiency_str, sizeof(efficiency_str), "%d", efficiency);

    // 更新UI标签
    lv_label_set_text(ui_ptr->screen_U_IN, u_in_str);
    lv_label_set_text(ui_ptr->screen_I_IN, i_in_str);
    lv_label_set_text(ui_ptr->screen_Uout, u_out_str);
    lv_label_set_text(ui_ptr->screen_Iout, i_out_str);
    lv_label_set_text(ui_ptr->screen_Pout, p_out_str);
    lv_label_set_text(ui_ptr->screen_P_IN, p_in_str);
    lv_label_set_text(ui_ptr->screen_Efficiency, efficiency_str);
}