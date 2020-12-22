/*
  Что идёт в порт: 0-CPU temp, 1-GPU temp, 2-mother temp, 3-max HDD temp, 4-CPU load, 5-GPU load, 6-RAM use, 7-GPU memory use,
  8-maxFAN, 9-minFAN, 10-maxTEMP, 11-minTEMP, 12-manualFAN, 13-manualCOLOR, 14-fanCtrl, 15-colorCtrl, 16-brightCtrl, 17-LOGinterval, 18-tempSource
*/
// ------------------------ НАСТРОЙКИ ----------------------------
#define ERROR_BACKLIGHT 0   // 0 - гасить подсветку при потере сигнала, 1 - не гасить
// ------------------------ НАСТРОЙКИ ----------------------------

// ----------------------- ПИНЫ ---------------------------
#define BTN1 A3             // первая кнопка
#define BTN2 A2             // вторая кнопка
#define DHTPIN 3            // пин датчика DHT11
// ----------------------- ПИНЫ ---------------------------

// -------------------- БИБЛИОТЕКИ ---------------------
#include <string.h>             // библиотека расширенной работы со строками
#include <LiquidCrystal_I2C.h>  // библиотека дисплея
#include "dht.h"                // библиотека термометра DHT22
// -------------------- БИБЛИОТЕКИ ---------------------

// -------- АВТОВЫБОР ОПРЕДЕЛЕНИЯ ДИСПЛЕЯ-------------
// Если кончается на 4Т - это 0х27. Если на 4АТ - 0х3f
//#if (DRIVER_VERSION)
LiquidCrystal_I2C lcd(0x27, 20, 4);
//#else
//LiquidCrystal_I2C lcd(0x3f, 20, 4);
//#endif
// -------- АВТОВЫБОР ОПРЕДЕЛЕНИЯ ДИСПЛЕЯ-------------

#define printByte(args)  write(args);


