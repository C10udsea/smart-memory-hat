//https://www.bilibili.com/video/BV1Dm4y1r71W 作者沫欢0x01
//配置wifi名字，密码，密钥三项就行
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#define BLINKER_WIFI
#define BLINKER_WITHOUT_SSL
//libraries里面的头文件
#include <Blinker.h>
//#include <ArduinoJson.h>
//esp32包自带头文件
#include "camera_pins.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include "esp_http_client.h" //图片上传


#define FLASH 4  //定义闪光灯的管脚,ESP32CAM自带的闪光灯低电平有效
#define LedCtr 33          //控制板载红色LED灯低电平有效
// define the number of bytes you want to access
#define EEPROM_SIZE 1

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
const char* uid = "YOUR_BEMFA_UID";  //用户私钥，巴法云控制台获取
const char* topic = "mypicture";                       //主题名字，可在控制台新建
const char* wechatMsg = "";                            //如果不为空，会推送到微信，可随意修改，修改为自己需要发送的消息
const char* wecomMsg = "";                             //如果不为空，会推送到企业微信，推送到企业微信的消息，可随意修改，修改为自己需要发送的消息
const char* urlPath = "";                              //如果不为空，会生成自定义图片链接，自定义图片上传后返回的图片url，url前一部分为巴法云域名，第二部分：私钥+主题名的md5值，第三部分为设置的图片链接值。
bool flashRequired = false;                            //闪光灯，true是打开闪光灯
const char* post_url = "http://images.bemfa.com/upload/v1/upimages.php";  // 默认上传地址
static String httpResponseString;                                         //接收服务器返回信息
bool internet_connected = false;
String url1;

int pictureNumber = 0;
camera_config_t config;

char auth[] = "YOUR_BLINKER_AUTH";     //去点灯里面获取 https://www.bilibili.com/video/BV1Dm4y1r71W
char ssid[] = "YOUR_WIFI_SSID";         //填写你的WiFi账号
char pswd[] = "YOUR_WIFI_PASSWORD";  //填写你的WiFi密码
void startCameraServer();
bool setup_camera = false;
bool ledstate=0;
bool oState = 0;         //开关标志位，记录开关状态
BlinkerButton Button1("LED");
BlinkerButton Button3("Photo");
BlinkerNumber Number1("rssi");
BlinkerSlider Slider1("Flash");       //点动定时滑块
BlinkerImage Image1("img");
BlinkerText Text1("txt");

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  pinMode(FLASH, OUTPUT);//设置为输出
  pinMode(LedCtr, OUTPUT);              //板载Led设置
  digitalWrite(LedCtr,HIGH);
  Blinker.begin(auth, ssid, pswd);
  Blinker.attachData(dataRead);
  Blinker.attachHeartbeat(heartbeat);
  BUILTIN_SWITCH.attach(button1_callback);
  Button1.attach(button1_callback);
  Button3.attach(button3_callback);
  Slider1.attach(slider1_callback);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.print("Initializing the camera module...");
  configInitCamera();
  Serial.println("Camera Ok!");
  WiFi.begin(ssid, pswd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  startCameraServer();
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  setupFlashPWM();  // configure PWM for the illumination LED
}

void loop() {
  
  Blinker.run();
  //delay(500);
}

void take_pictures(){
  Serial.print("Initializing the MicroSD card module... ");
  initMicroSDCard(); //这会使得闪光灯失控，除了重启没办法恢复控制
  camera_fb_t * fb = NULL;
  bool ok = 0; // Boolean indicating if the picture has been taken correctly
  do {
      // Take Picture with Camera
      fb = esp_camera_fb_get();  
      if(!fb) {
        Serial.println("Camera capture failed");
        return;
      }
      // initialize EEPROM with predefined size
      EEPROM.begin(EEPROM_SIZE);
      pictureNumber = EEPROM.read(0) + 1;
    
      // Path where new picture will be saved in SD Card
      String path = "/picture" + String(pictureNumber) +".jpg";
    
      fs::FS &fs = SD_MMC; 
      Serial.printf("Picture file name: %s\n", path.c_str());
      
      File file = fs.open(path.c_str(), FILE_WRITE);
      if(!file){
        Serial.println("Failed to open file in writing mode");
      } 
      else {
        file.write(fb->buf, fb->len); // payload (image), payload length
        Serial.printf("Saved file to path: %s , Size: %d bytes\n", path.c_str(),file.size());
      }
      file.close();
      esp_camera_fb_return(fb); 
      if (checkPhoto(fs,path)){
        ok=1;
        EEPROM.write(0, pictureNumber);
        EEPROM.commit();
        
      }
  }while(!ok);
  SD_MMC.end();
  Serial.println("finished!");
  
}


void configInitCamera() {
  //排线定义,不用改
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; //YUV422,GRAYSCALE,RGB565,JPEG
 
  // Select lower framesize if the camera doesn't support PSRAM
  if (psramFound()) {
    Serial.println("psram Found!");
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10; //10-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
 
  // Initialize the Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  //CMOS传感器配置,不配置就是黑漆漆一片
  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
  s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);            // 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);           // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 0);          // 0 = disable , 1 = enable
  s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
}
void initMicroSDCard() {
  // Start Micro SD card
  Serial.println("Starting SD Card");
  if (!SD_MMC.begin()) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD Card attached");
    return;
  }
}
// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ,String path) {
  File f_pic = fs.open(path);
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

