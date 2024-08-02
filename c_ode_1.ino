#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SDHT.h>
#include <Arduino_FreeRTOS.h>
//////////////////////////////
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
//////////////////////////////
#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#define redLed 7
#define greenLed 6
#define blueLed 5
#define relay 13
#define buzzer 33
#define LED_PW 4

////////////////////////////////////////
#define Analog_gas A4
#define Analog_lua A3
#define led 34 
#define pin_dht22 48
/////////////////////
#define FILTER_SIZE 26
#define LED_DELAY 500
/////////////////////
Servo myServo;
int servoPin = 10; //(D7)
#define trig A0   // chân trig của HC-SR04
#define echo A1  // chân echo của HC-SR04

boolean match = false;
boolean programMode = false;
int successRead;

byte storedCard[4];
byte readCard[4];
byte masterCard[4];


#define SS_PIN 53
#define RST_PIN 49
MFRC522 mfrc522(SS_PIN, RST_PIN);

//--------------------------------------------
LiquidCrystal_I2C lcd2(0X27, 20, 4);
LiquidCrystal_I2C lcd(0x26, 16, 2);
SDHT dht;
//................................................
char password[4];
char initial_password[4],new_password[4];
int vcc=11;
int i=0;
//int relay_pin = 11;
char key_pressed=0;
const byte rows = 4; 
const byte columns = 4; 
char hexaKeys[rows][columns] = {
{'1','2','3','A'},
{'4','5','6','B'},
{'7','8','9','C'},
{'*','0','#','D'}
};
byte row_pins[rows]={A8,A9,A10,A11};
byte column_pins[columns]={A12,A13,A14,A15};
Keypad keypad_key=Keypad( makeKeymap(hexaKeys),row_pins,column_pins,rows,columns);
//----------------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
  xTaskCreate(Humi_Temp_function, "Humi_Temp", 200, NULL, 2, NULL);
  xTaskCreate(RFID_CLOCK_function, "RFID_CLOCK", 500, NULL, 1, NULL);
  xTaskCreate(keypad_password_function, "keypad", 500, NULL,1, NULL);
  xTaskCreate(distance_hcsr04_function, "hcsr04", 500, NULL, 1, NULL);
}
//......................................................
float GetDistance()
{
    unsigned long duration; // biến đo thời gian
    int distanceCm;         // biến lưu khoảng cách
    /* Phát xung từ chân trig */
    digitalWrite(trig,0);   // tắt chân trig
    delayMicroseconds(2);
    digitalWrite(trig,1);   // phát xung từ chân trig
    delayMicroseconds(5);   // xung có độ dài 5 microSeconds
    digitalWrite(trig,0);   // tắt chân trig
    /* Tính toán thời gian */
    // Đo độ rộng xung HIGH ở chân echo. 
    duration = pulseIn(echo,HIGH);  
    // Tính khoảng cách đến vật.
    distanceCm = int(duration/2/29.412);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return distanceCm;
}

void distance_function()
{
  long distance = GetDistance();
  if (distance <= 0)
  {
    Serial.println("Quá thời gian đo khoảng cách !!");
  }
  else
  {   
    Serial.print("Khoảng cách (cm): ");
    Serial.println(distance);
  }
  if (distance < 20) 
  {
    runServo();
  }
}

void runServo() {
  myServo.write(90);  // Cho servo quay một góc 90 độ
  //delay(3000);
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  myServo.write(0); // Cho servo trở về vị trí ban đầu
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  //delay(1000);
}
void lockDoor() {
  digitalWrite(relay, LOW);
  digitalWrite(LED_PW, LOW);
}
void unlockDoor() {
  digitalWrite(relay, HIGH);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  digitalWrite(relay, LOW); 
  digitalWrite(LED_PW, HIGH);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  digitalWrite(LED_PW, LOW);
}
long measurements2[FILTER_SIZE];
int measurementIndex2 = 0;

void addMeasurement2(long measurement) {
  measurements2[measurementIndex2] = measurement;
  measurementIndex2 = (measurementIndex2 + 1) % FILTER_SIZE;
}

