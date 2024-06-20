#include <Arduino.h>
#include "WiFi.h"
#include "VideoStream.h"
#include "StreamIO.h"
#include "RTSP.h"
#include "NNFaceDetectionRecognition.h"
#include "VideoStreamOverlay.h"
#include "AmebaFatFS.h"
#include "Base64.h"

// 用戶配置
#define CHANNEL   0                         // 影音流和快照的通道
#define CHANNELNN 3                         // 僅在臉部辨識通道3上提供的RGB格式影音
#define FILENAME  "image.jpg"               // 保存為SD卡上的jpg圖片
#define VIDEO_FILENAME "video.mp4"          // 保存為SD卡上的mp4影片

// 輸入您的Google Script和Line Notify詳細訊息
String myScript = "https://script.google.com/macros/s/AKfycbxADuxUreZnrB5_0JEkr9j_94QvyhGlU7xKOwo7H2MjJ0tIMixrALXS8bcJQ7dAqk-PeQ/exec";    // 创建您的Google Apps Script并替换“myScript”路径。
String myFoldername = "&myFoldername=hi";                                       // 設置Google雲端硬碟文件夾名稱以儲存文件
String myFilename = "&myFilename=image.jpg";                                    // 設置要儲存數據的Google雲端硬碟文件名
String myImage = "&myFile=";

char ssid[] = "鴨";               // 您的網路SSID（名稱）
char pass[] = "12345678";         // 您的網路密碼
int status = WL_IDLE_STATUS;

uint32_t img_addr = 0;
uint32_t img_len = 0;

uint32_t video_start_time = 0;
bool recording_video = false;
bool flag_face_detected = false;

// 創建對象
VideoSetting config(VIDEO_D1, CAM_FPS, VIDEO_H264_JPEG, 1);    // 高分辨率影音流
VideoSetting configNN(VIDEO_VGA, 10, VIDEO_RGB, 0);            // 低分辨率RGB影音用於人臉辨識
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);
NNFaceDetectionRecognition facerecog;
AmebaFatFS fs;
WiFiSSLClient wifiClient;

char buf[512];
char *p;


void FRPostProcess(std::vector<FaceRecognitionResult> results)
{
    // 人臉辨識結果以數組表示
    // 迭代所有元素以檢查以辨識的臉部
    OSD.createBitmap(CHANNEL);
    if (facerecog.getResultCount() > 0) {
        for (uint16_t i = 0; i < facerecog.getResultCount(); i++) {
            FaceRecognitionResult result = results[i];
            int xmin = (int)(result.xMin() * config.width());
            int xmax = (int)(result.xMax() * config.width());
            int ymin = (int)(result.yMin() * config.height());
            int ymax = (int)(result.yMax() * config.height());
            uint32_t osd_color;
            if (String(result.name()) == String("unknown")) {
                osd_color = OSD_COLOR_RED;
                flag_face_detected = true;      // 偵測到未註冊的人臉
            } else {
                osd_color = OSD_COLOR_GREEN;
            }
            OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, osd_color);
            char text_str[40];
            snprintf(text_str, sizeof(text_str), "Face:%s", result.name());
            OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, osd_color);
        }
    } else {
        flag_face_detected = false;
    }
    OSD.update(CHANNEL);
    delay(100);
}

void setup()
{
    Serial.begin(115200);

    // 嘗試連接到WiFi網路：
    while (status != WL_CONNECTED) {
        Serial.print("嘗試連接到WPA SSID：");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);
        delay(2000);
    }
    WiFiCon();

    // 為所需分辨率和格式配置鏡頭通道
    // 根據WiFi網路品質調整位元率
    Camera.configVideoChannel(CHANNEL, config);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();

    // 配置RTSP以獲得高分辨率影音訊息
    rtsp.configVideo(config);
    rtsp.begin();

    // 為低分辨率RGB影音配置人臉辨識
    facerecog.configVideo(configNN);
    facerecog.modelSelect(FACE_RECOGNITION, NA_MODEL, DEFAULT_SCRFD, DEFAULT_MOBILEFACENET);
    facerecog.begin();
    facerecog.setResultCallback(FRPostProcess);

    // 配置StreamIO對象以將數據從高分辨率影音通道傳送到RTSP
    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO連結啟動失敗");
    }

    // 啟用高分辨率影音通道的數據端
    Camera.channelBegin(CHANNEL);
    Serial.println("影音RTSP開始");

    // 配置StreamIO對象已將數據從低分辨率影音通道傳送到人臉辨識
    videoStreamerNN.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerNN.setStackSize();
    videoStreamerNN.registerOutput(facerecog);
    if (videoStreamerNN.begin() != 0) {
        Serial.println("StreamIO連結啟動失敗");
    }

    // 啟用低分辨率影音通道的數據端
    Camera.channelBegin(CHANNELNN);

    // 配置OSD以在高分辨率影音上繪圖
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
    Serial.println("");
    Serial.println("================================");
    Serial.println("人臉辨識");
    Serial.println("================================");
    delay(2000);
}

