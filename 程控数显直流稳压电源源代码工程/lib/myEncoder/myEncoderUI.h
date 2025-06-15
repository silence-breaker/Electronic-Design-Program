#ifndef MY_ENCODER_UI_H
#define MY_ENCODER_UI_H

// U_SET显示回调函数 - 处理电压设置值显示和颜色变化
void updateUSetDisplay(float value, bool confirmed, bool isFineStep, void* encoderPtr);

#endif // MY_ENCODER_UI_H
