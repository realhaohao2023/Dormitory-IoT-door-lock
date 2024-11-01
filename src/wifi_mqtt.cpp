#include "wifi_mqtt.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <cJSON.h>

/* 线上环境域名和端口号 */
#define MQTT_SERVER "a1sejDZS1W1.iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define MQTT_PORT 1883 // 端口号
#define MQTT_USERNAME "esp-01s&a1sejDZS1W1"
#define CLIENT_ID "esp|securemode=3,signmethod=hmacsha1,timestamp=100|"
#define MQTT_PASSWORD "9F644999EB0E46F9462E38D8BA5507C6C15A70CC"

// 发布消息用的topic
#define PubTopic "/sys/a1sejDZS1W1/esp-01s/thing/event/property/post"
#define SubTopic "/sys/a1sejDZS1W1/esp-01s/thing/service/property/set"
WiFiClient espClient;
PubSubClient client(espClient);

// 声明数据上传的变量

int8_t lockstate_flag = 0; // 用于上传锁的状态 0为关闭 1为打开,由阿里云下发

//存储接收到的消息
char message[256];

//接收消息后的回调函数的回调函数释放信号量，在任务中进行解析
extern SemaphoreHandle_t xSemAliyun;

void Wifi_Connect()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin("虚空终端", "503503503");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the WiFi network");

    // 连接到MQTT服务器
    client.setServer(MQTT_SERVER, MQTT_PORT); /* 连接WiFi之后，连接MQTT服务器 */
    client.setCallback(mqttcallback);         /* 设置回调函数 */
}

// 连接到服务器
void mqttCheckConnect()
{
    while (!client.connected())
    {
        Serial.println("Connecting to MQTT Server...");
        if (client.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
        {
            Serial.println("MQTT Connected");
        }
        else
        {
            Serial.print("MQTT Connect err: ");
            Serial.println(client.state());
            delay(5000); 
        }
    }
}

void mqttcallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    //将接收到的消息转换为字符串，供void processJsonMessage(char *message)函数解析
    memcpy(message, payload, length);
    

    // 释放二值信号量
    xSemaphoreGive(xSemAliyun);
}

// 在一个进程中判断mqtt是否连接，如果没有连接则进行连接
void judgemqttconnect()
{
    if (!client.connected())
    {
        mqttCheckConnect();
    }
    client.loop();
}



//在按下外面的按键关门时，发布关门的消息,lockstate_flag为0
void publock()
{
    // 创建一个足够大的StaticJsonDocument对象来存储预期的JSON数据
    StaticJsonDocument<256> doc;

    // 设置JSON数据
    doc["id"] = "123";
    doc["version"] = "1.0";
    doc["sys"]["ack"] = 0;
    doc["params"]["lockstate"] = 0;
    doc["method"] = "thing.event.property.post";

    // 序列化JSON数据为字符串
    char jsonOutput[256];
    serializeJson(doc, jsonOutput);
    //打印json字符串
    Serial.println(jsonOutput);

    // 使用修改后的pubMsg函数发送JSON字符串
    if (client.publish(PubTopic, jsonOutput))
    {
        Serial.println("Publish success");
        // 连接成功后订阅主题
        client.subscribe(SubTopic);
    }
    else
    {
        Serial.println("Publish fail");
    }
}

// 订阅阿里云下发的信息
void processJsonMessage(char *message)
{

    StaticJsonDocument<256> doc; // 调整大小以适应您的需要
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // 从JSON中提取信息
    const char *method = doc["method"];         // "thing.service.property.set"
    const char *id = doc["id"];                 // "248166957"
    int lockstate = doc["params"]["lockstate"]; // 1 or 0
    const char *version = doc["version"];       // "1.0.0"

    Serial.print("Method: ");
    Serial.println(method);
    Serial.print("ID: ");
    Serial.println(id);
    Serial.print("lockstate: ");
    Serial.println(lockstate);
    Serial.print("Version: ");
    Serial.println(version);

    // 根据lockstate的值执行操作
    if (lockstate == 1)
    {
        // 如果lockstate为1，执行相应操作
        // digitalWrite(LED_PIN, HIGH); // 示例：打开LED
        lockstate_flag = 1;
        Serial.println("door opened");
    }
    else
    {
        // 如果lockstate为0，执行相应操作
        // digitalWrite(LED_PIN, LOW); // 示例：关闭LED
        lockstate_flag = 0;
        Serial.println("door closed");
    }
}

