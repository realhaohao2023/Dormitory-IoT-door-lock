#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <PubSubClient.h> //PubSubClient 作者 Nick O’Leary
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h> // 引入ArduinoJson库
#include "wifi_mqtt.h"

#define USE_MULTCORE 1 // 使用多核心

hw_timer_t *timer = NULL; // 定义一个定时器

// 创建一个舵机对象
Servo servo;

// 创建控制舵机运动的二值信号量的句柄
SemaphoreHandle_t xSemServo = NULL;
// 创建进行阿里云下发信息解析的二值信号量的句柄
SemaphoreHandle_t xSemAliyun = NULL;

// 全局变量声明


extern int8_t lockstate_flag;
extern char message[256];

void IRAM_ATTR TimerEvent()
{
}



void task1(void *pvParameters)
{
  while (1)
  {
    // 等待二值信号量
    if (xSemaphoreTake(xSemServo, portMAX_DELAY) == pdTRUE)
    {
      Serial.println("task1");
      // 判断是否可以开门
      if (lockstate_flag == 1)
      {
        servo.write(75);
        vTaskDelay(2000 / portTICK_PERIOD_MS); // 2s
        servo.write(0);
      }
      else
      {
        Serial.println("开门失败");
        //指示灯连闪三次，表示开门失败，闪完后灭
        for (int i = 0; i < 3; i++)
        {
          digitalWrite(5, HIGH);
          vTaskDelay(200 / portTICK_PERIOD_MS); // 500ms
          digitalWrite(5, LOW);
          vTaskDelay(200 / portTICK_PERIOD_MS); // 500ms
        }
      }

      Serial.println("xSemServo has been taken");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
  }
}

void task2(void *pvParameters)
{
  while (1)
  {
    judgemqttconnect();


  }
}

void task3(void *pvParameters)
{
  while (1)
  {
    // 读取按键状态
    if (digitalRead(4) == LOW)
    {
      // 软件消抖
      vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms
      if (digitalRead(4) == LOW)
      {
        // 每次按键按下，释放控制舵机的二值信号量
        xSemaphoreGive(xSemServo);
        // 打印 提示释放了二值信号量
        Serial.println("xSemServo has been released");
        vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
      }
    }

    if (digitalRead(8) == LOW)
    {
      // 软件消抖
      vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms
      if (digitalRead(8) == LOW)
      {
        lockstate_flag = 0;
        printf("lockstate_flag = %d\n", lockstate_flag);
        publock();
        //指示灯灭
        digitalWrite(5, LOW);
        vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
  }
}


//创建进行信息解析的任务
void task4(void *pvParameters)
{
  while (1)
  {
    //等待二值信号量
    if (xSemaphoreTake(xSemAliyun, portMAX_DELAY) == pdTRUE)
    {
      Serial.println("task4");

      //解析信息
      processJsonMessage(message);

      //根据当前lockstate_flag的值，确定指示灯的状态
      if (lockstate_flag == 1)
      {
        digitalWrite(5, HIGH);
      }
      else
      {
        digitalWrite(5, LOW);
      }

      Serial.println("xSemServo has been taken");
      vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
    }
    vTaskDelay(500 / portTICK_PERIOD_MS); // 500ms
  }
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  // 初始化定时器中断
  timer = timerBegin(0, 80, true);                // 初始化定时器，80分频，计数器向上计数 1us
  timerAttachInterrupt(timer, &TimerEvent, true); // 绑定定时器的中断处理函数
  timerAlarmWrite(timer, 1000000, true);          // 1s
  timerAlarmEnable(timer);                        //  使能定时器

  // 初始化按键，默认上拉输入，在进程中读取按键状态
  pinMode(8, INPUT_PULLUP); // 这个按键用于开门
  pinMode(4, INPUT_PULLUP); // 这个按键用于锁门

  //io5作为指示灯，用于在外面提示门锁是否打开
  pinMode(5, OUTPUT);
  //初值为低电平
  digitalWrite(5, LOW);

  // 创建二值信号量，用于外部中断控制舵机任务
  xSemServo = xSemaphoreCreateBinary();
  // 创建二值信号量，用于阿里云下发信息解析任务
  xSemAliyun = xSemaphoreCreateBinary();

  // 指定舵机引脚
  servo.attach(9);

  // 连接WiFi
  Wifi_Connect();

  // 连接MQTT服务器
  mqttCheckConnect();

#if !USE_MULTCORE // 如果不使用多核，使用rtos创建任务
  xTaskCreate(task1, "task1", 2048, NULL, 2, NULL);
  xTaskCreate(task2, "task2", 2048, NULL, 1, NULL);//检测网络连接的任务，优先级最低
  // 创建任务，用于循环读取按键状态
  xTaskCreate(task3, "task3", 2048, NULL, 3, NULL);//检测按键状态的任务，优先级最高

#else // 使用多核
  xTaskCreatePinnedToCore(task1, "task1", 2048, NULL, 2, NULL, 0); // 0号核心
  xTaskCreatePinnedToCore(task2, "task2", 1024, NULL, 1, NULL, 1); // 1号核心
  xTaskCreatePinnedToCore(task3, "task3", 2048, NULL, 3, NULL, 0); //0号核心 检测按键状态的任务，优先级最高
  //创建进行信息解析的任务
  xTaskCreatePinnedToCore(task4, "task4", 1024, NULL, 1, NULL, 0);

#endif
}

void loop()
{
  // put your main code here, to run repeatedly:
}



