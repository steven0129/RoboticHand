#include <Servo.h>  // 引入控制伺服馬達的函式庫

// 會出現在序列埠(Serial)訊息裡面的常數
const String mDELIMETER = ",";            // 輸入Data Streamer的字串需要用逗號分隔

String mInputString = "";                 // mInputString用來放傳入資料的變數
boolean mStringComplete = false;          // mInputString是否是完整的 (有無換行符號)

// 用來控制序列埠, 伺服馬達, 和程式的流程
int mServo_Interval = 35;                 // 每35毫秒更新一次伺服馬達的轉動位置
unsigned long mServo_PreviousTime = millis();    // 紀錄伺服馬達運轉的時間點

int mSerial_Interval = 75;                // 每75毫秒更新一次序列埠
unsigned long mSerial_PreviousTime = millis();   // 紀錄序列埠開啟的時間點

// Censoring constants used in censorTheBird() - censors WHEN:
const int MIN_BIRD = 25;                  // middle finger is below this
const int MAX_BIRD = 55;                  // AND remaining digits are above this

// 壓力感測器的最小值跟最大值
const int mSENSOR_MIN = 0;                // 設最小值為0
const int mSENSOR_MAX = 100;              // 設最大值為100

// 伺服馬達的最大角度跟最小角度
const int mSERVO_MIN = 4;                 // 設最小角度為4
const int mSERVO_MAX = 176;               // 設最大角度為176

// 數值超過25代表手指彎曲
const int flexThreshold = 25;

// 壓力感測器的數量
const int mNUM_SENSORS = 5;               // 5隻手指頭

// 為了自動校準而用來儲存最大最小值的7x2陣列
int mMinMax[mNUM_SENSORS][2] = {0};

// 用來儲存最後16個值的7x16陣列，用來消除不穩定脈衝
const int NUM_SAMPLES = 16;
int smoothingIndex = 0;
int mSensorSmoothing[mNUM_SENSORS][NUM_SAMPLES] = {0};
int mSensorTotal[mNUM_SENSORS] = {0};

// 紀錄壓力感測器的變數
int sensor1;

// 控制伺服馬達的變數
Servo servo1;

void setup() {
  Serial.begin(9600);  
  
  // 將5台servo對應到arduino的5個腳位
  servo1.attach(3);
}

/*
 * 開始主程式 -------------------------------------------------------------
 */ 
void loop()
{
  // 處理壓力感測器和伺服馬達: 讓手持續移動和資料持續傳輸
  processSensorsServos();
 
  // 從序列埠讀取Excel的指令
  processIncomingSerial();

  // 透過序列埠處理和傳送資料給Excel
  processOutgoingSerial();  
}

/*
 * 輸入壓力感測器以及輸出伺服馬達的程式碼--------------------------------------------------------------
 */
void processSensorsServos()
{
  if((millis() - mServo_PreviousTime) > mServo_Interval) // 超過mServer_Interval的時間時才執行
  {
    mServo_PreviousTime = millis();         // 重設時間點
    readSensors();
    driveServos();
  } 
}


void readSensors()
{
  // 從類比訊號的針腳讀取壓力感測器的資料
  sensor1 = getSensorValue(1);  // 食指
  
  smoothingIndex++;                       // increment smoothing array index
  if(smoothingIndex >= NUM_SAMPLES)       // if we hit then end of the array...
  {
    smoothingIndex = 0;                   // reset smoothing array index
  }
}


void driveServos()
{
  // Hand 1 servo writes
  servo1.write(mapServo(sensor1)); // 食指
}


int getSensorValue(int sensorPin)
{   
  int sensorValue = analogRead(sensorPin);                // 讀取壓力感測器的值
  sensorValue = smooth(sensorValue, sensorPin);           // 消除電壓尖峰
  if(sensorValue < mMinMax[sensorPin][0]) {mMinMax[sensorPin][0] = sensorValue;}  // 設定最小值
  if(sensorValue > mMinMax[sensorPin][1]) {mMinMax[sensorPin][1] = sensorValue;}  // 設定最大值
  // 將原始ADC的值 (5V對應到0-1023) 轉換成0-100
  sensorValue = map(sensorValue, mMinMax[sensorPin][0], mMinMax[sensorPin][1], mSENSOR_MIN, mSENSOR_MAX);
  return sensorValue;
}