void loop()
{
    if (flag_face_detected && !recording_video) {
        Serial.println("偵測到人臉");
        recording_video = true;
        video_start_time = millis();

        // 開始錄製影片
        fs.begin();
        String filepath = String(fs.getRootPath()) + String(VIDEO_FILENAME);
        File file = 

        if (!fs.begin()) {
            StreamEnd();
            pinMode(LED_B, OUTPUT);
            digitalWrite(LED_B, HIGH);
            Serial.println("");
            Serial.println("================================");
            Serial.println("[錯誤] SD卡加載失敗!!!");
            Serial.println("================================");
            while (1)
                ;
        }

        // 列出目錄===並將结果放入buf中
        memset(buf, 0, sizeof(buf));
        fs.readDir(fs.getRootPath(), buf, sizeof(buf));
        String filepath = String(fs.getRootPath()) + String(FILENAME);
        File file = fs.open(filepath);
        if (!file) {
            Serial.println("");
            Serial.println("================================");
            Serial.println("[錯誤] 打開文件失敗");
            Serial.println("================================");
            fs.end();
        }
        delay(100);

        // 拍照
        CamFlash();
        Camera.getImage(CHANNEL, &img_addr, &img_len);
        file.write((uint8_t *)img_addr, img_len);
        file.close();

        Serial.println("===================================");
        Serial.println("[訊息] 照片已捕獲...");
        Serial.println("===================================");

        // 文件處理
        p = buf;
        while (strlen(p) > 0) {
            if (strstr(p, FILENAME) != NULL) {
                Serial.println("[訊息] 在字串中找到'image.jpg'。");
                Serial.println("[訊息] 正在處理文件...");
            }
            p += strlen(p) + 1;
        }

        uint8_t *fileinput;
        file = fs.open(filepath);
        unsigned int fileSize = file.size();
        fileinput = (uint8_t *)malloc(fileSize + 1);
        file.read(fileinput, fileSize);
        fileinput[fileSize] = '\0';
        file.close();
        fs.end();

        char *input = (char *)fileinput;
        String imageFile = "data:image/jpeg;base32,";
        char output[base64_enc_len(3)];
        for (unsigned int i = 0; i < fileSize; i++) {
            base64_encode(output, (input++), 3);
            if (i % 3 == 0) {
                imageFile += urlencode(String(output));
            }
        }

        // 將文件傳送到Google雲端硬碟
        Serial.println("[訊息] 正在上傳文件到Google雲端硬碟...");
        String Data = myFoldername + myFilename + myImage;
        const char *myDomain = "script.google.com";
        String getAll = "", getBody = "";
        Serial.println("[訊息] 連接到 " + String(myDomain));

        if (wifiClient.connect(myDomain, 443)) {
            Serial.println("[訊息] 連接成功");

            wifiClient.println("POST " + myScript + " HTTP/1.1");
            wifiClient.println("Host: " + String(myDomain));
            wifiClient.println("Content-Length: " + String(Data.length() + imageFile.length()));
            wifiClient.println("Content-Type: application/x-www-form-urlencoded");
            wifiClient.println("Connection: keep-alive");
            wifiClient.println();

            wifiClient.print(Data);
            for (unsigned int Index = 0; Index < imageFile.length(); Index = Index + 1000) {
                wifiClient.print(imageFile.substring(Index, Index + 1000));
            }

            int waitTime = 10000;    // timeout 10 seconds
            unsigned int startTime = millis();
            boolean state = false;

            while ((startTime + waitTime) > millis()) {
                delay(100);
                while (wifiClient.available()) {
                    char c = wifiClient.read();
                    if (state == true) {
                        getBody += String(c);
                    }
                    if (c == '\n') {
                        if (getAll.length() == 0) {
                            state = true;
                        }
                        getAll = "";
                    } else if (c != '\r') {
                        getAll += String(c);
                    }
                    startTime = millis();
                }
                if (getBody.length() > 0) {
                    break;
                }
            }
            wifiClient.stop();
            Serial.println(getBody);
        } else {
            getBody = "連接到 " + String(myDomain) + " 失敗。";
            Serial.println("[訊息] 連接到 " + String(myDomain) + " 失敗。");
        }
        Serial.println("[訊息] 文件上傳完成。");
        Serial.println("===================================");
    } else {    // 未偵測到人臉
        Serial.print(".");
    }
}

// https://www.arduino.cc/reference/en/libraries/urlencode/
String urlencode(String str)
{
    const char *msg = str.c_str();
    const char *hex = "0123456789ABCDEF";
    String encodedMsg = "";
    while (*msg != '\0') {
        if (('a' <= *msg && *msg <= 'z') || ('A' <= *msg && *msg <= 'Z') || ('0' <= *msg && *msg <= '9') || *msg == '-' || *msg == '_' || *msg == '.' || *msg == '~') {
            encodedMsg += *msg;
        } else {
            encodedMsg += '%';
            encodedMsg += hex[(unsigned char)*msg >> 4];
            encodedMsg += hex[*msg & 0xf];
        }
        msg++;
    }
    return encodedMsg;
}

void CamFlash()
{
    pinMode(LED_G, OUTPUT);
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_G, HIGH);
        delay(100);
        digitalWrite(LED_G, LOW);
        delay(100);
    }
}

void WiFiCon()
{
    pinMode(LED_B, OUTPUT);
    for (int i = 0; i < 2; i++) {
        digitalWrite(LED_B, HIGH);
        delay(300);
        digitalWrite(LED_B, LOW);
        delay(300);
    }
}

void StreamEnd()
{
    videoStreamer.pause();    // 暫停連接
    videoStreamerNN.pause();
    rtsp.end();              // 停止RTSP通道/模組
    Camera.channelEnd();     // 停止攝影機通道/模組
    facerecog.end();         // 關閉模組
    Camera.videoDeinit();    // 影音去初始化
    delay(1000);
}
