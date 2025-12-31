#include <Wire.h>
#include <RTClib.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

/* ================== LCD ================== */
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

/* ================== RTC ================== */
RTC_DS3231 rtc;
#define CLOCK_INTERRUPT_PIN 2

/* ================= LED ===================*/
const int ledPins[] = {A3, A2, A1, A0}; 
const int liczbaDiod = 4;
bool mode = 0;

/* ================== KEYPAD ================== */
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {3, 4, 5, 6};
byte colPins[COLS] = {7, 8, 9, 10};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

/* ================== DFPLAYER ================== */
SoftwareSerial mp3Serial(12, 11); // RX, TX
DFRobotDFPlayerMini mp3;
int numberOfTracks = 0;
int soundVolume = 20;
int tempVol = 20;

/* ================== ALARM ================== */
int alarmHour = 7;
int alarmMinute = 0;
int alarmSound = 1;
int tempAlarmSound = 1;
byte soundChangeStep = 0;
byte alarmChangeStep = 0;

bool alarmActive = false;
bool alarmEnabled = false;
int dayIndex = 0;

/* ============== CLOCK =======================*/
char daysOfTheWeek[7][12] = {"Niedz", "Pon", "Wt", "Sr", "Czw", "Pt", "Sob"};
byte timeChangeStep = 0;
byte digitIndex = 0;
byte dateChangeStep = 0;
int newDay = 0;
int newMonth = 0;
int newYear = 2000;
char digits[3];
int newHour = 0;
int newMinute = 0;

/* ================== MENU ================== */
enum State {NORMAL, MENU, SET_ALARM, SET_TIME, SET_DATE, SET_SOUND, SET_VOLUME, ALARM_OFF, ALARM_TASK};
State currentState = NORMAL;
int menuIndex = 0;

const char* menuItems[] = {
  "Ustaw alarm",
  "Ustaw godzine",
  "Ustaw date",
  "Dzwiek alarmu",
  "Glosnosc",
  "Wylacz alarm",
  "Powrot"
};
const int MENU_SIZE = 7;

/* ================== TASKS ====================*/
int mathA, mathB;
int correctAnswer;
char mathOp;
char answerBuffer[5];
byte answerIndex = 0;

/* ================== SETUP ================== */
void setup() {
  lcd.begin(16, 4);
  lcd.backlight();

  Serial.begin(9600);

  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), alarmINT, FALLING);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  rtc.disable32K();

  mp3Serial.begin(9600);
  mp3.begin(mp3Serial);
  mp3.volume(soundVolume);

  for (int i = 0; i < liczbaDiod; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  lcd.clear();
}

/* ================== LOOP ================== */
void loop() {
  char key = keypad.getKey();

  if (alarmActive) {
    startAlarm();
  }

  switch (currentState) {

    case NORMAL:
      showTimeDate();
      if (key == '#') {
        currentState = MENU;
        menuIndex = 0;
        lcd.clear();
      }
      //checkAlarm();
      break;

    case MENU:
      showMenu();
      handleMenu(key);
      break;

    case SET_TIME:
      setTime(key, false);
      break;

    case SET_DATE:
      setDate(key);
      break;

    case SET_ALARM:
      setTime(key, true);
      break;

    case SET_SOUND:
      setSound(key);
      break;

    case SET_VOLUME:
      setVolume(key);
      break;

    case ALARM_OFF:
      alarmOff(key);
      break;

    case ALARM_TASK:
      handleMathTask(key);
      break;
  }
}

/* ================== FUNKCJE ================== */

void showTimeDate() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 1000) return;
  lastUpdate = millis();

  DateTime now = rtc.now();

  String yearStr = String(now.year(), DEC);
  String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  String hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC); 
  String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
  String secondStr = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
  String dayOfWeek = daysOfTheWeek[now.dayOfTheWeek()];

  lcd.setCursor(0, 0);
  lcd.print(hourStr + ":" + minuteStr + ":" + secondStr);
  lcd.setCursor(0, 1);
  lcd.print(dayOfWeek + " " + dayStr + "." + monthStr + "." + yearStr);
  if(alarmEnabled)
  {
    String alaHStr = (alarmHour < 10 ? "0" : "") + String(alarmHour);
    String alaMStr = (alarmMinute < 10 ? "0" : "") + String(alarmMinute);
    lcd.setCursor(0, 2);
    lcd.print("A: " + alaHStr + ":" + alaMStr);
  }
  lcd.setCursor(0, 3);
  lcd.print("# Menu");
}

