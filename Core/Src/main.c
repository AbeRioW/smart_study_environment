/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dht11.h"
#include "oled.h"
#include "uln2003.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_flash_ex.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define KEY_DEBOUNCE_MS 200

// Flash存储地址 (使用倒数第2页，避免覆盖程序代码)
// STM32F103C8T6有128页，每页1KB
#define FLASH_STORAGE_ADDR  0x0801F000  // 第126页起始地址
#define FLASH_MAGIC_NUMBER  0x1234ABCD  // 魔数，用于验证数据有效性

// Flash数据结构
typedef struct {
    uint32_t magic;          // 魔数
    uint8_t temp_threshold;  // 温度阈值
    uint8_t humidity_threshold; // 湿度阈值
    uint16_t mq5_threshold;  // MQ5阈值
    uint16_t noise_threshold; // 噪声阈值
    uint16_t light_threshold; // 光照阈值
    uint32_t checksum;       // 校验和
} Flash_Data_t;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 阈值变量（可在设置界面修改）
volatile uint16_t noise_threshold = 2000;
volatile uint16_t light_threshold = 300;
volatile uint16_t mq5_threshold = 2000;
volatile uint8_t humidity_threshold = 90;
volatile uint8_t temp_threshold = 20;

// 设置界面状态
volatile uint8_t setting_mode = 0;      // 0:主界面, 1:设置界面
volatile uint8_t setting_item = 0;      // 0:温度, 1:湿度, 2:MQ5, 3:noise, 4:light, 5:退出

// 按键消抖标志
volatile uint8_t key1_pressed = 0;
volatile uint8_t key2_pressed = 0;
volatile uint8_t key3_pressed = 0;
volatile uint32_t key_debounce_time = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 简单步进电机测试 - 4拍驱动
const uint8_t motor_seq[4] = {0x01, 0x02, 0x04, 0x08}; // IN1, IN2, IN3, IN4

// 步进电机标志
uint8_t stepper_flag = 0;

// USART2中断接收变量
extern uint8_t rx_data;
extern uint8_t rx_index;
extern uint8_t rx_buffer[];
extern uint32_t last_rx_time;
#define RX_TIMEOUT_MS 1000

// 函数声明
void Show_Main_Interface(void);
void Show_Setting_Interface(void);
void Update_Setting_Value(void);
void Update_Setting_Indicator(void);
void Process_Setting(void);
void Stepper_Test(void);
void Flash_Save_Thresholds(void);
void Flash_Load_Thresholds(void);
uint32_t Calculate_Checksum(Flash_Data_t *data);

// 计算校验和
uint32_t Calculate_Checksum(Flash_Data_t *data)
{
    uint32_t checksum = 0;
    checksum += data->magic;
    checksum += data->temp_threshold;
    checksum += data->humidity_threshold;
    checksum += data->mq5_threshold;
    checksum += data->noise_threshold;
    checksum += data->light_threshold;
    return checksum;
}

// 从Flash读取阈值参数
void Flash_Load_Thresholds(void)
{
    Flash_Data_t *flash_data = (Flash_Data_t *)FLASH_STORAGE_ADDR;
    
    // 检查魔数和校验和
    if(flash_data->magic == FLASH_MAGIC_NUMBER)
    {
        uint32_t calc_checksum = Calculate_Checksum(flash_data);
        if(calc_checksum == flash_data->checksum)
        {
            // 数据有效，加载阈值
            temp_threshold = flash_data->temp_threshold;
            humidity_threshold = flash_data->humidity_threshold;
            mq5_threshold = flash_data->mq5_threshold;
            noise_threshold = flash_data->noise_threshold;
            light_threshold = flash_data->light_threshold;
            printf("Flash: Thresholds loaded successfully\r\n");
            return;
        }
    }
    
    // 数据无效，使用默认值
    printf("Flash: No valid data found, using defaults\r\n");
}

