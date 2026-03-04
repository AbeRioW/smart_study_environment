#include "uln2003.h"

// 四相单4拍控制序列 (IN1-IN2-IN3-IN4顺序)
// 针对28BYJ-48: 蓝(IN1)-黄(IN2)-粉(IN3)-橙(IN4) - 交换线序
static const uint8_t ULN2003_Seq[4] = {
    0x01, // IN1通电 (蓝)
    0x02, // IN2通电 (黄)
    0x04, // IN3通电 (粉)
    0x08  // IN4通电 (橙)
};

// 当前速度（单位：ms/步）
static uint16_t uln2003_speed = ULN2003_SPEED_MEDIUM;

// 非阻塞式电机控制状态变量
static volatile ULN2003_State motor_state = ULN2003_STATE_STOP;
static volatile uint16_t motor_steps_target = 0;
static volatile uint16_t motor_steps_done = 0;
static volatile uint8_t motor_seq_index = 0;
static volatile uint32_t motor_last_step_time = 0;
static volatile uint8_t motor_direction = 1; // 1=正转, 0=反转

// 延时函数
void ULN2003_Delay(uint16_t ms)
{
    HAL_Delay(ms);
}

// 设置步进电机的相状态
void ULN2003_SetPhase(uint8_t phase)
{
    // 设置IN1
    if (phase & 0x01)
    {
        HAL_GPIO_WritePin(ULN2003_IN1_GPIO_Port, ULN2003_IN1_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(ULN2003_IN1_GPIO_Port, ULN2003_IN1_Pin, GPIO_PIN_RESET);
    }
    
    // 设置IN2
    if (phase & 0x02)
    {
        HAL_GPIO_WritePin(ULN2003_IN2_GPIO_Port, ULN2003_IN2_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(ULN2003_IN2_GPIO_Port, ULN2003_IN2_Pin, GPIO_PIN_RESET);
    }
    
    // 设置IN3
    if (phase & 0x04)
    {
        HAL_GPIO_WritePin(ULN2003_IN3_GPIO_Port, ULN2003_IN3_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(ULN2003_IN3_GPIO_Port, ULN2003_IN3_Pin, GPIO_PIN_RESET);
    }
    
    // 设置IN4
    if (phase & 0x08)
    {
        HAL_GPIO_WritePin(ULN2003_IN4_GPIO_Port, ULN2003_IN4_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(ULN2003_IN4_GPIO_Port, ULN2003_IN4_Pin, GPIO_PIN_RESET);
    }
}

/**
  * @brief  初始化ULN2003步进电机
  * @retval None
  */
void ULN2003_Init(void)
{
    // 初始化GPIO
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // IN1引脚
    GPIO_InitStruct.Pin = ULN2003_IN1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ULN2003_IN1_GPIO_Port, &GPIO_InitStruct);
    
    // IN2引脚
    GPIO_InitStruct.Pin = ULN2003_IN2_Pin;
    HAL_GPIO_Init(ULN2003_IN2_GPIO_Port, &GPIO_InitStruct);
    
    // IN3引脚
    GPIO_InitStruct.Pin = ULN2003_IN3_Pin;
    HAL_GPIO_Init(ULN2003_IN3_GPIO_Port, &GPIO_InitStruct);
    
    // IN4引脚
    GPIO_InitStruct.Pin = ULN2003_IN4_Pin;
    HAL_GPIO_Init(ULN2003_IN4_GPIO_Port, &GPIO_InitStruct);
    
    // 初始状态：所有相都不通电
    ULN2003_SetPhase(0x00);
    
    // 初始化非阻塞状态
    motor_state = ULN2003_STATE_STOP;
    motor_steps_target = 0;
    motor_steps_done = 0;
    motor_seq_index = 0;
    motor_last_step_time = 0;
}

/**
  * @brief  设置步进电机速度
  * @param  speed: 速度，单位ms/步
  * @retval None
  */
void ULN2003_SetSpeed(uint16_t speed)
{
    uln2003_speed = speed;
}

/**
  * @brief  步进电机正转（阻塞式）
  * @param  steps: 步数
  * @retval None
  */
void ULN2003_Forward(uint16_t steps)
{
    uint16_t i;
    uint8_t seq_index = 0;
    
    for (i = 0; i < steps; i++)
    {
        // 设置当前相状态
        ULN2003_SetPhase(ULN2003_Seq[seq_index]);
        
        // 延时
        ULN2003_Delay(uln2003_speed);
        
        // 更新序列索引
        seq_index = (seq_index + 1) % 4;
    }
    
    // 停止步进电机
    ULN2003_Stop();
}

/**
  * @brief  步进电机反转（阻塞式）
  * @param  steps: 步数
  * @retval None
  */
void ULN2003_Backward(uint16_t steps)
{
    uint16_t i;
    uint8_t seq_index = 3;
    
    for (i = 0; i < steps; i++)
    {
        // 设置当前相状态
        ULN2003_SetPhase(ULN2003_Seq[seq_index]);
        
        // 延时
        ULN2003_Delay(uln2003_speed);
        
        // 更新序列索引
        seq_index = (seq_index - 1) % 4;
    }
    
    // 停止步进电机
    ULN2003_Stop();
}

/**
  * @brief  停止步进电机
  * @retval None
  */
void ULN2003_Stop(void)
{
    // 所有相都不通电
    ULN2003_SetPhase(0x00);
}

/**
  * @brief  步进电机旋转指定角度（阻塞式）
  * @param  angle: 角度（0-360）
  * @param  direction: 方向（0-反转，1-正转）
  * @retval None
  */
void ULN2003_Rotate(uint16_t angle, uint8_t direction)
{
    // 28BYJ-48步进电机的步距角为5.625度/步，减速比为1:64
    // 所以实际每步转动的角度为5.625/64 = 0.087890625度
    // 转动指定角度需要的步数 = 角度 * 64 * 64 / 360 = 角度 * 4096 / 360
    // 简化: 角度 * 512 / 45
    uint16_t steps = (uint16_t)((uint32_t)angle * 512 / 45);
    
    if (direction)
    {
        ULN2003_Forward(steps);
    }
    else
    {
        ULN2003_Backward(steps);
    }
}

/* ==================== 非阻塞式电机控制接口 ==================== */

/**
  * @brief  启动非阻塞式正转
  * @param  steps: 步数
  * @retval None
  */
void ULN2003_StartForward_NB(uint16_t steps)
{
    if (motor_state == ULN2003_STATE_STOP)
    {
        motor_state = ULN2003_STATE_FORWARD;
        motor_steps_target = steps;
        motor_steps_done = 0;
        motor_seq_index = 0;
        motor_direction = 1;
        motor_last_step_time = HAL_GetTick();
        
        // 立即执行第一步
        ULN2003_SetPhase(ULN2003_Seq[motor_seq_index]);
        motor_seq_index = (motor_seq_index + 1) % 4;
        motor_steps_done++;
    }
}

/**
  * @brief  启动非阻塞式反转
  * @param  steps: 步数
  * @retval None
  */
void ULN2003_StartBackward_NB(uint16_t steps)
{
    if (motor_state == ULN2003_STATE_STOP)
    {
        motor_state = ULN2003_STATE_BACKWARD;
        motor_steps_target = steps;
        motor_steps_done = 0;
        motor_seq_index = 3;
        motor_direction = 0;
        motor_last_step_time = HAL_GetTick();
        
        // 立即执行第一步
        ULN2003_SetPhase(ULN2003_Seq[motor_seq_index]);
        motor_seq_index = (motor_seq_index - 1) % 4;
        motor_steps_done++;
    }
}

/**
  * @brief  非阻塞式电机控制处理函数，需要在主循环中周期性调用
  * @retval None
  */
void ULN2003_Handle_NB(void)
{
    if (motor_state == ULN2003_STATE_STOP)
    {
        return;
    }
    
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否到达下一步的时间
    if ((current_time - motor_last_step_time) < uln2003_speed)
    {
        return;
    }
    
    // 检查是否完成所有步数
    if (motor_steps_done >= motor_steps_target)
    {
        ULN2003_Stop();
        motor_state = ULN2003_STATE_STOP;
        return;
    }
    
    // 执行下一步
    ULN2003_SetPhase(ULN2003_Seq[motor_seq_index]);
    motor_last_step_time = current_time;
    
    if (motor_state == ULN2003_STATE_FORWARD)
    {
        motor_seq_index = (motor_seq_index + 1) % 4;
    }
    else // ULN2003_STATE_BACKWARD
    {
        motor_seq_index = (motor_seq_index - 1) % 4;
    }
    
    motor_steps_done++;
}

/**
  * @brief  检查电机是否正在运行（非阻塞模式）
  * @retval 0=停止, 1=运行中
  */
uint8_t ULN2003_IsRunning(void)
{
    return (motor_state != ULN2003_STATE_STOP) ? 1 : 0;
}

/**
  * @brief  停止非阻塞式电机运行
  * @retval None
  */
void ULN2003_Stop_NB(void)
{
    motor_state = ULN2003_STATE_STOP;
    ULN2003_Stop();
}

/**
  * @brief  启动非阻塞式旋转指定角度
  * @param  angle: 角度（0-360）
  * @param  direction: 方向（0-反转，1-正转）
  * @retval None
  */
void ULN2003_StartRotate_NB(uint16_t angle, uint8_t direction)
{
    // 28BYJ-48步进电机的步距角为5.625度/步，减速比为1:64
    // 所以实际每步转动的角度为5.625/64 = 0.087890625度
    // 转动指定角度需要的步数 = 角度 * 64 * 64 / 360 = 角度 * 4096 / 360
    // 简化: 角度 * 512 / 45
    uint16_t steps = (uint16_t)((uint32_t)angle * 512 / 45);
    
    if (direction)
    {
        ULN2003_StartForward_NB(steps);
    }
    else
    {
        ULN2003_StartBackward_NB(steps);
    }
}