void showMenu() {
  if (menuIndex < 4)
  {
    lcd.setCursor(0, menuIndex);
    lcd.print("> ");
    for(int i=0; i<4; i++)
    {
      lcd.setCursor(2,i);
      lcd.print(menuItems[i]);
    }
  }
  else if (menuIndex>=4)
  {
    lcd.setCursor(0, menuIndex-4);
    lcd.print("> ");
    for(int i=0; i<3; i++)
    {
      lcd.setCursor(2,i);
      lcd.print(menuItems[i+4]);
    }
  }
}

/* ====== OBSLUGA MENU ====== */
void handleMenu(char key) {
  if (key == 'A') {
    menuIndex = (menuIndex + MENU_SIZE - 1) % MENU_SIZE;
    lcd.clear();
  }
  if (key == 'B') {
    menuIndex = (menuIndex + 1) % MENU_SIZE;
    lcd.clear();
  }
  if (key == '#') {
    lcd.clear();
    switch (menuIndex) {
      case 0: currentState = SET_ALARM; break;
      case 1: currentState = SET_TIME; break;
      case 2: currentState = SET_DATE; break;
      case 3: currentState = SET_SOUND; break;
      case 4: currentState = SET_VOLUME; break;
      case 5: currentState = ALARM_OFF; break;
      case 6: currentState = NORMAL; break;
    }
  }
  if (key == '*') {
    currentState = NORMAL;
    lcd.clear();
  }
}


/* ====== USTAWIANIE CZASU ====== */
void setTime(char key, bool forAlarm) {
//Wyświetlanie
  lcd.setCursor(0,0);
  if(forAlarm) lcd.print("Ustaw alarm");
  else lcd.print("Ustaw godzine");

  lcd.setCursor(0, 1);
  if(timeChangeStep == 0) {
    lcd.print("Godzina: ");
  }
  else {
    lcd.print("Minuta: ");
  }

  lcd.print(digits[0] ? digits[0] : '_');
  lcd.print(digits[1] ? digits[1] : '_');

  lcd.setCursor(0,2);
  lcd.print("(np. 09)");
  lcd.setCursor(0,3);
  lcd.print("# OK   * Wroc");

//Wprowadzanie czasu
  if(key >= '0' && key <= '9' && digitIndex < 2) {
    digits[digitIndex++] = key;
  }

//Zatwierdzanie
  if(key == '#' && digitIndex == 2) {
    int value = (digits[0] - '0') * 10 + (digits[1] - '0');

    //Sprawdzanie poprawnych wartosci
    if ((timeChangeStep == 0 && value > 23) || (timeChangeStep == 1 && value > 59))
    {
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Bledna wartosc!");
      delay(1500);
      digitIndex = 0;
      digits[0] = digits[1] = 0;
      lcd.clear();
      return;
    }

    //Zapis nowej wartości
    if (timeChangeStep == 0 && !forAlarm) {
      newHour = value;
      timeChangeStep = 1;
    }
    else if (timeChangeStep == 0 && forAlarm)
    {
      alarmHour = value;
      timeChangeStep = 1;
    }
    else {
      if(!forAlarm) {
        newMinute = value;
        //Zapis do rtc ds3231
        DateTime now = rtc.now();
          rtc.adjust(DateTime(
            now.year(),
            now.month(),
            now.day(),
            newHour,
            newMinute,
            0
          )
        );
      }
      else {
        alarmMinute = value;
        DateTime now = rtc.now();
        DateTime alarmTime(
          now.year(),
          now.month(),
          now.day(),
          alarmHour,
          alarmMinute,
          0
        );
        if (alarmTime <= now) {
        alarmTime = alarmTime + TimeSpan(1, 0, 0, 0);
        }
        rtc.clearAlarm(1);
        rtc.setAlarm1(alarmTime, DS3231_A1_Date);
        alarmEnabled = true;
      }

      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Zapisano!");
      delay(1000);
      timeChangeStep = 0;
      currentState = MENU;
    }

    digitIndex = 0;
    digits[0] = digits[1] = 0;
    lcd.clear();
    return;
  }

// Anulowanie
  if (key == '*') {
    timeChangeStep = 0;
    digitIndex = 0;
    digits[0] = digits[1] = 0;
    currentState = MENU;
    lcd.clear();
    return;
  }
}

