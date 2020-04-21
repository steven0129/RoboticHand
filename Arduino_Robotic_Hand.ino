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

// TODO: censorTheBird()暫時看不懂
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
  
  // Hand 1 servos
  servo0.attach(2); servo1.attach(3); servo2.attach(4); servo3.attach(5); servo4.attach(6);
}

/*
 * START OF MAIN LOOP -------------------------------------------------------------
 */ 
void loop()
{
  if(mMatchTrigger==1) // Excel sends a trigger to enter into a match. 
  {    
    mMatchTrigger = 0;              // reset so we only enter into this once per match
    mMatchComplete = 0;             // reset match complete flag
    mRound = 0;                     // reset rounds count    
    for(int i=0; i<5; i++)          // reset round gesture data
    {
      mPlayer1rounds[i] = 0;
      mPlayer2rounds[i] = 0;
    }
    mStartMatch = 1;                // start match
  }

  if(mStartMatch==1) // Enter into this section once every round.
  {    
    if( (millis() - mRound_PreviousTime) > mRound_Interval )
    {
      mRound++;                       // increment round number     
      mCountDownStartTime = millis(); // reset countdown start time      
      countDown();                    // enter contdown sequence
      getRPSGestures();               // gather gesture data from glove      
      mRound_PreviousTime=millis();   // reset round interval timer
    }

    // Enter into this section at the end of the match
    if(mRound == mRoundsPerMatch)     // After last round reset match
    {
      mMatchEnding = 1;               
      mMatchTrigger = 0;
      mStartMatch = 0;  
      mMatchEnd_PreviousTime = millis(); // Start the mMatchEnd_Interval 
    }
  }

  // After last round wait for mMatchEnd_Interval to elapse,
  // then enter into this section to complete the match.
  if(mMatchEnding==1 && (millis() - mMatchEnd_PreviousTime) > mMatchEnd_Interval) 
  {
    mMatchEnding = 0;
    mMatchComplete = 1;       // Trigger sent to Excel to display final results of match
  }

  // Process sensors and drive servos - keep the hand moving and data flowing
  processSensorsServos();
 
  // Read Excel commands from serial port
  processIncomingSerial();

  // Process and send data to Excel via serial port
  processOutgoingSerial();  
}


/*
 * RPS GESTURE DETECTION
 */
void getRPSGestures()
{
/*
 *   Sensors are read and finger position is determined
 *   as either full extension ("e") or full flexion ("f")
 *   gesture is a string that is built up of 3 letters 
 *   example of full flexion of 3 fingers: gesture = "fff"
 *   
 *   Note: thumb and pinkie are very unreliable so they are 
 *   not used for gesture detection
 */

  readSensors();                        // get current position of fingers

  String gesture1="";                   // build 
  gesture1 += fingerPosition(sensor1);  // i-index
  gesture1 += fingerPosition(sensor2);  // m-middle
  gesture1 += fingerPosition(sensor3);  // a-ring

  mPlayer1RPSgesture = getGesture(gesture1);      // Read player 1 RPS gesture
  mPlayer1rounds[mRound-1] = mPlayer1RPSgesture;  // add it to player 2 round data array
  
  mPlayer2rounds[mRound-1] = mExcelRPSgesture;  // add it to player 2 round data array
  mPlayer2RPSgesture = mExcelRPSgesture;
}

// translates finger position (0-100) into flexion, extension, or out of range
String fingerPosition(int sensor)
{
  if(sensor>=0 && sensor <=flexThreshold)
    {return "e";}  // full extension
  else if(sensor>=flexThreshold && sensor <=100)
    {return "f";}  // full flexion 
  else 
    {return "x";}  // out of range (should never happen)
}


// translates flexion/extension into hand gesture
int getGesture(String gesture)
{ //index, middle, ring only
  if(gesture == "fff")
    {return ROCK;}
  else if(gesture == "eee")
    {return PAPER;}
  else if(gesture == "eef")
    {return SCISSORS;}
  else
  {return NAG;}   // Not A Gesture
}

/* 
 *  COUNTDOWN SEQUENCE
 */
