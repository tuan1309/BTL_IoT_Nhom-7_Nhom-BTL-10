#include <FirebaseESP8266.h>  
#include <ESP8266WiFi.h>  
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h> 

// Cấu hình WiFi
const char* ssid = "......";
const char* password = "13092003";
const char* mqtt_server = "8c5ca82a1e5f4d4294bf605318a3278d.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_topic = "home/security/pir";
const char* mqtt_username = "tuan2003"; 
const char* mqtt_password = "1234567T@m";
const char* FIREBASE_HOST = "hhethongchongtrom-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "3HpPWJpF1IqwVqB4RiYZ7YsJjEQcFgF9Qa1VA5aJ"; 
const int buzzerPin = D2; 
const int pirPin = D1;
bool alarmEnabled = false;
bool sensorEnabled = false;  
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); 
WiFiClientSecure espClient; 
PubSubClient client(espClient);

unsigned long lastTime = 0;
unsigned long timerDelay = 8000; 

void setup() {
    Serial.begin(115200);
    Serial.println("Khởi động...");

    pinMode(buzzerPin, OUTPUT);
    pinMode(pirPin, INPUT);
    digitalWrite(buzzerPin, LOW);

    WiFi.begin(ssid, password);
    Serial.print("Đang kết nối WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        yield(); // Cho phép các tác vụ khác thực hiện
        Serial.print(".");
    }
    Serial.println("\nĐã kết nối WiFi!");

    connectToFirebase();
    connectToMQTT();

    timeClient.begin();
    timeClient.update();
}

void connectToFirebase() {
    firebaseConfig.host = FIREBASE_HOST;
    firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    Firebase.reconnectWiFi(true);
}

void connectToMQTT() {
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    espClient.setInsecure();
    reconnect();
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Nhận tin nhắn trên topic: ");
    Serial.println(topic);
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("Tin nhắn nhận được: " + message);

    if (message == "ON") {
        sensorEnabled = true;
        alarmEnabled = true;
        Serial.println("Cảm biến PIR đã được bật.");
    } else if (message == "OFF") {
        sensorEnabled = false;
        alarmEnabled = false;
        digitalWrite(buzzerPin, LOW);
        Serial.println("Cảm biến PIR đã được tắt.");
    }
}

void saveAlertHistory(int status) {
    String path = "/hethongbaotrom/history";
    if (status == HIGH) {
        if (!timeClient.update()) {
            timeClient.forceUpdate();
        }
        String formattedTime = timeClient.getFormattedTime();
        String dataPath = path + "/" + formattedTime;
        FirebaseJson alertData;
        alertData.add("timestamp", formattedTime);
        alertData.add("status", "Cảnh báo!");
        alertData.add("message", "Phát hiện chuyển động!");

        if (Firebase.setJSON(firebaseData, dataPath, alertData)) {
            Serial.println("Cảnh báo đã được lưu vào Firebase.");
        } else {
            Serial.println("Lỗi khi lưu cảnh báo: " + firebaseData.errorReason());
        }
    }
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Đang kết nối MQTT...");
        if (client.connect("ESP8266Client", mqtt_username, mqtt_password)) {
            Serial.println("Đã kết nối tới MQTT!");
            client.subscribe(mqtt_topic);
        } else {
            Serial.print("Kết nối MQTT thất bại, lý do: ");
            Serial.println(client.state());
            delay(5000);
        }
    }
}

void updateFirebase(int pirState) {
    if (Firebase.setInt(firebaseData, "/hethongbaotrom/cb", pirState)) {
        Serial.println("Cập nhật trạng thái cảm biến thành công lên Firebase.");
    } else {
        Serial.println("Cập nhật trạng thái cảm biến thất bại lên Firebase. Lý do: " + firebaseData.errorReason());
    }
}

bool motionDetectedLastTime = false;

void loop() {
    unsigned long currentTime = millis();
    if (currentTime - lastTime >= timerDelay) {
        lastTime = currentTime;

        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        if (sensorEnabled) {
            int pirState = digitalRead(pirPin);
            Serial.print("PIR state: ");
            Serial.println(pirState);

            static int lastPirState = LOW; // Biến lưu trạng thái PIR trước đó
            if (pirState != lastPirState) { // Chỉ cập nhật khi có thay đổi
                updateFirebase(pirState);
                if (pirState == HIGH) {
                    Serial.println("Motion detected!");
                    if (alarmEnabled) {
                        digitalWrite(buzzerPin, HIGH);
                    }
                    saveAlertHistory(HIGH);
                } else {
                    Serial.println("No motion detected");
                    digitalWrite(buzzerPin, LOW);
                }
                lastPirState = pirState; // Cập nhật trạng thái PIR trước đó
            }
        } else {
            digitalWrite(buzzerPin, LOW);
        }
    }
}