// 保存阈值参数到Flash
void Flash_Save_Thresholds(void)
{
    HAL_StatusTypeDef status;
    Flash_Data_t flash_data;
    uint32_t PageError = 0;
    FLASH_EraseInitTypeDef EraseInitStruct;
    
    // 填充数据结构
    flash_data.magic = FLASH_MAGIC_NUMBER;
    flash_data.temp_threshold = temp_threshold;
    flash_data.humidity_threshold = humidity_threshold;
    flash_data.mq5_threshold = mq5_threshold;
    flash_data.noise_threshold = noise_threshold;
    flash_data.light_threshold = light_threshold;
    flash_data.checksum = Calculate_Checksum(&flash_data);
    
    // 解锁Flash
    HAL_FLASH_Unlock();
    
    // 擦除页
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = FLASH_STORAGE_ADDR;
    EraseInitStruct.NbPages = 1;
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
    
    if(status != HAL_OK)
    {
        printf("Flash: Erase failed\r\n");
        HAL_FLASH_Lock();
        return;
    }
    
    // 写入数据（按半字写入）
    uint16_t *data_ptr = (uint16_t *)&flash_data;
    uint32_t addr = FLASH_STORAGE_ADDR;
    uint32_t data_size = sizeof(Flash_Data_t);
    
    for(uint32_t i = 0; i < (data_size / 2); i++)
    {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + (i * 2), data_ptr[i]);
        if(status != HAL_OK)
        {
            printf("Flash: Write failed at offset %d\r\n", i);
            HAL_FLASH_Lock();
            return;
        }
    }
    
    // 锁定Flash
    HAL_FLASH_Lock();
    
    printf("Flash: Thresholds saved successfully\r\n");
}

