#ifndef __ULN2003_H
#define __ULN2003_H

#include "main.h"

// 步进电机状态定义
typedef enum {
    ULN2003_STATE_STOP,
    ULN2003_STATE_FORWARD,
    ULN2003_STATE_BACKWARD
} ULN2003_State;

// 步进电机速度定义（单位：ms/步）
#define ULN2003_SPEED_SLOW    10
#define ULN2003_SPEED_MEDIUM   5
#define ULN2003_SPEED_FAST     2

// 函数原型 - 阻塞式接口
void ULN2003_Init(void);
void ULN2003_SetSpeed(uint16_t speed);
void ULN2003_Forward(uint16_t steps);
void ULN2003_Backward(uint16_t steps);
void ULN2003_Stop(void);
void ULN2003_Rotate(uint16_t angle, uint8_t direction);

// 函数原型 - 非阻塞式接口（推荐用于主循环）
void ULN2003_StartForward_NB(uint16_t steps);
void ULN2003_StartBackward_NB(uint16_t steps);
void ULN2003_StartRotate_NB(uint16_t angle, uint8_t direction);
void ULN2003_Handle_NB(void);
void ULN2003_Stop_NB(void);
uint8_t ULN2003_IsRunning(void);

#endif