const int ledFreq = 500;        // PWM settings
const int ledChannel = 15;     // camera uses timer1
const int ledRresolution = 8;  // resolution (8 = from 0 to 255)
void setupFlashPWM() {
  ledcSetup(ledChannel, ledFreq, ledRresolution);
  ledcAttachPin(FLASH, ledChannel);
  Flash(0);
}
                      // change illumination LED brightness
void Flash(byte ledBrightness) {
  ledcWrite(ledChannel, ledBrightness);  // change LED brightness (0 - 255)
  Serial.println("Brightness changed to " + String(ledBrightness));
}



/********http请求处理函数*********/
esp_err_t _http_event_handler(esp_http_client_event_t* evt) {
  if (evt->event_id == HTTP_EVENT_ON_DATA) {
    httpResponseString.concat((char*)evt->data);
  }
  return ESP_OK;
}

static esp_err_t take_send_photo() {
  Serial.println("take_send_photo...");
  camera_fb_t* fb = NULL;
  esp_err_t res = ESP_OK;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed...");
    return ESP_FAIL;
  }
  httpResponseString = "";
  esp_http_client_handle_t http_client;
  esp_http_client_config_t config_client = { 0 };
  config_client.url = post_url;
  config_client.event_handler = _http_event_handler;
  config_client.method = HTTP_METHOD_POST;
  http_client = esp_http_client_init(&config_client);
  esp_http_client_set_post_field(http_client, (const char*)fb->buf, fb->len);  //设置http发送的内容和长度
  esp_http_client_set_header(http_client, "Content-Type", "image/jpg");        //设置http头部字段
  esp_http_client_set_header(http_client, "Authorization", uid);               //设置http头部字段
  esp_http_client_set_header(http_client, "Authtopic", topic);                 //设置http头部字段
  esp_http_client_set_header(http_client, "wechatmsg", wechatMsg);             //设置http头部字段
  esp_http_client_set_header(http_client, "wecommsg", wecomMsg);               //设置http头部字段
  esp_http_client_set_header(http_client, "picpath", urlPath);                 //设置http头部字段
  esp_err_t err = esp_http_client_perform(http_client);                        //发送http请求
  if (err == ESP_OK) {
    Serial.println(httpResponseString);  //打印获取的URL
    //json数据解析
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, httpResponseString);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
    }
    String url = doc["url"];
    url1=url;
    Serial.println(url1);  //打印获取的URL
  }
  Serial.println("Taking picture END");
  esp_camera_fb_return(fb);
  esp_http_client_cleanup(http_client);
  return res;
}

void heartbeat()
{
  Number1.print(WiFi.RSSI());
  oState = !digitalRead(LedCtr);
  if (oState == 1)
  {
    BUILTIN_SWITCH.print("on");
    Button1.color("#B22222");
    Button1.print("on");
  }

  else if (oState == 0)
  {
    BUILTIN_SWITCH.print("off");
    Button1.color("#000000");
    Button1.print("off");
  }
}

void dataRead(const String & data)
{
    BLINKER_LOG("Blinker readString: ", data);

    Blinker.vibrate();
    
    uint32_t BlinkerTime = millis();
    
    Blinker.print("millis", BlinkerTime);
}

void ON()
{
  digitalWrite(LedCtr, LOW);
  Button1.color("#B22222");
  Button1.print("on");
  BUILTIN_SWITCH.print("on");
  oState = !digitalRead(LedCtr);
}
//关灯函数
void OFF()
{
  digitalWrite(LedCtr, HIGH);
  Button1.color("#000000");
  Button1.print("off");
  BUILTIN_SWITCH.print("off");
  oState = !digitalRead(LedCtr);  
}

void button1_callback(const String & state)                  //定义LED灯开关按钮
{
  BLINKER_LOG("get button state: ", state);

  if (state == "on")
    {
      ON();
    }
    else if (state == "off")
    {
      OFF();
    }
    else if (state == "press")
    {
      ;
    }
}

void button3_callback(const String & state)                  //定义拍照按钮
{
  BLINKER_LOG("get button state: ", state);
  Button3.print("on");
  //take_pictures();
  take_send_photo();
//  Image1[0].print(url1);//垃圾，不支持图片换url
  Text1.print(url1);
  Button3.print("off");
  Button3.text("ok");
}
//转16进制
String tohex(int n) {
char _16[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
if (n == 0) {
    return "00"; //n为0
}else if (n<16) {
    return '0' + String(_16[n]); //补零
}else{
    return String(_16[n/16])+String(_16[n%16]);
}
}

void slider1_callback(int32_t value)
{
  Flash(value);
  String x=tohex(value);
  String colordesc="#"+x+x+x;
  Serial.println(colordesc);
  Slider1.color(colordesc);
}
