#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNFaceDetection.h"
#include "VideoStreamOverlay.h"
#include <SD.h>
#include <SPI.h>

#define CHANNEL     0
#define CHANNELNN   3

#define NNWIDTH     576
#define NNHEIGHT    320

VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
NNFaceDetection facedet;
RTSP rtsp;
StreamIO videoStreamer(1, 1);
StreamIO videoStreamerNN(1, 1);

char ssid[] = "HITRON-DF90-5G";   // your network SSID (name)
char pass[] = "0972211921";       // your network password
int status = WL_IDLE_STATUS;

IPAddress ip;
int rtsp_portnum; 

const int chipSelect = 4; // SD card CS pin

struct KnownFace {
    String name;
    std::vector<float> features;
};

std::vector<KnownFace> knownFaces;

bool loadKnownFaces(const char* directory) {
    File dir = SD.open(directory);
    if (!dir) {
        Serial.println("[Error] Failed to open directory.");
        return false;
    }

    while (true) {
        File file = dir.openNextFile();
        if (!file) break;

        if (!file.isDirectory()) {
            KnownFace face;
            face.name = String(file.name());
            while (file.available()) {
                float value;
                file.read((char*)&value, sizeof(float));
                face.features.push_back(value);
            }
            knownFaces.push_back(face);
        }
        file.close();
    }
    dir.close();
    return true;
}

float calculateEuclideanDistance(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        sum += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return sqrt(sum);
}

void setup() {
    Serial.begin(115200);

    // Initialize SD card
    if (!SD.begin(chipSelect)) {
        Serial.println("[Error] SD card initialization failed!");
        return;
    }
    Serial.println("SD card is ready to use.");

    // Load known faces from SD card
    if (!loadKnownFaces("/faces")) {
        Serial.println("[Error] Failed to load known faces.");
        return;
    }
    Serial.print("Loaded ");
    Serial.print(knownFaces.size());
    Serial.println(" known faces.");

    // Attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }
    ip = WiFi.localIP();

    // Configure camera video channels with video format information
    config.setBitrate(2 * 1024 * 1024); // Recommend to use 2Mbps for RTSP streaming to prevent network congestion
    Camera.configVideoChannel(CHANNEL, config);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();

    // Configure RTSP with corresponding video format information
    rtsp.configVideo(config);
    rtsp.begin();
    rtsp_portnum = rtsp.getPort();

    // Configure face detection with corresponding video format information
    facedet.configVideo(configNN);
    facedet.setResultCallback(FDPostProcess);
    facedet.modelSelect(FACE_DETECTION, NA_MODEL, DEFAULT_SCRFD, NA_MODEL);
    facedet.begin();

    // Configure StreamIO object to stream data from video channel to RTSP
    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    // Start data stream from video channel
    Camera.channelBegin(CHANNEL);

    // Configure StreamIO object to stream data from RGB video channel to face detection
    videoStreamerNN.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerNN.setStackSize();
    videoStreamerNN.setTaskPriority();
    videoStreamerNN.registerOutput(facedet);
    if (videoStreamerNN.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    // Start video channel for NN
    Camera.channelBegin(CHANNELNN);

    // Start OSD drawing on RTSP video channel
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
}

void loop() {
    // Do nothing
}

// User callback function for post processing of face detection results
void FDPostProcess(std::vector<FaceDetectionResult> results) {
    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");

    printf("Total number of faces detected = %d\r\n", facedet.getResultCount());
    OSD.createBitmap(CHANNEL);

    if (facedet.getResultCount() > 0) {
        for (uint32_t i = 0; i < facedet.getResultCount(); i++) {
            FaceDetectionResult item = results[i];
            // Result coordinates are floats ranging from 0.00 to 1.00
            // Multiply with RTSP resolution to get coordinates in pixels
            int xmin = (int)(item.xMin() * im_w);
            int xmax = (int)(item.xMax() * im_w);
            int ymin = (int)(item.yMin() * im_h);
            int ymax = (int)(item.yMax() * im_h);

            // Draw boundary box
            printf("Face %d confidence %d:\t%d %d %d %d\n\r", i, item.score(), xmin, xmax, ymin, ymax);
            OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, OSD_COLOR_WHITE);

            // Get detected face feature vector
            std::vector<float> detectedFeatures;
            for (int j = 0; j < item.featureCount(); j++) {
                detectedFeatures.push_back(item.feature(j));
            }

            // Compare detected face with known faces
            String closestName = "Unknown";
            float minDistance = FLT_MAX;
            for (const auto& knownFace : knownFaces) {
                float distance = calculateEuclideanDistance(knownFace.features, detectedFeatures);
                if (distance < minDistance) {
                    minDistance = distance;
                    closestName = knownFace.name;
                }
            }

            // Print identification text above boundary box
            char text_str[40];
            snprintf(text_str, sizeof(text_str), "%s %d", closestName.c_str(), item.score());
            OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, OSD_COLOR_CYAN);

            // Draw facial feature points
            for (int j = 0; j < 5; j++) {
                int x = (int)(item.xFeature(j) * im_w);
                int y = (int)(item.yFeature(j) * im_h);
                OSD.drawPoint(CHANNEL, x, y, 8, OSD_COLOR_RED);
            }
        }
    }
    OSD.update(CHANNEL);
}