// значок градуса!!!! lcd.write(223);
byte degree[8] = {0b11100,  0b10100,  0b11100,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000};
// правый край полосы загрузки
byte right_empty[8] = {0b11111,  0b00001,  0b00001,  0b00001,  0b00001,  0b00001,  0b00001,  0b11111};
// левый край полосы загрузки
byte left_empty[8] = {0b11111,  0b10000,  0b10000,  0b10000,  0b10000,  0b10000,  0b10000,  0b11111};
// центр полосы загрузки
byte center_empty[8] = {0b11111, 0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111};
// блоки для построения графиков
byte row8[8] = {0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row7[8] = {0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row6[8] = {0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row5[8] = {0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
byte row4[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111};
byte row3[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111};
byte row2[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111};
byte row1[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111};

char inData[82];       // массив входных значений (СИМВОЛЫ)
int PCdata[20];        // массив численных значений показаний с компьютера
byte blocks, halfs;
byte index = 0;
int display_mode = 6;
String string_convert;
unsigned long timeout;
boolean lightState, reDraw_flag = 1, updateDisplay_flag, timeOut_flag = 1;
byte lines[] = {4, 5, 7, 6};
byte plotLines[] = {0, 1, 4, 5, 6, 7};    // 0-CPU temp, 1-GPU temp, 2-CPU load, 3-GPU load, 4-RAM load, 5-GPU memory
String perc;
boolean btn1_sig, btn2_sig, btn1_flag, btn2_flag, btn3_flag, btn3_sig, timeOutLCDClear = 0, restoreConnectToPC=0;



void setup() {
  Serial.begin(9600);
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  // инициализация дисплея
  lcd.init();
  lcd.backlight();
  lcd.clear();            // очистить дисплей
}

// ------------------------------ ОСНОВНОЙ ЦИКЛ -------------------------------
void loop() {
  parsing();                          // парсим строки с компьютера
  buttonsTick();                      // опрос кнопок и смена режимов
  updateDisplay();                    // обновить показания на дисплее
  delay(1000);
  timeoutTick();                      // проверка таймаута
}
// ------------------------------ ОСНОВНОЙ ЦИКЛ -------------------------------

void buttonsTick() {
  btn1_sig = !digitalRead(BTN1);
  btn2_sig = !digitalRead(BTN2);
  if (btn1_sig && !btn1_flag) {
    display_mode++;
    reDraw_flag = 1;
    if (display_mode > 7) display_mode = 0;
    btn1_flag = 1;
  }
  if (!btn1_sig && btn1_flag) {
    btn1_flag = 0;
  }
  if (btn2_sig && !btn2_flag) {
    display_mode--;
    reDraw_flag = 1;
    if (display_mode < 0) display_mode = 7;
    btn2_flag = 1;
  }
  if (!btn2_sig && btn2_flag) {
    btn2_flag = 0;
  }
}

void parsing() {
  while (Serial.available() > 0) {
    char aChar = Serial.read();
    if (aChar != 'E') {
      inData[index] = aChar;
      index++;
      inData[index] = '\0';
    } else {
      char *p = inData;
      char *str;
      index = 0;
      String value = "";
      while ((str = strtok_r(p, ";", &p)) != NULL) {
        string_convert = str;
        PCdata[index] = string_convert.toInt();
        index++;
      }
      index = 0;
      updateDisplay_flag = 1;
    }
    timeout = millis();
    timeOut_flag = 1;
    restoreConnectToPC = 1;
    lcd.backlight();    
   }
}
   
void updateDisplay() {
  if (updateDisplay_flag) {
    if (reDraw_flag) {
      lcd.clear();
      reDraw_flag = 0;
    }
    switch (display_mode) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      case 5: 
        break;
      case 6: display1();
        break;
      case 7: display2();
        break;
    }
    updateDisplay_flag = 0;
  }
  if(timeOutLCDClear)
  {reDraw_flag = 1;}
}

/*void show_random_message1 () {
char* firstline[]={"Vita nostra", "O tempora,", "One ring", "Make Midori", "Good rat", "Rule, Britannia,", "Dao ke dao"};  
char* secondline[]={"brevis est", "o mores!", "to rule them all", "fed again", "is a fed rat", "the seas", "fei chang dao"};  
  lcd.clear();//Clean the screen  
  lcd.setCursor(0,0); 
  int array_number=random(0, 7);
  lcd.print(firstline[array_number]);
  lcd.setCursor(0,1);
  lcd.print(secondline[array_number]);
  delay(3000); 
  }*/

/*void show_random_message2 () {
char* firstline[]={"Midori is a", "I know who is a", "Please feed a", "Play with a"};
char* secondline[]={"good birdie", "green bird", "lovely parakeet", "nice parrot", "clever bird", "loud parrot", "beautiful bird"};  
  lcd.clear();//Clean the screen  
  lcd.setCursor(0,0); 
  lcd.print(firstline[random(0, 4)]);
  lcd.setCursor(0,1);
  lcd.print(secondline[random(0, 7)]);
  delay(3000); 
  }*/
  
void show_random_message3 () {
  int number1 = random (1, 100);
  int number2 = random (1, 100);
  int result = number1+number2;
//  lcd.clear();//Clean the screen  
  lcd.setCursor(0,0); 
  lcd.print(number1);
  lcd.print(" + ");
  lcd.print(number2);
  lcd.print(" = ");
//  delay(1000); 
//  lcd.setCursor(0,1); 
//  lcd.print("  ");
  lcd.print(result);
  lcd.print("  ");
//  delay(1000); 
}

void display1() {

  timeOutLCDClear = 0;

  lcd.createChar(0, degree);
  lcd.createChar(1, left_empty);
  lcd.createChar(2, center_empty);
  lcd.createChar(3, right_empty);
  lcd.createChar(4, row8);

dht DHT; //Инициация датчика DHT11
int chk = DHT.read11(DHTPIN);
int temperature = int (DHT.temperature);
int humidity = int (DHT.humidity);
  
  //--Строка a
  // a, b, c, d - имена строк. Номера строк: 0 - 1-я, 1 - 2-я, 2 - 3-я, 3 - 4-я
  int a=1;
  lcd.setCursor(0, a);
  lcd.print("CPU:");
  lcd.setCursor(4, a); lcd.print(PCdata[0]); lcd.write(223); // CPU temp
  lcd.setCursor(17, a); lcd.print(PCdata[4]); // CPU load
  if (PCdata[4] < 10) perc = "% ";
  else if (PCdata[4] < 100) perc = "%";
  else perc = "";  lcd.print(perc);
  
       byte line1 = ceil(PCdata[lines[0]]/10);
    lcd.setCursor(7, a);
    if (line1 == 0) lcd.printByte(1)
      else lcd.printByte(4);
    for (int n = 1; n < 9; n++) {
      if (n < line1) lcd.printByte(4);
      if (n >= line1) lcd.printByte(2);   
    }   
    if (line1 == 10) lcd.printByte(4)
      else lcd.printByte(3); 
         
  //--Строка b
  int b=2;
  lcd.setCursor(0, b);
  lcd.print("GPU:");
  lcd.setCursor(4, b); lcd.print(PCdata[1]); lcd.write(223); // GPU temp
  lcd.setCursor(17, b); lcd.print(PCdata[5]); // GPU load
  if (PCdata[5] < 10) perc = "% ";
  else if (PCdata[5] < 100) perc = "%";
  else perc = "";  lcd.print(perc);
  
      byte line2 = ceil(PCdata[lines[1]]/10);
    lcd.setCursor(7, b);
    if (line2 == 0) lcd.printByte(1)
      else lcd.printByte(4);
    for (int n = 1; n < 9; n++) {
      if (n < line2) lcd.printByte(4);
      if (n >= line2) lcd.printByte(2);   
    }   
    if (line2 == 10) lcd.printByte(4)
      else lcd.printByte(3);
  
  //-- Строка c
  int c=3;
  lcd.setCursor(0, c);
  lcd.print("RAMuse:");
  lcd.setCursor(17, c); lcd.print(PCdata[6]); // RAM usage
  if (PCdata[6] < 10) perc = "% ";
  else if (PCdata[6] < 100) perc = "%";
  else perc = "";  lcd.print(perc);
  
      byte line3 = ceil(PCdata[lines[3]]/10);
    lcd.setCursor(7, c);
    if (line3 == 0) lcd.printByte(1)
      else lcd.printByte(4);
    for (int n = 1; n < 9; n++) {
      if (n < line3) lcd.printByte(4);
      if (n >= line3) lcd.printByte(2);   
    }   
    if (line3 == 10) lcd.printByte(4)
      else lcd.printByte(3);
  
  //--Cтрока d
/*  int d=0;
  lcd.setCursor(0, d);
  lcd.print("Temp:");
  lcd.setCursor(5, d); lcd.print(temperature);
  lcd.setCursor(7, d);
  lcd.write(223);
  lcd.setCursor(8, d);
  lcd.print("C  Humid:");
  lcd.setCursor(17, d); lcd.print(humidity);
  lcd.setCursor(19, d);
  lcd.print("%");*/

show_random_message3 ();

}


void display2() {
  lcd.createChar(0, degree);
  lcd.createChar(1, left_empty);
  lcd.createChar(2, center_empty);
  lcd.createChar(3, right_empty);
  lcd.createChar(4, row8);

  lcd.setCursor(0, 0);
  lcd.print("0123456789");
  lcd.setCursor(0, 1);
  lcd.print("0123456789");
  lcd.setCursor(0, 2);
  lcd.print("0123456789");
  lcd.setCursor(0, 3);
  lcd.print("0123456789");
}

void timeoutTick() {
  if ((millis() - timeout > 5000))
  { lcd.clear(); }  
  while (Serial.available() < 1){
  if ((millis() - timeout > 5000) && timeOut_flag) {        
    index = 0;
    if(restoreConnectToPC)
    {
     reDraw_flag=1;
     restoreConnectToPC=0;
     if (reDraw_flag) {
     lcd.clear();
     reDraw_flag = 0;}
    } 

    lcd.setCursor(5, 1);
    lcd.print("CONNECTION");
    lcd.setCursor(7, 2);
    lcd.print("FAILED");
    reDraw_flag = 0;
    updateDisplay_flag = 1;
    timeOutLCDClear = 1;
    if(timeOutLCDClear)
    {reDraw_flag = 1;}
    if (!ERROR_BACKLIGHT) lcd.noBacklight();
  }}
}
