/* freertos.c */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "main.h"         // pulls in stm32f4xx_hal.h and GPIO_PIN_x/GPIOB defs
#include "ssd1306.h"      // SSD1306_COLOR_BLACK, SSD1306_* APIs
#include "fonts.h"        // Font_11x18, Font_16x26
#include <stdio.h>        // snprintf
#include <string.h>       // memcpy, strncmp
#include <ctype.h>        // isdigit
#include "usbd_cdc_if.h"

#define SEGMENT_COUNT 7
#define DIGIT_COUNT   2

static const uint16_t segmentPins[SEGMENT_COUNT] = {
  GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10,
  GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14
};

static const uint16_t digitPins[DIGIT_COUNT] = {
  GPIO_PIN_0, GPIO_PIN_1
};

static const uint8_t digitPatterns[10][SEGMENT_COUNT] = {
  {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1},
  {1,1,1,1,0,0,1}, {0,1,1,0,0,1,1}, {1,0,1,1,0,1,1},
  {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0}, {1,1,1,1,1,1,1},
  {1,1,1,1,0,1,1}
};

void displayDigit(uint8_t digit, uint8_t position)
{
  for (uint8_t i = 0; i < SEGMENT_COUNT; ++i) {
    HAL_GPIO_WritePin(GPIOB, segmentPins[i], GPIO_PIN_RESET);
  }
  for (uint8_t i = 0; i < DIGIT_COUNT; ++i) {
    HAL_GPIO_WritePin(GPIOB, digitPins[i], GPIO_PIN_RESET);
  }
  for (uint8_t i = 0; i < SEGMENT_COUNT; ++i) {
    HAL_GPIO_WritePin(GPIOB, segmentPins[i],
                      digitPatterns[digit][i] ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
  HAL_GPIO_WritePin(GPIOB, digitPins[position], GPIO_PIN_SET);
}

extern void CountdownTask(void *argument);
extern void DisplayTask(void *argument);
extern void displayDigit(uint8_t digit, uint8_t position);

void CountdownTask(void *argument);
void DisplayTask  (void *argument);

/* semafor ve mutex handle’ları (main.h ya da freertos.h içinde extern tanımlı olmalı) */
osSemaphoreId_t startSem;
osSemaphoreId_t finishSem;
osSemaphoreId_t wordCountSem;
osMutexId_t     i2cMutex;

extern uint32_t trueWordCount;
extern char CDC_Buffer[64];

void DisplayTask(void *argument) {
  (void)argument;
  char buf[24];
  for (;;) {
    /* geri sayım bitti semaforünü bekle */
    osSemaphoreAcquire(finishSem, osWaitForever);
    char numStr[12];                                         // 4 294 967 295 + '\0'
    snprintf(numStr, sizeof(numStr), "%lu", (unsigned long)trueWordCount);
    /* doğru kelime sayısını oku */
    //uint32_t total = osSemaphoreGetCount(finishCountSem);

    /* ekrana yaz */
    /* trueWordCount = uint32_t  → %lu veya %u  */
    printf("%lu\r\n", (unsigned long)trueWordCount);
    snprintf(buf, sizeof(buf), "True Words: %u", (unsigned int)trueWordCount);
    osMutexAcquire(i2cMutex, osWaitForever);
    SSD1306_Fill(SSD1306_COLOR_BLACK);
    SSD1306_GotoXY(0,0);
    SSD1306_Puts(buf, &Font_11x18, 1);
    /* --- 2) Sayı  (alt satır, ortalanmış) --------------------------- */
    uint8_t digits = strlen(numStr);                         // kaç rakam?
    uint8_t pxWidth = digits * 16;                           // 16x26 → 16 px genişlik
    uint8_t startX  = (128 - pxWidth) / 2;                   // yatay merkezle

    SSD1306_GotoXY(startX, 26);                              // y = 18+4 padding ≈ 26
    SSD1306_Puts(numStr, &Font_16x26, 1);
    SSD1306_UpdateScreen();
    osMutexRelease(i2cMutex);
  }
}

void CountdownTask(void *argument) {
  (void)argument;
  for (;;) {
    /* “begin” komutu gelene kadar bekle */
    osSemaphoreAcquire(startSem, osWaitForever);

    /* OLED’e “Start!” yaz */
    osMutexAcquire(i2cMutex, osWaitForever);
    SSD1306_Init();
    SSD1306_Fill(SSD1306_COLOR_BLACK);
    SSD1306_GotoXY(20,20);
    SSD1306_Puts("Start!", &Font_16x26, 1);
    SSD1306_UpdateScreen();
    osMutexRelease(i2cMutex);

    /* 30’dan 0’a geri sayım */
    for (int cnt = 30; cnt >= 0; --cnt) {
      uint8_t tens = cnt / 10;
      uint8_t ones = cnt % 10;
      /* multiplex display için kısmi beklemelerle */
      for (int i = 0; i < 50; ++i) {
        osMutexAcquire(i2cMutex, osWaitForever);
        displayDigit(tens, 0);
        osDelay(10);
        displayDigit(ones, 1);
        osDelay(10);
        osMutexRelease(i2cMutex);

      }
    }

    /* geri sayım bitti, DisplayTask’a haber ver */
    osSemaphoreRelease(finishSem);
  }
}

void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Create the semaphores */
  startSem = osSemaphoreNew(
      1,    /* max count */
      0,    /* initial count = 0, bekletecek */
      NULL  /* attr */);
  finishSem = osSemaphoreNew(1, 0, NULL);
  wordCountSem = osSemaphoreNew(
      100,  /* max count = en fazla 100 “give”’i tutabilir */
      0,    /* initial = 0 */
      NULL);
  /* Create the mutex for I²C/OLED/7-seg */
  i2cMutex = osMutexNew(NULL);

  /* Create the threads */
  const osThreadAttr_t cntdAttrs = {
      .name = "CntdTask",
      .priority = (osPriority_t) osPriorityNormal,
      .stack_size = 512
  };
  osThreadNew(CountdownTask, NULL, &cntdAttrs);

  const osThreadAttr_t dispAttrs = {
      .name = "DispTask",
      .priority = (osPriority_t) osPriorityLow,
      .stack_size = 512
  };
  osThreadNew(DisplayTask, NULL, &dispAttrs);

  /* USER CODE BEGIN RTOS_THREADS */
  /* burada ek görevler ekleyebilirsiniz */
  /* USER CODE END RTOS_THREADS */
}