void Stepper_Test(void)
{
    // 转30度，约170步 (28BYJ-48: 4096步/圈)
    uint16_t steps = 170;
    uint8_t i, j;
    
    printf("DEBUG: Stepper motor start\r\n");
    
    for(i = 0; i < steps; i++)
    {
        for(j = 0; j < 4; j++)
        {
            uint8_t val = motor_seq[j];
            HAL_GPIO_WritePin(ULN2003_IN1_GPIO_Port, ULN2003_IN1_Pin, (val & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(ULN2003_IN2_GPIO_Port, ULN2003_IN2_Pin, (val & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(ULN2003_IN3_GPIO_Port, ULN2003_IN3_Pin, (val & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(ULN2003_IN4_GPIO_Port, ULN2003_IN4_Pin, (val & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            
            // 使用非阻塞延时
            uint32_t start = HAL_GetTick();
            while(HAL_GetTick() - start < 15);
        }
    }
    
    // 停止
    HAL_GPIO_WritePin(ULN2003_IN1_GPIO_Port, ULN2003_IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ULN2003_IN2_GPIO_Port, ULN2003_IN2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ULN2003_IN3_GPIO_Port, ULN2003_IN3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ULN2003_IN4_GPIO_Port, ULN2003_IN4_Pin, GPIO_PIN_RESET);
    
    printf("DEBUG: Stepper motor stop\r\n");
}

// 显示主界面
void Show_Main_Interface(void)
{
    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t*)"H:", 8, 1);
    OLED_ShowString(65, 0, (uint8_t*)"T:", 8, 1);
    OLED_ShowString(0, 8, (uint8_t*)"MQ5:", 8, 1);
    OLED_ShowString(0, 16, (uint8_t*)"Noise:", 8, 1);
    OLED_ShowString(0, 24, (uint8_t*)"Light:", 8, 1);
    OLED_Refresh();
}

// 显示设置界面
void Show_Setting_Interface(void)
{
    OLED_Clear();
    OLED_ShowString(30, 0, (uint8_t*)"SETTING", 8, 1);
    
    // 显示温度阈值 (10-40, 2位)
    OLED_ShowString(0, 8, (uint8_t*)"Temp:", 8, 1);
    OLED_ShowNum(35, 8, temp_threshold, 2, 8, 1);
    
    // 显示湿度阈值 (60-100, 3位)
    OLED_ShowString(65, 8, (uint8_t*)"Hum:", 8, 1);
    OLED_ShowNum(95, 8, humidity_threshold, 3, 8, 1);
    
    // 显示MQ5阈值 (2000-3000, 4位)
    OLED_ShowString(0, 16, (uint8_t*)"MQ5:", 8, 1);
    OLED_ShowNum(30, 16, mq5_threshold, 4, 8, 1);
    
    // 显示Noise阈值 (1000-2000, 4位)
    OLED_ShowString(65, 16, (uint8_t*)"N:", 8, 1);
    OLED_ShowNum(78, 16, noise_threshold, 4, 8, 1);
    
    // 显示Light阈值 (100-300, 3位)
    OLED_ShowString(0, 24, (uint8_t*)"Light:", 8, 1);
    OLED_ShowNum(42, 24, light_threshold, 3, 8, 1);
    
    // 始终显示Exit文本（在右下角，位置80，指示器在104）
    OLED_ShowString(80, 24, (uint8_t*)"Exit", 8, 1);
    
    // 显示当前选中项的指示 (放在数值后面，避免覆盖)
    switch(setting_item)
    {
        case 0: // 温度: 35+2*6=47, 指示器放在49
            OLED_ShowChar(49, 8, '<', 8, 1);
            break;
        case 1: // 湿度: 95+3*6=113, 指示器放在115
            OLED_ShowChar(115, 8, '<', 8, 1);
            break;
        case 2: // MQ5: 30+4*6=54, 指示器放在56
            OLED_ShowChar(56, 16, '<', 8, 1);
            break;
        case 3: // Noise: 78+4*6=102, 指示器放在104
            OLED_ShowChar(104, 16, '<', 8, 1);
            break;
        case 4: // Light: 42+3*6=60, 指示器放在62
            OLED_ShowChar(62, 24, '<', 8, 1);
            break;
        case 5: // 退出: 在Exit后面显示指示器 (80+4*6=104, 放106)
            OLED_ShowChar(106, 24, '<', 8, 1);
            break;
    }
    
    OLED_Refresh();
}

// 更新设置界面中当前选中项的数值显示（不清屏，避免闪烁）
void Update_Setting_Value(void)
{
    switch(setting_item)
    {
        case 0: // 温度 (位置35, 2位)
            OLED_ShowNum(35, 8, temp_threshold, 2, 8, 1);
            break;
        case 1: // 湿度 (位置95, 3位)
            OLED_ShowNum(95, 8, humidity_threshold, 3, 8, 1);
            break;
        case 2: // MQ5 (位置30, 4位)
            OLED_ShowNum(30, 16, mq5_threshold, 4, 8, 1);
            break;
        case 3: // Noise (位置78, 4位)
            OLED_ShowNum(78, 16, noise_threshold, 4, 8, 1);
            break;
        case 4: // Light (位置42, 3位)
            OLED_ShowNum(42, 24, light_threshold, 3, 8, 1);
            break;
    }
    OLED_Refresh();
}

// 更新设置界面中的选中指示器（切换选项时调用）
void Update_Setting_Indicator(void)
{
    // 先清除所有指示器位置（用空格覆盖）
    OLED_ShowChar(49, 8, ' ', 8, 1);   // 温度指示器位置 (35+2*6=47, 放49)
    OLED_ShowChar(115, 8, ' ', 8, 1);  // 湿度指示器位置 (95+3*6=113, 放115)
    OLED_ShowChar(56, 16, ' ', 8, 1);  // MQ5指示器位置 (30+4*6=54, 放56)
    OLED_ShowChar(104, 16, ' ', 8, 1); // Noise指示器位置 (78+4*6=102, 放104)
    OLED_ShowChar(62, 24, ' ', 8, 1);  // Light指示器位置 (42+3*6=60, 放62)
    OLED_ShowChar(106, 24, ' ', 8, 1); // 退出指示器位置 (80+4*6=104, 放106)
    
    // 显示当前选中项的指示
    switch(setting_item)
    {
        case 0: // 温度
            OLED_ShowChar(49, 8, '<', 8, 1);
            break;
        case 1: // 湿度
            OLED_ShowChar(115, 8, '<', 8, 1);
            break;
        case 2: // MQ5
            OLED_ShowChar(56, 16, '<', 8, 1);
            break;
        case 3: // Noise
            OLED_ShowChar(104, 16, '<', 8, 1);
            break;
        case 4: // Light
            OLED_ShowChar(62, 24, '<', 8, 1);
            break;
        case 5: // 退出
            OLED_ShowChar(106, 24, '<', 8, 1);
            break;
    }
    OLED_Refresh();
}

// 处理设置界面的按键操作
void Process_Setting(void)
{
    // 处理KEY1 - 切换设置项
    if(key1_pressed)
    {
        key1_pressed = 0;
        setting_item++;
        if(setting_item > 5)
        {
            setting_item = 0;
            setting_mode = 0;  // 退出设置界面
            Flash_Save_Thresholds();  // 保存阈值到Flash
            Show_Main_Interface();
            return;
        }
        Update_Setting_Indicator(); // 只更新指示器，不清屏
    }
    
    // 处理KEY2 - 增加
    if(key2_pressed)
    {
        key2_pressed = 0;
        switch(setting_item)
        {
            case 0: // 温度: 10-40循环,步长1
                temp_threshold++;
                if(temp_threshold > 40)
                    temp_threshold = 10;
                break;
            case 1: // 湿度: 60-100循环,步长1
                humidity_threshold++;
                if(humidity_threshold > 100)
                    humidity_threshold = 60;
                break;
            case 2: // MQ5: 2000-3000循环,步长100
                mq5_threshold += 100;
                if(mq5_threshold > 3000)
                    mq5_threshold = 2000;
                break;
            case 3: // Noise: 1000-2000循环,步长100
                noise_threshold += 100;
                if(noise_threshold > 2000)
                    noise_threshold = 1000;
                break;
            case 4: // Light: 100-300循环,步长10
                light_threshold += 10;
                if(light_threshold > 300)
                    light_threshold = 100;
                break;
        }
        Update_Setting_Value(); // 只更新数值，不清屏
    }
    
    // 处理KEY3 - 减少
    if(key3_pressed)
    {
        key3_pressed = 0;
        switch(setting_item)
        {
            case 0: // 温度: 10-40循环,步长1
                temp_threshold--;
                if(temp_threshold < 10)
                    temp_threshold = 40;
                break;
            case 1: // 湿度: 60-100循环,步长1
                humidity_threshold--;
                if(humidity_threshold < 60)
                    humidity_threshold = 100;
                break;
            case 2: // MQ5: 2000-3000循环,步长100
                if(mq5_threshold >= 2100)
                    mq5_threshold -= 100;
                else
                    mq5_threshold = 3000;
                break;
            case 3: // Noise: 1000-2000循环,步长100
                if(noise_threshold >= 1100)
                    noise_threshold -= 100;
                else
                    noise_threshold = 2000;
                break;
            case 4: // Light: 100-300循环,步长10
                if(light_threshold >= 110)
                    light_threshold -= 10;
                else
                    light_threshold = 300;
                break;
        }
        Update_Setting_Value(); // 只更新数值，不清屏
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
DHT11_Data_t dht_data;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
   printf("go\r\n");
   OLED_Init();
   OLED_Clear();
   
   OLED_ShowString(0, 0, (uint8_t*)"H:", 8, 1);
   OLED_ShowString(65, 0, (uint8_t*)"T:", 8, 1);
   OLED_ShowString(0, 8, (uint8_t*)"MQ5:", 8, 1);
   OLED_ShowString(0, 16, (uint8_t*)"Noise:", 8, 1);
   OLED_ShowString(0, 24, (uint8_t*)"Light:", 8, 1);
   OLED_Refresh();
   
   // 初始化LAY引脚为低电平
   HAL_GPIO_WritePin(LAY_GPIO_Port, LAY_Pin, GPIO_PIN_RESET);
   
   // 测试USART2发送
   HAL_UART_Transmit(&huart2, (uint8_t*)"USART2 Ready\r\n", 13, 100);
   
   // 从Flash加载阈值参数
   Flash_Load_Thresholds();
   
   // ULN2003_Init(); // 暂时注释掉，使用简单测试函数
   // ULN2003_SetSpeed(ULN2003_SPEED_SLOW);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint16_t mq5_value, noise_value, light_value;
  uint8_t dht11_ok = 0;
  static uint8_t humidity_over = 0;  // 湿度超过阈值标志
  static uint8_t noise_over = 0;     // 噪音超过阈值标志
  static uint8_t mq5_over = 0;       // MQ5超过阈值标志
  
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		// 处理设置模式
		if(setting_mode)
		{
			Process_Setting();
			// 在设置模式下，只处理按键，不更新主界面显示
		}
		else
		{
			// 主界面模式：检查KEY1是否按下进入设置界面
			if(key1_pressed)
			{
				key1_pressed = 0;
				setting_mode = 1;
				setting_item = 0;
				Show_Setting_Interface();
				continue;  // 跳过本次循环的其余部分
			}
			
			// 检查串口接收超时
			if(rx_index > 0 && (HAL_GetTick() - last_rx_time) > RX_TIMEOUT_MS)
			{
				printf("DEBUG: RX timeout, resetting buffer\r\n");
				rx_index = 0;
				// 直接重置索引，usart.c 会处理缓冲区
			}
			
			dht11_ok = (DHT11_READ_DATA(&dht_data) == 0);
			
			if(dht11_ok)
			{
				OLED_ShowNum(10, 0, dht_data.humidity_int, 2, 8, 1);
				OLED_ShowChar(25, 0, '.', 8, 1);
				OLED_ShowNum(30, 0, dht_data.humidity_dec, 1, 8, 1);
				OLED_ShowChar(38, 0, '%', 8, 1);
				
				OLED_ShowNum(75, 0, dht_data.temp_int, 2, 8, 1);
				OLED_ShowChar(90, 0, '.', 8, 1);
				OLED_ShowNum(95, 0, dht_data.temp_dec, 1, 8, 1);
				OLED_ShowChar(103, 0, 'C', 8, 1);
				
				if(dht_data.humidity_int > humidity_threshold)
				{
					HAL_UART_Transmit(&huart2, (uint8_t*)"Humidity Too High\r\n", 19, 100);
					Stepper_Test(); // 步进电机转动30度
					HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET); // 点亮LED_R (低电平)
					humidity_over = 1;  // 标记已超阈值
				}
				else if(humidity_over)  // 湿度小于等于阈值且之前超过过
				{
					HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET); // 熄灭LED_R (高电平)
					humidity_over = 0;  // 清除标志，下次超阈值才能再次点亮
				}
				
				if(dht_data.temp_int > temp_threshold)
				{
					HAL_UART_Transmit(&huart2, (uint8_t*)"Temperature Too High\r\n", 22, 100);
					HAL_GPIO_WritePin(LAY_GPIO_Port, LAY_Pin, GPIO_PIN_SET); // 拉高LAY，驱动继电器
				}
				else
				{
					HAL_GPIO_WritePin(LAY_GPIO_Port, LAY_Pin, GPIO_PIN_RESET); // 拉低LAY，关闭继电器
				}
			}
			
			ADC_Read_All(&mq5_value, &noise_value, &light_value);
			
			OLED_ShowNum(30, 8, mq5_value, 4, 8, 1);
			OLED_ShowNum(40, 16, noise_value, 4, 8, 1);
			OLED_ShowNum(40, 24, light_value, 4, 8, 1);
			
			OLED_Refresh();
			
			if(noise_value > noise_threshold)
			{
				HAL_UART_Transmit(&huart2, (uint8_t*)"Noise High\r\n", 12, 100);
				HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET); // 点亮LED_B (低电平)
				noise_over = 1;  // 标记已超阈值
			}
			else if(noise_over)  // 噪音小于等于阈值且之前超过过
			{
				HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET); // 熄灭LED_B (高电平)
				noise_over = 0;  // 清除标志
			}
			
			if(mq5_value > mq5_threshold)
			{
				HAL_UART_Transmit(&huart2, (uint8_t*)"Air Quality Poor\r\n", 18, 100);
				HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET); // 点亮LED_G (低电平)
				mq5_over = 1;  // 标记已超阈值
			}
			else if(mq5_over)  // MQ5小于等于阈值且之前超过过
			{
				HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET); // 熄灭LED_G (高电平)
				mq5_over = 0;  // 清除标志
			}
			
			if(light_value < light_threshold)
			{
				HAL_UART_Transmit(&huart2, (uint8_t*)"Light Too Strong\r\n", 18, 100);
			}
		}
		
		// 检查步进电机标志
		if(stepper_flag)
		{
			Stepper_Test();
			stepper_flag = 0;
		}
		
		HAL_Delay(2000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
