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

int mRound_Interval = 5000;               // 每5000毫秒偵測一次剪刀石頭布
unsigned long mRound_PreviousTime = millis();    // 用來記錄剪刀石頭布偵測的時間點

int mMatchEnd_Interval = 3000;            // 剪刀石頭布比對完成後3000毫秒再顯示結果
unsigned long mMatchEnd_PreviousTime = millis();    // 用來記錄比對完成的時間點

// 用來倒數計時的變數
int mCountDown = 0;                       // 紀錄現在倒數到哪個數字了
unsigned long mCountDownStartTime = 0;    // 紀錄開始倒數的時間點

// 剪刀石頭布的常數
const int ROCK = 1;        // 石頭
const int PAPER= 2;        // 布
const int SCISSORS = 3;    // 剪刀
const int NAG = -1;        // 都不是

// 剪刀石頭布的變數
int mPlayer1RPSgesture = 0;  // Player1的手勢
int mPlayer2RPSgesture = 0;  // Player2的手勢
int mExcelRPSgesture = 0;    // Excel上的手勢

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

// 控制程式流程的變數
int mMatchTrigger = 0;                    // Excel發送1之後馬上設mStartMatch=0
int mStartMatch = 0;                      // mMatchTrigger設定這個變數以啟動剪刀石頭布比對
int mMatchEnding = 0;                     // 比對完成但還沒顯示
int mMatchComplete = 1;                   // 顯示完成才算完成

int mRoundsPerMatch = 5;                  // 每次剪刀石頭布比對的回合數
int mRound = 0;                           // 已經比了幾回合

// 存放Player1和Player2每回合的手勢
int mPlayer1rounds[5];
int mPlayer2rounds[5];

// 紀錄壓力感測器的變數
int sensor0; int sensor1; int sensor2; int sensor3; int sensor4;

// 控制伺服馬達的變數
Servo servo0; Servo servo1; Servo servo2; Servo servo3; Servo servo4; 

void setup() {
  Serial.begin(9600);  
  
  // 將5台servo對應到arduino的5個腳位
  servo0.attach(2); servo1.attach(3); servo2.attach(4); servo3.attach(5); servo4.attach(6);
}

/*
 * 開始主程式 -------------------------------------------------------------
 */ 
void loop()
{
  if(mMatchTrigger==1) // Excel觸發剪刀石頭布
  {    
    mMatchTrigger = 0;              // 每次比對都重設一次mMatchTrigger
    mMatchComplete = 0;             // 重設mMatchComplete
    mRound = 0;                     // 重設回合數
    for(int i=0; i<5; i++)          // 重設Player1跟Player2的手勢
    {
      mPlayer1rounds[i] = 0;
      mPlayer2rounds[i] = 0;
    }
    mStartMatch = 1;                // 開始比對
  }

  if(mStartMatch==1) // 每回合都都執行這一段一次
  {    
    if( (millis() - mRound_PreviousTime) > mRound_Interval )
    {
      mRound++;                       // 回合數+1
      mCountDownStartTime = millis(); // 重設倒數計時時間
      countDown();                    // 倒數計時
      getRPSGestures();               // 收集手勢資料
      mRound_PreviousTime=millis();   // 重設回合間隔計時器
    }

    // 剪刀石頭布比對完後執行這一段
    if(mRound == mRoundsPerMatch)     // 在最後一回合後重設比對
    {
      mMatchEnding = 1;               
      mMatchTrigger = 0;
      mStartMatch = 0;  
      mMatchEnd_PreviousTime = millis(); // 開始mMatchEnd_Interval 
    }
  }

  // 在最後一回合後等mMatchEnd_Interval的時間逃脫
  // 然後進入這一段完成比對
  if(mMatchEnding==1 && (millis() - mMatchEnd_PreviousTime) > mMatchEnd_Interval) 
  {
    mMatchEnding = 0;
    mMatchComplete = 1;       // 顯示比對結果在Excel上
  }

  // 處理壓力感測器和伺服馬達: 讓手持續移動和資料持續傳輸
  processSensorsServos();
 
  // 從序列埠讀取Excel的指令
  processIncomingSerial();

  // 透過序列埠處理和傳送資料給Excel
  processOutgoingSerial();  
}


/*
 * 剪刀石頭布手勢偵測
 */
void getRPSGestures()
{
/*
 *   "e"代表手指伸直
 *   "f"代表手指全彎
 *   3個字代表3隻手指頭
 *   例如: 3隻手指頭全伸為"fff"
 *   
 *   Note: 這邊沒有偵測大拇指跟小指
 */

  readSensors();                        // 取得目前手指頭的位置

  String gesture1="";                   
  gesture1 += fingerPosition(sensor1);  // 食指
  gesture1 += fingerPosition(sensor2);  // 中指
  gesture1 += fingerPosition(sensor3);  // 無名指

  mPlayer1RPSgesture = getGesture(gesture1);      // 取得Player1的手勢
  mPlayer1rounds[mRound-1] = mPlayer1RPSgesture;  // 紀錄在Player1的回合陣列
  
  mPlayer2rounds[mRound-1] = mExcelRPSgesture;  // 紀錄在Player2的回合陣列
  mPlayer2RPSgesture = mExcelRPSgesture;
}