/* ====== USTAWIANIE DATY ====== */
void setDate(char key) {
//Wyświetlanie
  lcd.setCursor(0,0);
  lcd.print("Ustaw date");
  lcd.setCursor(0, 1);
  if(dateChangeStep == 0) {
    lcd.print("Rok: 20");
  }
  else if (dateChangeStep == 1) {
    lcd.print("Miesiac: ");
  }
  else {
    lcd.print("Dzien: ");
  }

  lcd.print(digits[0] ? digits[0] : '_');
  lcd.print(digits[1] ? digits[1] : '_');

  lcd.setCursor(0,2);
  lcd.print("(np. 09)  * Wroc");
  lcd.setCursor(0,3);
  lcd.print("# OK  C cofnij");

//Wprowadzanie daty
  if(key >= '0' && key <= '9' && digitIndex < 2) {
    digits[digitIndex++] = key;
  }

  if (key == 'C' && digitIndex > 0) {
    digitIndex--;
    digits[digitIndex] = 0;   // czyścimy cofniętą cyfrę
  }

//Zatwierdzanie
  if(key == '#' && digitIndex == 2) {
    int value = (digits[0] - '0') * 10 + (digits[1] - '0');

    //Sprawdzanie poprawnych wartosci
    if ((dateChangeStep == 1 && value > 12) || 
    (dateChangeStep == 2 && newMonth == 2 && value > 28) ||
    (dateChangeStep == 2 && (newMonth == 1 || newMonth == 3 || newMonth == 5 || newMonth == 7 || newMonth == 8 || newMonth == 10 || newMonth == 12) && value > 31) ||
    (dateChangeStep == 2 && (newMonth == 4 || newMonth == 6 || newMonth == 9 || newMonth == 11) && value > 30))
    {
      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Bledna wartosc!");
      delay(1500);
      digitIndex = 0;
      digits[0] = digits[1] = 0;
      lcd.clear();
      return;
    }

    //Zapis nowej wartości
    if (dateChangeStep == 0) {
      newYear = 2000 + value;
      dateChangeStep = 1;
    }
    else if (dateChangeStep == 1) {
      newMonth = value;
      dateChangeStep = 2;
    }
    else {
      newDay = value;
      //Zapis do rtc ds3231
      DateTime now = rtc.now();
        rtc.adjust(DateTime(
          newYear,
          newMonth,
          newDay,
          now.hour(),
          now.minute(),
          now.second()
        )
      );

      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Zapisano!");
      delay(1000);
      dateChangeStep = 0;
      currentState = MENU;
    }

    digitIndex = 0;
    digits[0] = digits[1] = 0;
    lcd.clear();
  }

//Anulowanie
  if (key == '*') {
    dateChangeStep = 0;
    digitIndex = 0;
    digits[0] = digits[1] = 0;
    currentState = MENU;
    lcd.clear();
    return;
  }
}

/* ====== USTAWIANIE DZWIĘKU ====== */
void setSound(char key) {
  numberOfTracks = mp3.readFileCounts();
  lcd.setCursor(0,0);
  lcd.print("Dzwiek: ");
  lcd.print(tempAlarmSound);

  if(key == 'B' && tempAlarmSound < numberOfTracks) {
    mp3.stop();
    tempAlarmSound++;
    mp3.play(tempAlarmSound);
  }
  else if (key == 'B' && tempAlarmSound == numberOfTracks)
  {
    mp3.stop();
    tempAlarmSound = 1;
    mp3.play(tempAlarmSound);
  }
  else if (key == 'A' && tempAlarmSound > 1) {
    mp3.stop();
    tempAlarmSound--;
    mp3.play(tempAlarmSound);
  }
  else if (key == 'A' && tempAlarmSound == 1) {
    mp3.stop();
    tempAlarmSound = numberOfTracks;
    mp3.play(tempAlarmSound);
  }

  lcd.setCursor(0, 2);
  lcd.print("A przod  B wtyl");
  lcd.setCursor(0,3);
  lcd.print("# OK   * Wroc");

  if (key == '#') {
    mp3.stop();
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Zapisano!");
    delay(1000);
    alarmSound = tempAlarmSound;
    currentState = MENU;
    lcd.clear();
  }
  else if (key == '*') {
    mp3.stop();
    tempAlarmSound = alarmSound;
    currentState = MENU;
    lcd.clear();
  }
}

void setVolume(char key)
{
  lcd.setCursor(0,0);
  lcd.print("Glosnosc:");
  lcd.print(String(tempVol));

  lcd.setCursor(0,1);
  lcd.print("A ^  B v");
  lcd.setCursor(0,2);
  lcd.print("D Testuj");
  lcd.setCursor(0,3);
  lcd.print("# OK   * Wroc");

  if (key == 'A' && tempVol < 30) {
    tempVol++;
  }
  else if (key == 'B' && tempVol > 0) {
    tempVol--;
  }
  else if (key == '#') {
    soundVolume = tempVol;
    mp3.stop();
    mp3.volume(soundVolume);
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Zapisano!");
    delay(1000);
    currentState = MENU;
    lcd.clear();
  }
  else if (key == '*') {
    mp3.stop();
    tempVol = soundVolume;
    currentState = MENU;
    lcd.clear();
  }
  else if (key == 'D') {
    mp3.volume(tempVol);
    mp3.play(alarmSound);
  }
}

