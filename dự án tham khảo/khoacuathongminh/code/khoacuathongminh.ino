#include <WiFi.h>
#include <FirebaseESP32.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <ESP32Servo.h>

// --- Cấu hình WiFi & Firebase ---
#define WIFI_SSID "Nhat Duy"
#define WIFI_PASSWORD "nhatduy12345"
#define DATABASE_URL "https://khoacuathongminh-6ffd4-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "U5XENgrL1wE2msoforE0UOCp3lk8rJX4YSMUfHpS"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- LCD I2C ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Bàn phím 4x4 ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Cảm biến vân tay AS608 ---
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// --- Servo ---
Servo myServo;
#define SERVO_PIN 4

// --- Biến điều khiển ---
String inputPassword = "";
int mode = 0; // 0: Menu, 1: Nhập pass, 2: Vân tay

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Ket noi thanh cong toi cam bien");
  } else {
    Serial.println("Loi cam bien");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Da ket noi WiFi");

  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  unsigned long startTime = millis();
  while (!Firebase.ready()) {
    if (millis() - startTime > 10000) {  // 10 giây timeout
      Serial.println("Ket noi toi firebase khong thanh cong");
      break;
    }
    delay(500);
    Serial.print(".");
  }

  if (Firebase.ready()) {
    Serial.println("\nKet noi toi firebase thanh cong!");
  } else {
    Serial.println("\Khong the ket noi toi Firebase.");
  }
  lcd.init();
  lcd.backlight();

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  showMainMenu();
}

void loop() {
    char key = keypad.getKey();
    if (key) {
        if (mode == 0) {
            if (key == '1') {
                inputPassword = "";
                lcd.clear();
                lcd.print("Nhap mat khau:");
                mode = 1;
            } else if (key == '2') {
                lcd.clear();
                lcd.print("Dang quet van tay");
                verifyFingerprint();
            } else if (key == '3') {
                registerFingerprint();
            }
        } else if (mode == 1) {
            if (key == '#') {
                checkPassword(inputPassword);
            } else {
                inputPassword += key;
                lcd.setCursor(0, 1);
                lcd.print(inputPassword);
            }
        } else if (mode == 3) {
            // Xử lý nhập ID đã được chuyển vào hàm registerFingerprint()
        }
    }
}
void showMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1: Mat khau 2:VT");
  lcd.setCursor(0, 1);
  lcd.print("3: Dang ky VT");
  mode = 0;
}

void openLock() {
  lcd.clear();
  lcd.print("Mo khoa thanh cong");
  myServo.write(90);
  delay(5000);
  myServo.write(0);
  showMainMenu();
}

void verifyFingerprint() {
    lcd.clear();
    lcd.print("Dang quet...");
    delay(5000);

    int p = finger.getImage();
    if (p != FINGERPRINT_OK) {
        lcd.clear();
        lcd.print("Loi quet vt!");
        delay(2000);
        showMainMenu();
        return;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
        lcd.clear();
        lcd.print("Loi ma hoa!");
        delay(2000);
        showMainMenu();
        return;
    }

    p = finger.fingerSearch();
    if (p != FINGERPRINT_OK) {
        lcd.clear();
        lcd.print("Khong tim thay!");
        delay(2000);
        showMainMenu();
        return;
    }

    int id = finger.fingerID;
    Serial.print("Tim thay ID: ");
    Serial.println(id);

    if (Firebase.getJSON(fbdo, "/users/" + String(id))) {
        FirebaseJson &json = fbdo.jsonObject();
        FirebaseJsonData data;
        if (json.get(data, "fingerID") && data.to<int>() == id) {
            openLock();
            return;
        }
    }

    lcd.clear();
    lcd.print("Khong hop le!");
    delay(2000);
    showMainMenu();
}
void checkPassword(String input) {
  if (Firebase.getJSON(fbdo, "/users")) {
    FirebaseJson &json = fbdo.jsonObject();
    size_t len = json.iteratorBegin();
    for (size_t i = 0; i < len; i++) {
      int type;
      String key, value;
      json.iteratorGet(i, type, key, value); // key sẽ là "1", "2", v.v.
      FirebaseJsonData data;
      if (json.get(data, key + "/pass")) {
        String firebasePass = data.to<String>();
        if (firebasePass == input) {
          openLock();
          json.iteratorEnd();
          return;
        }
      }
    }
    json.iteratorEnd();
  }

  lcd.clear();
  lcd.print("Mat khau sai!");
  delay(2000);
  showMainMenu();
}