// 判斷手指現在是伸直、彎曲還是不在範圍內(以0-100代表)
String fingerPosition(int sensor)
{
  if(sensor>=0 && sensor <=flexThreshold)
    {return "e";}  // 伸直
  else if(sensor>=flexThreshold && sensor <=100)
    {return "f";}  // 全彎
  else 
    {return "x";}  // 不在範圍內(不該發生)
}


// 根據彎曲伸直的資料轉換成手勢
int getGesture(String gesture)
{
  if(gesture == "fff")        // 石頭
    {return ROCK;}
  else if(gesture == "eee")   // 布
    {return PAPER;}
  else if(gesture == "eef")   // 剪刀
    {return SCISSORS;}
  else
  {return NAG;}               // 不是手勢
}

/* 
 *  倒數計時
 */
void countDown()  
{ // 這邊會一直執行直到結束
  int countdownFinished = 0;                          // 重設倒數計時
  while(countdownFinished==0) {
    int timeSlice = millis() - mCountDownStartTime;   // 決定時間過了多久
    if(timeSlice >= 0 && timeSlice <= 1000) {
      mCountDown = 4;
    }
    if(timeSlice >= 1001 && timeSlice <= 2000 ) {
      mCountDown = 3;
    }
    if(timeSlice >= 2001 && timeSlice <= 3000 ) { 
      mCountDown = 2;
    }
    if(timeSlice >= 3001 && timeSlice <= 4000 ) { 
      mCountDown = 1;
    }
    if(timeSlice >= 4001 && timeSlice <= 5250 ) { 
      mCountDown = 0;
    }
    if(timeSlice > 5251) {
      mCountDown = -1;
      countdownFinished = 1;
    }
    processSensorsServos();   // 讓手掌持續移動
    processIncomingSerial();  // 從序列埠讀取Excel的命令
    processOutgoingSerial();  // 處理並透過序列埠將訊息送回給Excel
  }
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
  sensor0 = getSensorValue(0);  // 大拇指
  sensor1 = getSensorValue(1);  // 食指
  sensor2 = getSensorValue(2);  // 中指
  sensor3 = getSensorValue(3);  // 無名指
  sensor4 = getSensorValue(4);  // 小指

  // censor the middle finger gesture
  sensor2 = censorTheBird(sensor0, sensor1, sensor2, sensor3, sensor4);
  
  smoothingIndex++;                       // increment smoothing array index
  if(smoothingIndex >= NUM_SAMPLES)       // if we hit then end of the array...
  {
    smoothingIndex = 0;                   // reset smoothing array index
  }
}


void driveServos()
{
  // Hand 1 servo writes
  servo0.write(mapServo(sensor0)); // 大拇指
  servo1.write(mapServo(sensor1)); // 食指
  servo3.write(mapServo(sensor3)); // 中指
  servo4.write(mapServo(sensor4)); // 無名指
  servo2.write(mapServo(sensor2)); // 小指
}


int censorTheBird(int thumb, int index, int middle, int ring, int pinky)
{
  if(index>MAX_BIRD && middle<MIN_BIRD && ring>MAX_BIRD)
  {
    return 100;             // pull it down
  } else {
    return middle;          // leave it be
  }
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
    
    mRound_Interval       = getValue(mInputString, ',', 4).toInt();   // Excel工作表"Data Out"的E5欄位
    mRound_Interval       = mRound_Interval * 1000;

    if(mMatchComplete==1){
      mMatchTrigger       = getValue(mInputString, ',', 5).toInt();   // Excel工作表"Data Out"的F5欄位
    }

    mExcelRPSgesture    = getValue(mInputString, ',', 8).toInt();   // Excel工作表"Data Out"的I5欄位
      
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
  // 剪刀石頭埠的程式流程控制
  Serial.print(0);                 //mWorkbookMode - not used anymore;
  
  Serial.print(mDELIMETER);
  Serial.print(mMatchTrigger);     // 開始剪刀石頭布比對

  Serial.print(mDELIMETER);
  Serial.print(mMatchComplete);    // 比對完成
  
  Serial.print(mDELIMETER);
  Serial.print(mCountDown);        // 每回合的倒數計時

  // Hand 1 sensor data for visualization in Machines That Emulate Humans workbook. 
  Serial.print(mDELIMETER);
  Serial.print(sensor0);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor1);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor2);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor3);
  
  Serial.print(mDELIMETER);
  Serial.print(sensor4);
  
  //Current round gesture variables for Rock,Paper,Scissors workbook
  Serial.print(mDELIMETER);
  Serial.print(mRound);
  
  Serial.print(mDELIMETER);
  Serial.print(mPlayer1RPSgesture);
  
  Serial.print(mDELIMETER);
  Serial.print(mPlayer2RPSgesture);

  //Player1 gestures rounds 1-5
  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[0]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[1]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[2]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[3]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer1rounds[4]);  
  
  //Player2 gestures rounds 1-5
  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[0]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[1]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[2]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[3]);

  Serial.print(mDELIMETER);
  Serial.print(mPlayer2rounds[4]);

  Serial.println();
}