int alarmSelect = 0;

/* ======= USUN ALARM =========*/
void alarmOff(char key) {

  if(alarmEnabled){
    lcd.setCursor(0, 0);
    lcd.print("Wylaczyc alarm?");
    lcd.setCursor(0, 3);
    lcd.print("# Tak   * Nie");
    if (key == '#') {
      rtc.disableAlarm(1);
      alarmEnabled = false;

      lcd.clear();
      lcd.setCursor(0,1);
      lcd.print("Wylaczono");
      delay(1000);

      currentState = MENU;
      lcd.clear();
      return;
    }
  }
  else {
    lcd.setCursor(0,1);
    lcd.print("Brak alarmow");
    delay(1000);
    currentState = MENU;
    lcd.clear();
  }

  if (key == '*') {
    alarmSelect = 0;
    currentState = MENU;
    lcd.clear();
  }
}

/* ====== ALARM ====== */
void alarmINT() {
  alarmActive = true;
}

void startAlarm() {
  currentState = ALARM_TASK;
  alarmActive = false;
  mode = 1;
  //alarm na następny dzien
  DateTime next = rtc.now() + TimeSpan(1,0,0,0);
  rtc.clearAlarm(1);
  rtc.setAlarm1(next, DS3231_A1_Date);
  // MP3
  mp3.play(alarmSound);
  int opType = random(0, 4);

  switch (opType) {
    case 0: // DODAWANIE
      mathA = random(10, 50);
      mathB = random(1, 20);
      correctAnswer = mathA + mathB;
      mathOp = '+';
      break;

    case 1: // ODEJMOWANIE
      mathA = random(20, 60);
      mathB = random(1, mathA); // żeby nie było ujemnych
      correctAnswer = mathA - mathB;
      mathOp = '-';
      break;

    case 2: // MNOŻENIE
      mathA = random(2, 10);
      mathB = random(2, 10);
      correctAnswer = mathA * mathB;
      mathOp = '*';
      break;

    case 3: // DZIELENIE (CAŁKOWITE)
      mathB = random(2, 10);
      correctAnswer = random(2, 10);
      mathA = mathB * correctAnswer;
      mathOp = '/';
      break;
  }

  answerIndex = 0;
  memset(answerBuffer, 0, sizeof(answerBuffer));

  lcd.clear();
}

void handleMathTask(char key) {
  for (byte i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], mode);
  }
  mode = !mode;

  if (mp3.available()) {
      uint8_t type = mp3.readType();
      if (type == DFPlayerPlayFinished) {
        mp3.play(alarmSound);  // gra od nowa
      }
  }

  lcd.setCursor(0,0);
  lcd.print("ALARM!!!       ");

  lcd.setCursor(0, 1);
  lcd.print(mathA);
  lcd.print(" ");
  lcd.print(mathOp);
  lcd.print(" ");
  lcd.print(mathB);
  lcd.print(" = ?     ");

  lcd.setCursor(0,2);
  lcd.print("Odp: ");
  for (byte i = 0; i < answerIndex; i++) {
    lcd.print(answerBuffer[i]);
  }

  if (key >= '0' && key <= '9' && answerIndex < 4) {
    answerBuffer[answerIndex++] = key;
  }

  if (key == '#') {
    int userAnswer = atoi(answerBuffer);

    if (userAnswer == correctAnswer) {
      stopAlarm();
    } else {
      lcd.setCursor(5, 2);
      lcd.print("           ");
      lcd.setCursor(0,3);
      lcd.print("Zla odpowiedz!");
      delay(1000);
      lcd.setCursor(0,3);
      lcd.print("               ");
      answerIndex = 0;
      memset(answerBuffer, 0, sizeof(answerBuffer));
    }
  }

  if (key == 'C' && answerIndex > 0) {
    answerIndex--;
    answerBuffer[answerIndex] = '\0';
  }
}

void stopAlarm() {

  mp3.stop();

  for (byte i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], LOW);
  }

  rtc.clearAlarm(2);

  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("Alarm wylaczony");
  delay(1500);
  lcd.clear();

  currentState = NORMAL;
}