long getFilteredMeasurement2() {
  long sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) {
    sum += measurements2[i];
  }
  return sum / FILTER_SIZE;
}
//-----------------------------------------------------------
void keypad_function()
{
  //digitalWrite(relay,LOW);
  key_pressed = keypad_key.getKey();
  if(key_pressed=='#')
  change();
  if(key_pressed)
  {
    password[i++]=key_pressed;
    lcd.print(key_pressed);
  }
  if(i==4)
  {
    vTaskDelay(200 / portTICK_PERIOD_MS);
    for(int j=0;j<4;j++)
    initial_password[j]=EEPROM.read(j);
    if(!(strncmp(password, initial_password,4)))
    {
      lcd.clear();
      lcd.print("Pass Accepted");
      //digitalWrite(relay,LOW);
      unlockDoor();
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      lcd.setCursor(0,0);
      lcd.print("Pres >START< to");
      lcd.setCursor(0,1);
      lcd.print("change the pass");
      vTaskDelay(3000 / portTICK_PERIOD_MS);
      lcd.clear();
      lcd.print("Enter Password:");
      lcd.setCursor(0,1);
      i=0;
    }
    else
    {
      //digitalWrite(relay, HIGH);
      lockDoor();
      lcd.clear();
      lcd.print("Wrong Password");
      lcd.setCursor(0,0);
      lcd.print("Pres >#< to");
      lcd.setCursor(0,1);
      lcd.print("change the pass");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      lcd.clear();
      lcd.print("Enter Password");
      lcd.setCursor(0,1);
      i=0;
    }
  }
}
void change()
{
  int j=0;
  lcd.clear();
  lcd.print("Current Password");
  lcd.setCursor(0,1);
  while(j<4)
  {
    char key=keypad_key.getKey();
    if(key)
    {
      new_password[j++]=key;
      lcd.print(key);
    }
    key=0;
  }
  vTaskDelay(500 / portTICK_PERIOD_MS);
  if((strncmp(new_password, initial_password, 4)))
  {
    lcd.clear();
    lcd.print("Wrong Password");
    lcd.setCursor(0,1);
    lcd.print("Try Again");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  else
  {
    j=0;
    lcd.clear();
    lcd.print("New Password:");
    lcd.setCursor(0,1);
    while(j<4)
    {
      char key=keypad_key.getKey();
      if(key)
      {
        initial_password[j]=key;
        lcd.print(key);
        EEPROM.write(j,key);
        j++;
      }
    }
    lcd.print("Pass Changed");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  lcd.clear();
  lcd.print("Enter Password");
  lcd.setCursor(0,1);
  key_pressed=0;
}
void initialpassword()
{
  int j;
  for(j=0;j<4;j++)
  EEPROM.write(j,j+49);
  for(j=0;j<4;j++)
  initial_password[j]=EEPROM.read(j);
}

//------------------------------------------------------------------
void humid_function() {
  if (dht.read(DHT11, pin_dht22)) {
    Serial.print("H:");
    Serial.print(double(dht.humidity) / 10, 1);
    Serial.print(",T:");
    Serial.println(double(dht.celsius) / 10, 1);
    lcd2.setCursor(7, 0);
    lcd2.print(double(dht.humidity) / 10, 1);
    lcd2.setCursor(7, 1);
    lcd2.print(double(dht.celsius) / 10, 1);

    // Đọc giá trị từ cảm biến khí GAS
    int gasValue = analogRead(Analog_gas);
    Serial.print("Gas:");
    Serial.println(gasValue);
    lcd2.setCursor(7, 2);
    lcd2.print(gasValue);

    // Đọc giá trị từ cảm biến lửa (flame sensor)
    int flameValue = analogRead(Analog_lua);
    Serial.print("Flame:");
    Serial.println(flameValue);
    if (flameValue < 490) {
    lcd2.setCursor(0, 3);
    lcd2.print("                ");
    lcd2.setCursor(0, 3);
    lcd2.print("    CANH BAO CHAY");
    digitalWrite(buzzer, HIGH);
    } 
    else if (flameValue > 500) {
    lcd2.setCursor(0, 3);
    lcd2.print("                ");
    lcd2.setCursor(0, 3);
    lcd2.print("       AN TOAN    ");
    digitalWrite(buzzer, LOW);
    } 
    String dataToSend = "H:" + String(double(dht.humidity) / 10, 1) + ",T:" + String(double(dht.celsius) / 10, 1) + ",Gas:" + String(gasValue) + ",Flame:" + String(flameValue) + "\n";
    Serial.println(dataToSend);
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

//------------------------------------------------------------------
void loop() 
{}

void Enter_Pass_Word(void *pvParameters) {
  Serial.print("Serial ON");
  Serial.write(0xff);
  Serial.write(0xff);
  Serial.write(0xff);
  lockDoor();
  pinMode(LED_PW, OUTPUT);
  while (1) {
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}
//---------------------------------------------
void keypad_password_function(void *pvParameters)
{
  lcd.init();
  lcd.begin(16,2);
  lcd.backlight();
  //pinMode(relay, OUTPUT);
  pinMode(vcc, OUTPUT);
  lcd.print(" MOUNT DYNAMICS ");
  lcd.setCursor(0,1);
  lcd.print("Electronic Lock ");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Enter Password");
  lcd.setCursor(0,1);
  initialpassword();
  while(1)
  {
    keypad_function();
  }
} 
//-----------------------------------------------
void Humi_Temp_function(void *pvParameters) {
  pinMode(pin_dht22, INPUT);
  lcd2.init();
  lcd2.backlight();
  lcd2.begin(20, 4);
  lcd2.setCursor(0, 0);
  lcd2.print("HUMI:  ");
  lcd2.setCursor(0, 1);
  lcd2.print("TEMP: ");
  lcd2.setCursor(11, 0);
  lcd2.print(" %");
  lcd2.setCursor(11, 1);
  lcd2.print(" C");
  lcd2.setCursor(0, 2);
  lcd2.print("GAS:  ");
 /* pinMode(relay_pin, OUTPUT);
  pinMode(vcc, OUTPUT);
  lcd.setCursor(0,0);
  lcd.print("Enter Password");
  lcd.setCursor(0,1);
  initialpassword(); */
  while (1) {
    humid_function();
  }
}
/////////////////////////////////////////

void RFID_CLOCK() {
  do {
    successRead = getID();
    if (programMode) {
      cycleLeds();
    } else {
      normalModeOn();
    }
  } while (!successRead);
  if (programMode) {
    if (isMaster(readCard)) {
      Serial.println("This is Master Card");
      Serial.println("Exiting Program Mode");
      Serial.println("-----------------------------");
      programMode = false;
      return;
    } else {
      if (findID(readCard)) {
        Serial.println("I know this PICC, so removing");
        deleteID(readCard);
        Serial.println("-----------------------------");
      } else {
        Serial.println("I do not know this PICC, adding...");
        writeID(readCard);
        Serial.println("-----------------------------");
      }
    }
  } else {
    if (isMaster(readCard)) {
      programMode = true;
      Serial.println("Hello Master - Entered Program Mode");
      int count = EEPROM.read(0);
      Serial.print("I have ");
      Serial.print(count);
      Serial.print(" record(s) on EEPROM");
      Serial.println("");
      Serial.println("Scan a PICC to ADD or REMOVE");
      Serial.println("-----------------------------");
    } else {
      if (findID(readCard)) {
        Serial.println("Welcome, You shall pass");
        digitalWrite(greenLed, HIGH);
        digitalWrite(relay, HIGH);
        digitalWrite(redLed, LOW);
        vTaskDelay(1500 / portTICK_PERIOD_MS);
        digitalWrite(greenLed, LOW);
        digitalWrite(relay, LOW);
      } else {
        Serial.println("You shall not pass");
        failed();
      }
    }
  }
}

//////////////////////////////////////////////////////
void RFID_CLOCK_function(void *pvParameters) {
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_OFF);
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  if (EEPROM.read(1) != 143) {
    Serial.println("No Master Card Defined");
    Serial.println("Scan A PICC to Define as Master Card");
    do {
      successRead = getID();
      digitalWrite(blueLed, LED_ON);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(blueLed, LED_OFF);
      vTaskDelay(200 / portTICK_PERIOD_MS);
    } while (!successRead);
    for (int j = 0; j < 4; j++) {
      EEPROM.write(2 + j, readCard[j]);
    }
    EEPROM.write(1, 143);
    Serial.println("Master Card Defined");
  }
  Serial.println("Master Card's UID");
  for (int i = 0; i < 4; i++) {
    masterCard[i] = EEPROM.read(2 + i);
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println("Waiting PICCs to bo scanned :)");
  cycleLeds();
  while (1) {
    RFID_CLOCK();
    // vTaskDelay(500/portTICK_PERIOD_MS);
  }
}
/////////////////////////////////////////////////////////////
void distance_hcsr04_function(void *pvParameters)          
{
  myServo.attach(servoPin); 
  myServo.write(0);
  pinMode(trig,OUTPUT);   // chân trig sẽ phát tín hiệu
  pinMode(echo,INPUT);    // chân echo sẽ nhận tín hiệu
  while(1)
  {
    distance_function();
  }
}
//////////////////////////////////////////////////////////////

int getID() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }

  Serial.println("Scanned PICC's UID:");
  for (int i = 0; i < 4; i++) {
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA();
  return 1;
}


void cycleLeds() {
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_ON);
  digitalWrite(blueLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(redLed, LED_ON);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
}

void normalModeOn() {
  digitalWrite(blueLed, LED_ON);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
}

void readID(int number) {
  int start = (number * 4) + 2;
  for (int i = 0; i < 4; i++) {
    storedCard[i] = EEPROM.read(start + i);
  }
}


void writeID(byte a[]) {
  if (!findID(a)) {
    int num = EEPROM.read(0);
    int start = (num * 4) + 6;
    num++;
    EEPROM.write(0, num);
    for (int j = 0; j < 4; j++) {
      EEPROM.write(start + j, a[j]);
    }
    successWrite();
  } else {
    failedWrite();
  }
}


void deleteID(byte a[]) {
  if (!findID(a)) {
    failedWrite();
  } else {
    int num = EEPROM.read(0);
    int slot;
    int start;
    int looping;
    int j;
    int count = EEPROM.read(0);
    slot = findIDSLOT(a);
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;
    EEPROM.write(0, num);
    for (j = 0; j < looping; j++) {
      EEPROM.write(start + j, EEPROM.read(start + 4 + j));
    }
    for (int k = 0; k < 4; k++) {
      EEPROM.write(start + j + k, 0);
    }
    successDelete();
  }
}


boolean checkTwo(byte a[], byte b[]) {
  if (a[0] != NULL)
    match = true;
  for (int k = 0; k < 4; k++) {
    if (a[k] != b[k])
      match = false;
  }
  if (match) {
    return true;
  } else {
    return false;
  }
}


int findIDSLOT(byte find[]) {
  int count = EEPROM.read(0);
  for (int i = 1; i <= count; i++) {
    readID(i);
    if (checkTwo(find, storedCard)) {

      return i;
      break;
    }
  }
}


boolean findID(byte find[]) {
  int count = EEPROM.read(0);
  for (int i = 1; i <= count; i++) {
    readID(i);
    if (checkTwo(find, storedCard)) {
      return true;
      break;
    } else {
    }
  }
  return false;
}



void successWrite() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(greenLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(greenLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(greenLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(greenLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(greenLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  Serial.println("Succesfully added ID record to EEPROM");
}

void failedWrite() 
{
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(redLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(redLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(redLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(redLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(redLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  Serial.println("Failed! There is something wrong with ID or bad EEPROM");
}


void successDelete() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(blueLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(blueLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(blueLed, LED_ON);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(blueLed, LED_OFF);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  digitalWrite(blueLed, LED_ON);  //
  vTaskDelay(200 / portTICK_PERIOD_MS);
  Serial.println("Succesfully removed ID record from EEPROM");
}

boolean isMaster(byte test[]) {
  if (checkTwo(test, masterCard))
    return true;
  else
    return false;
}

void failed() {
  digitalWrite(greenLed, LED_ON);
  digitalWrite(blueLed, LED_ON);
  digitalWrite(redLed, LED_OFF);
  vTaskDelay(1200 / portTICK_PERIOD_MS);
}