void countDown()  
{ // enter into this and stay here until complete
  int countdownFinished = 0;                          // reset countown flag
  while(countdownFinished==0) {
    int timeSlice = millis() - mCountDownStartTime;   // determine time passed
    if(timeSlice >= 0 && timeSlice <= 1000) {         // 1st second interval
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
    processSensorsServos();   // Keep the hand moving   
    processIncomingSerial();  // Read Excel commands from serial port
    processOutgoingSerial();  // Process and send message to Excel via serial port
  }
}


/*
 * SENSOR INOUT AND SERVO OUTPUT CODE--------------------------------------------------------------
 */
void processSensorsServos()
{
  if((millis() - mServo_PreviousTime) > mServo_Interval) // Enter into this only when interval has elapsed
  {
    mServo_PreviousTime = millis();         // Reset interval timestamp
    readSensors();
    driveServos();
  } 
}


void readSensors()
{
  // Hand sensor reads from analog pins
  sensor0 = getSensorValue(0);  // p-thumb
  sensor1 = getSensorValue(1);  // i-index
  sensor2 = getSensorValue(2);  // m-middle
  sensor3 = getSensorValue(3);  // a-ring
  sensor4 = getSensorValue(4);  // c-pinky

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
  servo0.write(mapServo(sensor0)); // p-thumb
  servo1.write(mapServo(sensor1)); // i-index
  servo3.write(mapServo(sensor3)); // m-middle
  servo4.write(mapServo(sensor4)); // a-ring
  servo2.write(mapServo(sensor2)); // c-pinky
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
  int sensorValue = analogRead(sensorPin);                // read sensor values  
  sensorValue = smooth(sensorValue, sensorPin);           // smooth out voltage peaks  
  if(sensorValue < mMinMax[sensorPin][0]) {mMinMax[sensorPin][0] = sensorValue;}  // set min
  if(sensorValue > mMinMax[sensorPin][1]) {mMinMax[sensorPin][1] = sensorValue;}  // set max  
  // Map the raw ADC values (5v to range 0-1023) to range 0-100
  sensorValue = map(sensorValue, mMinMax[sensorPin][0], mMinMax[sensorPin][1], mSENSOR_MIN, mSENSOR_MAX);
  return sensorValue;
}


int mapServo(int sensorValue)
{   // map sensor value to servo position
  return map(sensorValue, mSENSOR_MIN, mSENSOR_MAX, mSERVO_MIN, mSERVO_MAX);
}


int smooth(int sensorValue, int sensorPin)
{
  mSensorTotal[sensorPin] = sensorValue;                            // add to totals array
  mSensorSmoothing[sensorPin][smoothingIndex] = sensorValue;        // add to smoothing array
  mSensorTotal[sensorPin] = mSensorTotal[sensorPin] + sensorValue;  // add to total
  sensorValue = mSensorTotal[sensorPin]/NUM_SAMPLES;                // get moving average 
  return sensorValue;
}


/*
 * INCOMING SERIAL DATA PROCESSING CODE-------------------------------------------------------------------
 */

void processIncomingSerial()
{
  getSerialData();
  parseSerialData();
}


//Gather bits from serial port to build mInputString
void getSerialData() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();      // get new byte
    mInputString += inChar;                  // add it to input string
    if (inChar == '\n') {                   // if we get a newline... 
      mStringComplete = true;                // we have a complete string of data to process
    }
  }
}


void parseSerialData() 
{
  if (mStringComplete) { // process data from mInputString to set program variables. 
    //process serial data - set variables using: var = getValue(mInputString, ',', index).toInt(); // see getValue function below
    
    mRound_Interval       = getValue(mInputString, ',', 4).toInt();   //Data Out worksheet cell E5
    mRound_Interval       = mRound_Interval * 1000;

    if(mMatchComplete==1){
      mMatchTrigger       = getValue(mInputString, ',', 5).toInt();   //Data Out worksheet cell F5
    }

    mExcelRPSgesture    = getValue(mInputString, ',', 8).toInt();   //Data Out worksheet cell I5
      
    mInputString = "";                         // reset mInputString
    mStringComplete = false;                   // reset stringComplete flag
  }
}


//Get value from mInputString using a matching algorithm
String getValue(String mDataString, char separator, int index)
{ // mDataString is mInputString, separator is a comma, index is where we want to look in the data 'array'
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
 * OUTGOING SERIAL DATA PROCESSING CODE-------------------------------------------------------------------
 */
void processOutgoingSerial()
{
  if((millis() - mSerial_PreviousTime) > mSerial_Interval)  // Enter into this only when interval has elapsed
  {
    mSerial_PreviousTime = millis(); // Reset interval timestamp
    sendDataToSerial(); 
  }
}

void sendDataToSerial()
{
  //Program flow variables for Rock,Paper,Scissors workbook
  Serial.print(0);                 //mWorkbookMode - not used anymore;
  
  Serial.print(mDELIMETER);
  Serial.print(mMatchTrigger);     // Starts a Rock,Paper,Scissors match

  Serial.print(mDELIMETER);
  Serial.print(mMatchComplete);    // Flag for match completion
  
  Serial.print(mDELIMETER);
  Serial.print(mCountDown);        // Countdown in between match rounds

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