int mapServo(int sensorValue)
{   // 將壓力感測器的值轉換成伺服馬達的轉動位置
  return map(sensorValue, mSENSOR_MIN, mSENSOR_MAX, mSERVO_MIN, mSERVO_MAX);
}


int smooth(int sensorValue, int sensorPin)
{
  mSensorTotal[sensorPin] = sensorValue;                          
  mSensorSmoothing[sensorPin][smoothingIndex] = sensorValue;       
  mSensorTotal[sensorPin] = mSensorTotal[sensorPin] + sensorValue;
  sensorValue = mSensorTotal[sensorPin]/NUM_SAMPLES;                // 使用移動平均法來對消除電壓尖峰
  return sensorValue;
}


/*
 * 處理從序列埠輸入的資料-------------------------------------------------------------------
 */

void processIncomingSerial()
{
  getSerialData();
  parseSerialData();
}


// 收集從序列埠進入的位元組成mInputString
void getSerialData() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();      // 取得新的byte
    mInputString += inChar;                 // 將之加入mInputString
    if (inChar == '\n') {                   // 如果遇到換行\n
      mStringComplete = true;               // 這筆資料就算處理完成了
    }
  }
}


void parseSerialData() 
{
  if (mStringComplete) { // 根據mInputString來設定程式變數 
    //process serial data - set variables using: var = getValue(mInputString, ',', index).toInt(); // see getValue function below
    mInputString = "";                         // 重設mInputString
    mStringComplete = false;                   // 重設stringComplete
  }
}


// 利用字串比對演算法取得mInputString的值
String getValue(String mDataString, char separator, int index)
{ // mDataString就是mInputString, separator就是逗號, index就是我們在尋找的資料陣列的地方
  int matchingIndex = 0;
  int strIndex[] = {0, -1};
  int maxIndex = mDataString.length()-1; 
  for(int i=0; i<=maxIndex && matchingIndex<=index; i++){     // loop until end of array or until we find a match
    if(mDataString.charAt(i)==separator || i==maxIndex){       // if we hit a comma OR we are at the end of the array
      matchingIndex++;                                        // increment matchingIndex to keep track of where we have looked
      // set substring parameters (see return)
      strIndex[0] = strIndex[1]+1;                            // increment first substring index
      // ternary operator in objective c - [condition] ? [true expression] : [false expression] 
      strIndex[1] = (i == maxIndex) ? i+1 : i;                // set second substring index
    }
  }
  return matchingIndex>index ? mDataString.substring(strIndex[0], strIndex[1]) : ""; // if match return substring or ""
}


/*
 * 輸出序列埠資料-------------------------------------------------------------------
 */
void processOutgoingSerial()
{
  if((millis() - mSerial_PreviousTime) > mSerial_Interval)  // 超過mSerial_Interval的時間時才執行
  {
    mSerial_PreviousTime = millis(); // 重設時間點
    sendDataToSerial(); 
  }
}

void sendDataToSerial()
{
  Serial.print(0);                 //mWorkbookMode - not used anymore;
  
  Serial.print(mDELIMETER);
  Serial.print(0);     // 開始剪刀石頭布比對

  Serial.print(mDELIMETER);
  Serial.print(0);    // 比對完成
  
  Serial.print(mDELIMETER);
  Serial.print(0);        // 每回合的倒數計時

  // Hand 1 sensor data for visualization in Machines That Emulate Humans workbook. 
  Serial.print(mDELIMETER);
  Serial.print(0);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor1);
  
  Serial.print(mDELIMETER);
  Serial.print(0);
  
  Serial.print(mDELIMETER);
  Serial.print(0);
  
  Serial.print(mDELIMETER);
  Serial.print(0);

  Serial.println();
}