void registerFingerprint() {
    lcd.clear();
    lcd.print("Nhap ID:");
    String inputID = "";
    mode = 3; // Chuyển sang chế độ nhập ID đăng ký

    while (mode == 3) {
        char key = keypad.getKey();
        if (key) {
            if (key == '#') {
                if (inputID.length() > 0) {
                    int userID = inputID.toInt();
                    lcd.clear();
                    lcd.print("Dang kiem tra ID...");

                    String userPath = "/users/" + String(userID);
                    if (Firebase.getJSON(fbdo, userPath)) {
                        // ID tồn tại trên Firebase, tiến hành đăng ký vân tay
                        lcd.clear();
                        lcd.print("Dat ngon tay len");
                        int p = -1;
                        while (p != FINGERPRINT_OK) {
                            p = finger.getImage();
                            if (p == FINGERPRINT_NOFINGER) continue;
                            if (p != FINGERPRINT_OK) {
                                lcd.clear();
                                lcd.print("Loi anh lan 1");
                                delay(2000);
                                showMainMenu();
                                return;
                            }
                        }

                        if (finger.image2Tz(1) != FINGERPRINT_OK) {
                            lcd.clear();
                            lcd.print("Loi ma hoa 1");
                            delay(2000);
                            showMainMenu();
                            return;
                        }

                        lcd.clear();
                        lcd.print("Nhap lai vtay");
                        delay(2000);
                        while (finger.getImage() != FINGERPRINT_NOFINGER);

                        p = -1;
                        while (p != FINGERPRINT_OK) {
                            p = finger.getImage();
                            if (p == FINGERPRINT_NOFINGER) continue;
                            if (p != FINGERPRINT_OK) {
                                lcd.clear();
                                lcd.print("Loi anh lan 2");
                                delay(2000);
                                showMainMenu();
                                return;
                            }
                        }

                        if (finger.image2Tz(2) != FINGERPRINT_OK) {
                            lcd.clear();
                            lcd.print("Loi ma hoa 2");
                            delay(2000);
                            showMainMenu();
                            return;
                        }

                        if (finger.createModel() != FINGERPRINT_OK) {
                            lcd.clear();
                            lcd.print("Tao mau that bai");
                            delay(2000);
                            showMainMenu();
                            return;
                        }

                        if (finger.storeModel(userID) != FINGERPRINT_OK) {
                            lcd.clear();
                            lcd.print("Luu cam bien FAIL");
                            delay(2000);
                            showMainMenu();
                            return;
                        }

                        // Lưu ID vân tay lên Firebase theo ID người dùng
                        String fingerIDPath = "/users/" + String(userID) + "/fingerID";
                        if (Firebase.setInt(fbdo, fingerIDPath, userID)) {
                            lcd.clear();
                            lcd.print("Dang ky thanh cong");
                            lcd.setCursor(0, 1);
                            lcd.print("ID: " + String(userID));
                            delay(3000);
                            showMainMenu();
                        } else {
                            lcd.clear();
                            lcd.print("Loi Firebase");
                            delay(2000);
                            showMainMenu();
                        }
                        mode = 0; // Trở lại chế độ menu
                        return;
                    } else {
                        // ID không tồn tại trên Firebase
                        lcd.clear();
                        lcd.print("ID khong hop le!");
                        delay(2000);
                        showMainMenu();
                        mode = 0; // Trở lại chế độ menu
                        return;
                    }
                } else {
                    lcd.setCursor(0, 1);
                    lcd.print("ID khong hop le");
                    delay(1000);
                    lcd.setCursor(0, 1);
                    lcd.print("             "); // Xóa dòng
                }
            } else {
                inputID += key;
                lcd.setCursor(0, 1);
                lcd.print(inputID);
            }
        }
    }
}
