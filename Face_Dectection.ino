#include <SD.h>
#include <SPI.h>

const int chipSelect = 5; // SD卡模块的CS引脚
std::vector<std::vector<float>> known_embeddings; // 用於存储多个特徵向量

void setup() {
  Serial.begin(115200);
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card is ready to use.");

  // 讀取特徵向量文件
  File file = SD.open("embeddings.dat", FILE_READ);
  if (file) {
    while (file.available()) {
      std::vector<float> embedding(128);
      for (int i = 0; i < 128; i++) {
        file.read((char*)&embedding[i], sizeof(float));
      }
      known_embeddings.push_back(embedding);
    }
    file.close();
    Serial.println("Face embedding vectors loaded from SD card.");
  } else {
    Serial.println("Failed to open face embedding file!");
  }
}

void loop() {
  if (Serial.available()) {
    // 讀取新特徵向量
    float new_embedding[128];
    for (int i = 0; i < 128; i++) {
      while (!Serial.available());
      Serial.readBytes((char*)&new_embedding[i], sizeof(float));
    }

    // 計算歐氏距离
    for (const auto& known_embedding : known_embeddings) {
      float distance = 0;
      for (int i = 0; i < 128; i++) {
        distance += (new_embedding[i] - known_embedding[i]) * (new_embedding[i] - known_embedding[i]);
      }
      distance = sqrt(distance);

      // 設置一个閾值进行判断
      float threshold = 0.6;
      if (distance < threshold) {
        Serial.println("Face recognized!");
        break;
      } else {
        Serial.println("Face not recognized.");
      }
    }
  }
}

