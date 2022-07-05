//Autor: Mateusz

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DS3231.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

#define halotron_m 6
#define halotron_h 5
#define SMC_Sleep A2 //Stepper Motor Controller
#define SMC_Step A1
#define SMC_Dir A0
#define one_wire_pin 4
#define Int_SQW 2
#define Int_ClockSynchro 3
#define WiFiStatus 12
#define Up 8
#define Down 7
#define Left 10
#define Right 9
#define Menu 11
#define positionStepPin A3

#define maxItemMenu 6

#define degCelsiusChar 0
#define ThermometerChar 1
#define UpArrowChar 2
#define DownArrowChar 3
#define choseStepMotorTemp 0
#define choseControllerTemp 1

#define timeToMeasureTemps 30
#define waitTimePressSwitch 50        //Opuznienie ponownego sprawdzenia wcisniecia przycisku w [ms]
#define timeToExitMenuSet 60          //Czas po ktorym nastapi automatyczne wyjscie z MENU [s]
#define timeToTurnOffBacklightSet 30  //Czas po ktorym nastapi wylaczenie podswietlenia LCD [s]
#define timeToSendDataToESPSet 3      //Czas po ktorym wyslane ostana cykliczne dane z Arduino do ESP [s]

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Ustawienie adresu ukladu na 0x27
DS3231 clock;
RTCDateTime dt; // Structure for data time
OneWire oneWire(one_wire_pin);
DallasTemperature sensors(&oneWire);

byte customDegCelsiusChar[] = {
  B11000,
  B11000,
  B00000,
  B00011,
  B00100,
  B00100,
  B00100,
  B00011
};
byte customThermometerChar[] = {
  B00100,
  B01010,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110
};
byte customUpArrowChar[] = {
  B00100,
  B01110,
  B11111,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100
};
byte customDownArrowChar[] = {
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B11111,
  B01110,
  B00100
};

int Hour = 17, Minute = 30, Second = 0;
int Year = 2018, Month = 1, Day = 1;

//Zmienne do cyklicznego pomiaru temperatury
bool measureTempFlag = false;
uint8_t countSecondsTemp = 0;

uint8_t countSeconds = 0; //Zmienna do zliczania sekund, przydatna do zliczania czasu

//Zmienne do cyklicznego odmierzania minuty
uint8_t minuteHasPassedFlag = 0;
int countSecondsSteps = 0;
uint8_t whichSetSteps = 0;  //0 - 7steps, 1 - 7steps, 2 - 6steps

bool useSynchroWithBellFlag = false;
bool actualStep = true;
bool nextStep = false;
uint8_t synchroNextHour = 0;
bool isFullHour = false;
uint8_t nextFullHour = 0;

bool synchroWithBellIsActive = false;
int timeDifference = 0;

//Zmienne do cyklicznego odliczania wyjscia z MENU
bool timeToExitMenuFlag = false;
uint8_t timeToExitMenu = 0;

//Zmienne do cyklicznego odliczania wylaczenia podswietlenia LCD
uint8_t timeToTurnOffBacklight = 0;
bool timeToTurnOffBacklightFlag = false;

bool isActiveMENU = false;
bool isStartPageFlag = true;
uint8_t MenuValue = 1;
uint8_t computeOneSecondDelay = 0;  //Zmienna potrzebna do obliczenia jednej sekundy jesli petla leci co np. 200ms to licze ile razy musi sie obrucic zeby byla 1s

float tempStepMotor  = 20.0, tempStepMotorMin  = 20.0, tempStepMotorMax  = 20.0;
float tempController = 20.0, tempControllerMin = 20.0, tempControllerMax = 20.0;

//Zmienne do cyklicznego odliczania wylaczenia podswietlenia LCD
bool timeToSendDataToESPFlag = false;
uint8_t timeToSendDataToESP = 0;
bool remoteControlFlag = false;

void setup() {
  Serial.begin(115200);

  pinMode(SMC_Sleep, OUTPUT);  digitalWrite(SMC_Sleep, LOW);  //LOW - Sleep, HIGH - no Sleep
  pinMode(SMC_Step, OUTPUT);   digitalWrite(SMC_Step, LOW);
  pinMode(SMC_Dir, OUTPUT);    digitalWrite(SMC_Dir, LOW);    //LOW - Godz--, HIGH - Godz++
  pinMode(halotron_h, INPUT);  digitalWrite(halotron_h, HIGH);  //ZMienic w zaleznosci jaki sygnal bedzie aktywny. HIGH jak active LOW
  pinMode(halotron_m, INPUT);  digitalWrite(halotron_m, HIGH);
  pinMode(Int_SQW, INPUT_PULLUP);
  pinMode(Int_ClockSynchro, INPUT_PULLUP);
  pinMode(WiFiStatus, OUTPUT); digitalWrite(WiFiStatus, LOW);
  pinMode(Up, INPUT_PULLUP);
  pinMode(Down, INPUT_PULLUP);
  pinMode(Left, INPUT_PULLUP);
  pinMode(Right, INPUT_PULLUP);
  pinMode(Menu, INPUT_PULLUP);
  pinMode(positionStepPin, INPUT);  digitalWrite(positionStepPin, HIGH);
  
  //lcd.noBacklight();
  lcd.init();
  //lcd.begin();
  lcd.createChar(degCelsiusChar,  customDegCelsiusChar);
  lcd.createChar(ThermometerChar, customThermometerChar);
  lcd.createChar(UpArrowChar,     customUpArrowChar);
  lcd.createChar(DownArrowChar,   customDownArrowChar);
  
  lcd.setCursor(3,0);     //0123456789abcdef
  lcd.print("Sterownik"); //---Sterownik----
  lcd.setCursor(5,1);
  lcd.print("zegara");    //-----zegara-----
  lcd.backlight();
  delay(3000);
  clock.begin();
  clock.enable32kHz(false); //disable 32kHz
  clock.setOutput(DS3231_1HZ);  //Select 1Hz on pin SQW
  clock.enableOutput(false); //Enable 1Hz on pin SQW

  sensors.begin();
  delay(2000);
  
  //Reset clock
  digitalWrite(SMC_Sleep, HIGH);  //Wake up step motor

  tempControllerMin      = EEPROM.read(0); //EEPROM adres min temp sterownika
  tempControllerMax      = EEPROM.read(1); //EEPROM addres for 
  tempStepMotorMin       = EEPROM.read(2); //EEPROM addres for 
  tempStepMotorMax       = EEPROM.read(3); //EEPROM addres for
  useSynchroWithBellFlag = EEPROM.read(5);

  attachInterrupt(digitalPinToInterrupt(Int_SQW), Counter_SQW, FALLING);
  attachInterrupt(digitalPinToInterrupt(Int_ClockSynchro), ClockSynchro, FALLING);

  clock.enableOutput(true); //Enable 1Hz on pin SQW

  if(ResetClock(true)) {
    SetActualTime(false);
  }
  ReadTempController();
  ReadTempStepMotor();
  
  timeToTurnOffBacklight = 0;
}

void loop() {
  // send data only when you receive data:
  String odebraneDane = "";
  
  if (Serial.available() > 0) {
    odebraneDane = Serial.readStringUntil('\r');    // \r \n
    odebraneDane.remove(0, 1);  //usuwam pierwszy znak '\n' (pozostalosc w buforze)
    Serial.println("Arduino [" + odebraneDane + "]");

    if(odebraneDane == "Start") {
      remoteControlFlag = true;
      digitalWrite(WiFiStatus,HIGH);
      Serial.print("L1");
      //Serial.println("WiFi Start");

      lcd.clear();
      lcd.setCursor(5,0);       //0123456789abcdef
      lcd.print("Zdalne");      //-----Zdalne-----
      lcd.setCursor(3,1);       //0123456789abcdef
      lcd.print("sterowanie");  //---sterowanie---
      lcd.backlight();
    }
    else if(odebraneDane == "Stop") {
      remoteControlFlag = false;
      digitalWrite(WiFiStatus,LOW);
      Serial.print("L0");
      //Serial.println("WiFi Stop");

      lcd.clear();
      isStartPageFlag = true;
      timeToTurnOffBacklight = 0;
    }
    else if(odebraneDane.charAt(0) == (char)'G') {
      String Time = odebraneDane.substring(1, 3); //0123456789ABCDEF
      Hour = Time.toInt();
      Time = odebraneDane.substring(4, 6);
      Minute = Time.toInt();
      clock.setDateTime(Year, Month, Day, Hour, Minute, Second);
    }
    else if((odebraneDane) == "SetClk") {
      bool resetErrorFlag = false;
      int stepsOfMotor = 0;
      
      Serial.print("C");
      digitalWrite(SMC_Dir, HIGH); //Godz++
      while(digitalRead(halotron_h)==HIGH || digitalRead(halotron_m)==HIGH) {
        //Serial.println(stepsOfMotor);
        
        digitalWrite(SMC_Step, HIGH);
        stepsOfMotor += 1;
        delay(5);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
        digitalWrite(SMC_Step, LOW);
        delay(35);
    
        if(stepsOfMotor >= 5600) {  //rozdzielczosc(400krokow) x 14 godz
          resetErrorFlag = true;
          break;
        }
      }
      delay(5000); //Czekaj 5s (stabilizacja wskazowki)
      if(resetErrorFlag == false) {
        //set clock
        SetActualTime(true);
      }
    }
    else if((odebraneDane) == "OR") {
      digitalWrite(SMC_Dir, HIGH); //Godz++
      while(true) {
        actualStep = digitalRead(positionStepPin);
        if(actualStep == nextStep) {
          delay(300);  //Czekam az tarcza sie uspokoi
          actualStep = digitalRead(positionStepPin);
          if(actualStep == nextStep) {  //Faktycznie zmienilo sie pole to zmien stan nastepny i wyjdz
            nextStep = !actualStep;
            break;
          }
        }
        digitalWrite(SMC_Step, HIGH);
        delay(50);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
        digitalWrite(SMC_Step, LOW);
        delay(50);
      }
      digitalWrite(SMC_Dir, HIGH); //Godz++
    }
    else if((odebraneDane) == "OL") {
      digitalWrite(SMC_Dir, LOW); //Godz--
      while(true) {
        actualStep = digitalRead(positionStepPin);
        if(actualStep == nextStep) {
          delay(300);  //Czekam az tarcza sie uspokoi
          actualStep = digitalRead(positionStepPin);
          if(actualStep == nextStep) {  //Faktycznie zmienilo sie pole to zmien stan nastepny i wyjdz
            nextStep = !actualStep;
            break;
          }
        }
        digitalWrite(SMC_Step, HIGH);
        delay(50);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
        digitalWrite(SMC_Step, LOW);
        delay(50);
      }
      digitalWrite(SMC_Dir, HIGH); //Godz++
    }
    else if(odebraneDane == "R") {
      
      tempControllerMin = tempController;
      EEPROM.write(0, tempControllerMin);
      tempControllerMax = tempController;
      EEPROM.write(1, tempControllerMax);
      tempStepMotorMin  = tempStepMotor;
      EEPROM.write(2, tempStepMotorMin);
      tempStepMotorMax  = tempStepMotor;
      EEPROM.write(3, tempStepMotorMax);
      delay(50);
      Serial.print("R1");
    }
  }

  

  //Cykliczne wysylanie danych o godzinie, dacie, temp
  if(timeToSendDataToESPFlag == true && remoteControlFlag == true) {
    timeToSendDataToESPFlag = false;

    ReadTimeDate();
    ReadTempController();
    ReadTempStepMotor();

    String msgESP = GenerateMsgESPTime() + GenerateMsgESPDate();
    msgESP += GenerateMsgESPTempController() + GenerateMsgESPTempStepMotor();
    Serial.print(msgESP);
    Serial.println("");
  }
  
  if(remoteControlFlag == true) {
    delay(500);
    return;
  }

  if(timeToTurnOffBacklightFlag == true) {
    timeToTurnOffBacklightFlag = false;
    lcd.noBacklight();
  }

  if(measureTempFlag == true) {
    measureTempFlag = false;

    ReadTempController();
    ReadTempStepMotor();

    ShowTempOnLCD(8, 1, tempController, choseControllerTemp);
    ShowTempOnLCD(8, 0, tempStepMotor, choseStepMotorTemp);

    if(tempStepMotor > 80.0) {
      bool showString = false;
      
      digitalWrite(SMC_Sleep, LOW);
      lcd.clear();
      lcd.setCursor(0,0);            //0123456789abcdef
      lcd.print("Temp. silnika za"); //Temp. silnika za
      lcd.setCursor(0,1);            //0123456789abcdef
      lcd.print("wysoka!");          //wysoka!
      countSeconds = 0;
      
      while(true) {
        if(countSeconds >= 1) {
          countSeconds = 0;
          if(showString == false) {
            showString = true;
            lcd.setCursor(0,0);            //0123456789abcdef
            lcd.print("Temp. silnika za"); //Temp. silnika za
            lcd.setCursor(0,1);            //0123456789abcdef
            lcd.print("wysoka!");          //wysoka!

            ReadTempStepMotor();
            ShowTempOnLCD(8, 1, tempStepMotor, choseStepMotorTemp);
          }
          else if(showString == true) {
            showString = false;
            lcd.setCursor(0,0);            //0123456789abcdef
            lcd.print("                 "); //Temp. silnika za
            lcd.setCursor(0,1);            //0123456789abcdef
            lcd.print("        ");          //wysoka!
          }
        }
        if(tempStepMotor < 60) {
          isStartPageFlag = true;
          lcd.clear();
          digitalWrite(SMC_Sleep, HIGH);
          if(ResetClock(false)) {
            SetActualTime(false);
          }
          break;
        }
      }
    }
  }
 
  if(isStartPageFlag == true) {
    isStartPageFlag = false;
    isActiveMENU = false;
    resetTimeToExitMenu();
    lcd.noBlink();
    lcd.clear();
    ReadTimeDate();
    ShowTimeOnLCD(0, 0, false);
    ShowTempOnLCD(8, 0, tempStepMotor, choseStepMotorTemp);
    ShowTempOnLCD(8, 1, tempController, choseControllerTemp);
  }

  if( (digitalRead(Menu)==LOW) || (digitalRead(Up)==LOW) || (digitalRead(Down)==LOW) || (digitalRead(Left)==LOW) || (digitalRead(Right)==LOW) ) { lcd.backlight(); timeToTurnOffBacklight = 0; }

  if(useSynchroWithBellFlag == true) {    //Jesli chcemy sie synchronizowac
    //ReadTimeDate();
    if(synchroWithBellIsActive == true) { //sprawdzam czy wystapilo synchro z dzwonem, moze byc rozna godz
      synchroWithBellIsActive = false;
      if(Hour >= 11 && Hour < 12) { //interesuje mnie godzina 12:00 czyli zegar sie spuznia
        digitalWrite(SMC_Dir, HIGH); //Godz++
        Hour = 12;
        Minute = 0;
        Second = 0;
        ShowTimeOnLCD(0, 0, false);
        while(digitalRead(halotron_h)==HIGH || digitalRead(halotron_m)==HIGH) {
          digitalWrite(SMC_Step, HIGH);
          delay(10);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
          digitalWrite(SMC_Step, LOW);
          delay(10);
        }
        clock.setDateTime(Year, Month, Day, Hour, Minute, Second);
        minuteHasPassedFlag = 0;
        countSecondsSteps = 0;
      }
    }
    if(Hour == 11 && Minute == 59 && synchroWithBellIsActive == false) {  //Jesli godzina jest 11:59 ale nie bylo synchro, zegar idzie za szybko i musi poczekac
      if(synchroWithBellIsActive == false) {  //nie ma jeszcze 12:00 to nie przestawiaj godziny
        clock.setDateTime(Year, Month, Day, Hour, Minute, Second);
        ShowTimeOnLCD(0, 0, false);
        minuteHasPassedFlag = 0;
        return;
      }
    }
  }

  if(minuteHasPassedFlag > 0) {
    ReadTimeDate();

    String msgESP = GenerateMsgESPTime() + GenerateMsgESPDate();
    Serial.print(msgESP);
    Serial.println("");

    ShowTimeOnLCD(0, 0, false);

    for(int i = 0; i < minuteHasPassedFlag; i++) {
      while(true) {
        actualStep = digitalRead(positionStepPin);
        if(actualStep == nextStep) {
          delay(3000);  //Czekam az tarcza sie uspokoi
          actualStep = digitalRead(positionStepPin);
          if(actualStep == nextStep) {  //Faktycznie zmienilo sie pole to zmien stan nastepny i wyjdz
            nextStep = !actualStep;

            //dodaj pare krokow zeby zegar wszedl na pelne pole biale lub czarne
            for(int i = 0; i < 3; i++) {
              digitalWrite(SMC_Step, HIGH);
              delay(50);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
              digitalWrite(SMC_Step, LOW);
              delay(50);
            }
            
            break;
          }
        }
        digitalWrite(SMC_Step, HIGH);
        delay(50);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
        digitalWrite(SMC_Step, LOW);
        delay(50);
      }
    }

//    if(Hour == nextFullHour) {  //Sprawdzam czy wybila pelna godzina zeby zrobic synchro wskazowki co godz
//      nextFullHour = Hour + 1;  //ustawiam nowa godzine
//      if(nextFullHour > 23) { nextFullHour = 0; }
//      digitalWrite(SMC_Dir, HIGH); //Godz++
//      while(digitalRead(halotron_m)==HIGH) {  //Podciagam minuty do pelnej godziny
//        digitalWrite(SMC_Step, HIGH);
//        delay(10);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
//        digitalWrite(SMC_Step, LOW);
//        delay(10);
//      }
//    }

    if((Hour == 0 || Hour == 12) && Minute == 0) {  //Synchro zegara do pozycji startowej 00:00 lub 12:00
      if((digitalRead(halotron_h) == LOW && digitalRead(halotron_m) == LOW) == false) {
        int stepsOfMotor = 0;
        digitalWrite(SMC_Dir, HIGH); //Godz++
        
        while(digitalRead(halotron_h)==HIGH || digitalRead(halotron_m)==HIGH) {
          digitalWrite(SMC_Step, HIGH);
          stepsOfMotor += 1;
          delay(5);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
          digitalWrite(SMC_Step, LOW);
          delay(35);
      
          if(stepsOfMotor >= 5600) {  //rozdzielczosc(400krokow) x 14 godz
            //resetErrorFlag = true;
            break;
          }
        }
      }
    }
    
    minuteHasPassedFlag = 0;
  }
 
  if(digitalRead(Menu)==LOW) { lcd.backlight(); delay(1000); timeToTurnOffBacklight = 0; if(digitalRead(Menu)==LOW) {
    lcd.clear();
    MenuValue = 1;
    MainMenu();
  }}

  delay(200);
}

String GenerateMsgESPTime() {
  String msgESP = "G";

  if(Hour < 10) { msgESP += "0"; msgESP += String(Hour); }
  else { msgESP += String(Hour); }
  msgESP += ":";
  if(Minute < 10) { msgESP += "0"; msgESP += String(Minute); }
  else { msgESP += String(Minute); }

  return msgESP;
}
String GenerateMsgESPDate() {
  String msgESP = "D";

  if(Day < 10) { msgESP += "0"; msgESP += String(Day); }
  else { msgESP += String(Day); }
  msgESP += ".";
  if(Month < 10) { msgESP += "0"; msgESP += String(Month); }
  else { msgESP += String(Month); }
  msgESP += ".";
  msgESP += String(Year);

  return msgESP;
}
String GenerateMsgESPTempController() {
  String msgESP = "";
  
  if(tempController >= 0.0)    { msgESP += "d0" + String(tempController); }
  else                         { msgESP += "d"  + String(tempController); }
  if(tempControllerMin >= 0.0) { msgESP += "e0" + String(tempControllerMin); }
  else                         { msgESP += "e"  + String(tempControllerMin); }
  if(tempControllerMax >= 0.0) { msgESP += "f0" + String(tempControllerMax); }
  else                         { msgESP += "f"  + String(tempControllerMax); }

  return msgESP;
}
String GenerateMsgESPTempStepMotor() {
  String msgESP = "";

  if(tempStepMotor >= 0.0)     { msgESP += "a0" + String(tempStepMotor); }
  else                         { msgESP += "a"  + String(tempStepMotor); }
  if(tempStepMotorMin >= 0.0)  { msgESP += "b0" + String(tempStepMotorMin); }
  else                         { msgESP += "b"  + String(tempStepMotorMin); }
  if(tempStepMotorMax >= 0.0)  { msgESP += "c0" + String(tempStepMotorMax); }
  else                         { msgESP += "c"  + String(tempStepMotorMax); }

  return msgESP;
}

void resetTimeToExitMenu() {
  timeToExitMenu = 0;
  timeToExitMenuFlag = false;
}

void MainMenu() {
  lcd.setCursor(0, 0);
  lcd.print("MENU");
  delay(1000);
  isActiveMENU = true;
  countSeconds = 0;

  while(true) {
    if(countSeconds >= timeToExitMenuSet) {
      isStartPageFlag = true;
        Serial.println("Wyjscie z MENU");
      break;
    }
    
    if (digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Right) == LOW) { 
      ++MenuValue;
      lcd.clear();
      if (MenuValue > maxItemMenu) { MenuValue = 1; }
      countSeconds = 0;
    }}
    if (digitalRead(Left)  == LOW) { delay(waitTimePressSwitch); if (digitalRead(Left)  == LOW) { 
      --MenuValue;
      lcd.clear();
      if (MenuValue < 1) { MenuValue = maxItemMenu; }
      countSeconds = 0;
    }}

    if(MenuValue == 1) {
      ShowMenuHeader(MenuValue);
      lcd.setCursor(2, 1);      //0123456789abcdef
      lcd.print("Czas i data"); //--Czas i data---
      
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) { //Wejscie do podMenu ustawiania Godziny (następnie daty)
        Menu1();
        countSeconds = 0;
      }}
    }
    else if(MenuValue == 2) {
      ShowMenuHeader(MenuValue);
      lcd.setCursor(2, 1);       //0123456789abcdef
      lcd.print("Reset zegara"); //--Reset zegara--
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Menu) == LOW) {
        if(ResetClock(false)) {
          SetActualTime(false);
        }
        countSeconds = 0;
      }}
    }
    else if(MenuValue == 3) {
      ShowMenuHeader(MenuValue);
      lcd.setCursor(2, 1);       //0123456789abcdef
      lcd.print("Test silnika"); //--Test silnika--
      
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Menu) == LOW) {
        Menu3();
      }}
    }
    else if(MenuValue == 4) {
      ShowMenuHeader(MenuValue);
      lcd.setCursor(2, 1);      //0123456789abcdef
      lcd.print("Diagnostyka"); //--Diagnostyka
      
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Menu) == LOW) {
        Menu4();
      }}
    }
    else if(MenuValue == 5) {
      ShowMenuHeader(MenuValue);
      lcd.setCursor(5, 1);  //0123456789abcdef
      lcd.print("Opcje");   //-----Opcje------
      
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Menu) == LOW) {
        Menu5();
      }}
    }
    else if(MenuValue == maxItemMenu) {
      ShowMenuHeader(MenuValue);
      lcd.setCursor(5, 1);  //0123456789abcdef
      lcd.print("Wyjscie"); //-----Wyjscie----
      if (digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Menu) == LOW) {
          isStartPageFlag = true;
          break;
      }}
    }
    delay(200);
  }
}
  void Menu1() {  //Podmenu: "Ustaw czas", "Ustaw date", "Wyjscie"
    Serial.println("Menu1");
    bool wasPresed = false;
    int menu1Value = 1;
    lcd.clear();
    lcd.setCursor(0, 0);           //0123456789abcdef
    lcd.print("Ustaw:       1/3"); //Ustaw:       1/3
    lcd.setCursor(6, 1);
    lcd.print("Czas");             //------Czas------
    delay(1000);
    countSeconds = 0;

    while(true) {
      if(countSeconds >= timeToExitMenuSet) {
        isStartPageFlag = true;
          Serial.println("Wyjscie z MENU 1.1");
        return;
      }
      
      if (digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Right) == LOW) { 
        wasPresed = true;
        ++menu1Value;
        if (menu1Value > 3) { menu1Value = 1; }
        countSeconds = 0;
      }}
      if (digitalRead(Left)  == LOW) { delay(waitTimePressSwitch); if (digitalRead(Left)  == LOW) {
        wasPresed = true;
        --menu1Value;
        if (menu1Value < 1) { menu1Value = 3; }
        countSeconds = 0;
      }}
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
        wasPresed = true;
        switch(menu1Value) {
          case 1: Menu1_1(); break;
          case 2: Menu1_2(); break;
          case 3: lcd.clear(); return; break;
        }
        countSeconds = 0;
      }}
      if(wasPresed == true) {
        wasPresed = false;
        lcd.clear();
        switch(menu1Value) {
          case 1:
            lcd.setCursor(0, 0);           //0123456789abcdef
            lcd.print("Ustaw:       1/3"); //Ustaw:       1/3
            lcd.setCursor(6, 1); //0123456789abcdef
            lcd.print("Czas");   //------Czas------
            break;
          case 2:
            lcd.setCursor(0, 0);          //0123456789abcdef
            lcd.print("Ustaw:       2/3"); //Ustaw:       2/3
            lcd.setCursor(6, 1);          //0123456789abcdef
            lcd.print("Date");            //------Date------
            break;
          case 3:
            lcd.setCursor(0, 0);           //0123456789abcdef
            lcd.print("Ustaw:       3/3"); //Ustaw:       3/3
            lcd.setCursor(5, 1);           //0123456789abcdef
            lcd.print("Wyjscie");          //-----Wyjscie----
            break;
        }
      }
      delay(200);
    }
  }
    void Menu1_1() {  //Podmenu "Ustaw czas" - ustawianie i zapis czasu
      int cursorPosition = 1;
      bool wasPresed = false;

      ReadTimeDate();
      
      lcd.clear();
      lcd.setCursor(0, 0);      //0123456789abcdef
      lcd.print("Ustaw czas:"); //Ustaw czas:  1/2
      ShowTimeOnLCD(4, 1, true);
      lcd.blink();
      lcd.setCursor(5, 1);
      delay(1000);
      countSeconds = 0;
      
      while(true) {   //Ustawianie godziny
        if(countSeconds >= timeToExitMenuSet) {
          isStartPageFlag = true;
            Serial.println("Wyjscie z MENU 1.1.1");
          return;
        }
        
        if(digitalRead(Up) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Up) == LOW) {
          EditLocalTime(cursorPosition, true);
          wasPresed = true;
        }}
        if(digitalRead(Down) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Down) == LOW) {
          EditLocalTime(cursorPosition, false);
          wasPresed = true;
        }}
        if(digitalRead(Left) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Left) == LOW) {
          --cursorPosition;
          if(cursorPosition < 1) { cursorPosition = 1; }
          wasPresed = true;
        }}
        if(digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Right) == LOW) {
          ++cursorPosition;
          if(cursorPosition > 3) { cursorPosition = 3; }
          wasPresed = true;
        }}
        if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
          lcd.noBlink();
          lcd.clear();
          clock.setDateTime(Year, Month, Day, Hour, Minute, Second);
          lcd.setCursor(1, 0);         //0123456789abcdef
          lcd.print("Zapisano czas");  //-Zapisano czas--
          lcd.setCursor(4, 1);   //0123456789abcdef
          lcd.print("do RTC!");  //-----do RTC!----
          minuteHasPassedFlag = 0;
          countSecondsSteps = Second;
          delay(3000);
          if(ResetClock(false)) {
            SetActualTime(false);
          }
          countSeconds = 0;
          break;
        }}
        if(wasPresed == true) {
          wasPresed = false;
          ShowTimeOnLCD(4, 1, true);
          lcd.setCursor(uint8_t(5 + (cursorPosition - 1) * 3), 1);
          countSeconds = 0;
        }
        delay(200);
      }
    }
    void Menu1_2() {
      int cursorPosition = 1;
      bool wasPresed = false;
      lcd.clear();
      lcd.setCursor(0, 0);           //0123456789abcdef
      lcd.print("Ustaw date:"); //Ustaw date:  2/2
      ShowDateOnLCD(3, 1);
      lcd.blink();
      lcd.setCursor(4, 1);           //---01.01.2019---
      delay(1000);
      
      while(true) { //Ustawianie daty
        if(timeToExitMenu >= timeToExitMenuSet) {  //Automatyczne wyjscie z MENU
          timeToExitMenu = 0;
          timeToExitMenuFlag = true;
          isStartPageFlag = true;
            Serial.println("Wyjscie z MENU 1.1.2");
          return;
        }
        
        if(digitalRead(Up) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Up) == LOW) {
          EditLocalDate(cursorPosition, true);
          wasPresed = true;
        }}
        if(digitalRead(Down) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Down) == LOW) {
          EditLocalDate(cursorPosition, false);
          wasPresed = true;
        }}
        if(digitalRead(Left) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Left) == LOW) {
          --cursorPosition;
          if(cursorPosition < 1) { cursorPosition = 1; }
          wasPresed = true;
        }}
        if(digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Right) == LOW) {
          ++cursorPosition;
          if(cursorPosition > 3) { cursorPosition = 3; }
          wasPresed = true;
        }}
        if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
          resetTimeToExitMenu();
//              lcd.noCursor();
          lcd.noBlink();
          lcd.clear();
          // Manual (YYYY, MM, DD, HH, II, SS
          clock.setDateTime(Year, Month, Day, Hour, Minute, Second);
          lcd.setCursor(1, 0);         //0123456789abcdef
          lcd.print("Zapisano date");  //-Zapisano czas--
          lcd.setCursor(5, 1);   //0123456789abcdef
          lcd.print("do RTC!");  //-----do RTC!----
          delay(3000);
          lcd.clear();
          return;
        }}
        if(wasPresed == true) {
          wasPresed = false;
          ShowDateOnLCD(3, 1);
          if(cursorPosition == 1) { lcd.setCursor(4, 1); }
          if(cursorPosition == 2) { lcd.setCursor(7, 1); }
          if(cursorPosition == 3) { lcd.setCursor(12, 1); }
          resetTimeToExitMenu();
        }
        delay(200);
      }
    }
  void Menu3() {
    bool setDir = false;

    digitalWrite(SMC_Sleep, HIGH); //Run SMC
    
    lcd.clear();
    lcd.setCursor(2, 0);        //0123456789abcdef
    lcd.print("Test silnika");  //--Test silnika--
    lcd.setCursor(7, 1);                                          //0123456789abcdef
    lcd.print("|" + String(!digitalRead(positionStepPin)) + "|"); // <-  h |x| m  ->, gdzie x 0-biale, 1-czarne pole tarczy
    delay(1000);
    
    while(true) {
      if(countSeconds >= timeToExitMenuSet) {
        isStartPageFlag = true;
          Serial.println("Wyjscie z MENU 1.1");
        return true;
      }

      lcd.setCursor(7, 1);           //0123456789abcdef
      lcd.print("|" + String(!digitalRead(positionStepPin)) + "|");
      
      if(digitalRead(Left) == LOW) {
        lcd.setCursor(1, 1); //0123456789abcdef
        lcd.print("<-");     // <-

        digitalWrite(SMC_Dir, LOW);
        digitalWrite(SMC_Step, HIGH);
        delay(10);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
        digitalWrite(SMC_Step, LOW);
        countSeconds = 0;
      }
      else {
        lcd.setCursor(1, 1); //0123456789abcdef
        lcd.print("  ");     //
      }
      if(digitalRead(Right) == LOW) {
        lcd.setCursor(14, 1); //0123456789abcdef
        lcd.print("->");      //              ->

        digitalWrite(SMC_Dir, HIGH);
        digitalWrite(SMC_Step, HIGH);
        delay(10);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
        digitalWrite(SMC_Step, LOW);
        countSeconds = 0;
      }
      else {
        lcd.setCursor(14, 1); //0123456789abcdef
        lcd.print("  ");      //
      }
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
        if(ResetClock(false)) {
          SetActualTime(false);
        }
        countSeconds = 0;
        lcd.clear();
        break;
      }}

      if(digitalRead(halotron_h) == LOW) {
        lcd.setCursor(5, 1); //0123456789abcdef
        lcd.print("h");      // <-  h |x| m  -> 
      }
      else {
        lcd.setCursor(5, 1); //0123456789abcdef
        lcd.print(" ");      // <-  h |x| m  -> 
      }
      
      if(digitalRead(halotron_m) == LOW) {
        lcd.setCursor(11, 1); //0123456789abcdef
        lcd.print("m");       // <-  h |x| m  -> 
      }
      else {
        lcd.setCursor(11, 1); //0123456789abcdef
        lcd.print(" ");       // <-  h |x| m  -> 
      }
    }
  }
  bool Menu4() {
    bool waitOnce = true;
    bool wasPresed = true;
    int menu4Value = 1;
    lcd.clear();
    countSeconds = 0;

    while(true) {
      if(countSeconds >= timeToExitMenuSet) {
        isStartPageFlag = true;
          Serial.println("Wyjscie z MENU 4");
        return true;  //Wyjscie przez timeout
      }
      
      if(wasPresed == true) {
        wasPresed = false;
        lcd.clear();
        switch(menu4Value) {
          case 1:
            lcd.setCursor(0, 0);           //0123456789abcdef
            lcd.print("Diagnostyka: 1/2"); //Diagnostyka: 1/2
            lcd.setCursor(2, 1);
            lcd.print("Temperatura");      //--Temperatura---
            break;
          case 2:
            lcd.setCursor(0, 0);           //0123456789abcdef
            lcd.print("Diagnostyka: 2/2"); //Diagnostyka: 2/2
            lcd.setCursor(5, 1);           //0123456789abcdef
            lcd.print("Wyjscie");          //-----Wyjscie----
            break;
        }
      }

      if(waitOnce) { delay(1000); waitOnce = false; } //Czekaj chwile po wejsciu do podmenu

      if (digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Right) == LOW) { 
        wasPresed = true;
        ++menu4Value;
        if (menu4Value > 2) { menu4Value = 1; }
        countSeconds = 0;
      }}
      if (digitalRead(Left)  == LOW) { delay(waitTimePressSwitch); if (digitalRead(Left)  == LOW) {
        wasPresed = true;
        --menu4Value;
        if (menu4Value < 1) { menu4Value = 2; }
        countSeconds = 0;
      }}
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
        wasPresed = true;
        switch(menu4Value) {
          case 1:
            if(Menu4_1()) {
              return true;  //Wyjscie przez timeout
            }
            countSeconds = 0;
            break;
          case 2:
            lcd.clear();
            countSeconds = 0;
            return false; //Wyjscie przez uzytkownika
            break;
        }
      }}
      
      delay(200);
    }
  }
    bool Menu4_1() {
      bool waitOnce = true;
      bool wasPresed = true;
      int menu4_1Value = 1;
      
      lcd.clear();
      countSeconds = 0;
  
      while(true) {
        if(countSeconds >= timeToExitMenuSet) {
          isStartPageFlag = true;
            Serial.println("Wyjscie z MENU 1.1");
          return true;  //Wyjscie przez timeout
        }
        
        if(wasPresed == true) {
          wasPresed = false;
          lcd.clear();
          switch(menu4_1Value) {
            case 1:
              lcd.setCursor(0, 0);           //0123456789abcdef
              lcd.print("Temperatura: 1/4"); //Diagnostyka: 1/2
              lcd.setCursor(4, 1);
              lcd.print("Sterownik");        //----Sterownik---
              break;
            case 2:
              lcd.setCursor(0, 0);           //0123456789abcdef
              lcd.print("Temperatura: 2/4"); //Temperatura: 2/4
              lcd.setCursor(5, 1);
              lcd.print("Silnik");           //-----Silnik-----
              break;
            case 3:
              lcd.setCursor(0, 0);           //0123456789abcdef
              lcd.print("Temperatura: 3/4"); //Temperatura: 3/4
              lcd.setCursor(5, 1);
              lcd.print("Reset");            //-----Reset------
              break;
            case 4:
              lcd.setCursor(0, 0);           //0123456789abcdef
              lcd.print("Temperatura: 4/4"); //Temperatura: 4/4
              lcd.setCursor(5, 1);           //0123456789abcdef
              lcd.print("Wyjscie");          //-----Wyjscie----
              break;
          }
        }

        if(waitOnce) { delay(1000); waitOnce = false; } //Czekaj chwile po wejsciu do podmenu

        if (digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Right) == LOW) { 
          wasPresed = true;
          ++menu4_1Value;
          if (menu4_1Value > 4) { menu4_1Value = 1; }
          countSeconds = 0;
        }}
        if (digitalRead(Left)  == LOW) { delay(waitTimePressSwitch); if (digitalRead(Left)  == LOW) {
          wasPresed = true;
          --menu4_1Value;
          if (menu4_1Value < 1) { menu4_1Value = 4; }
          countSeconds = 0;
        }}
        if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
          wasPresed = true;
          switch(menu4_1Value) {
            case 1 ... 3:
            //case 2:
            //case 3:
              if(Menu4_1_1(menu4_1Value)) {
                return true;  //Wyjscie przez timeout
              }
              countSeconds = 0;
              break;
            case 4:
              lcd.clear();
              countSeconds = 0;
              return false; //Wyjscie przez uzytkownika
              break;
          }
        }}
        
        delay(200);
      }
    }
      bool Menu4_1_1(int menuValue) {
        bool waitOnce = true;
        lcd.clear();
        countSeconds = 0;
        
        while(true) {
          if(countSeconds >= timeToExitMenuSet) {
            isStartPageFlag = true;
              Serial.println("Wyjscie z MENU 4.1.1");
            return true;  //Wyjscie przez timeout
          }
          
          switch(menuValue) {
            case 1:
              ReadTempController();
            
              lcd.setCursor(0, 0);      //0123456789abcdef
              lcd.print("Sterownik:");  //Sterownik:-23.5C
              if(tempController < 0) { lcd.setCursor(10, 0); } else { lcd.setCursor(11, 0); }
              lcd.print(tempController, 1);
              lcd.setCursor(15, 0);
              lcd.write(degCelsiusChar);
              
              lcd.setCursor(0, 1);
              lcd.write(DownArrowChar);
              if(tempControllerMin < 0) { lcd.setCursor(1, 1); } else { lcd.setCursor(2, 1); }
              lcd.print(tempControllerMin, 1);
              lcd.setCursor(6, 1);
              lcd.write(degCelsiusChar);
              
              lcd.setCursor(9, 1);
              lcd.write(UpArrowChar);
              if(tempControllerMax < 0) { lcd.setCursor(10, 1); } else { lcd.setCursor(11, 1); }
              lcd.print(tempControllerMax, 1);
              lcd.setCursor(15, 1);
              lcd.write(degCelsiusChar);    //_-23.4C  _-23.4C      
              break;
            case 2:
              ReadTempStepMotor();
            
              lcd.setCursor(0, 0);    //0123456789abcdef
              lcd.print("Silnik:");   //Silnik:   -23.5C
              if(tempStepMotor < 0) { lcd.setCursor(10, 0); } else { lcd.setCursor(11, 0); }
              lcd.print(tempStepMotor, 1);
              lcd.setCursor(15, 0);
              lcd.write(degCelsiusChar);
              
              lcd.setCursor(0, 1);
              lcd.write(DownArrowChar);
              if(tempStepMotorMin < 0) { lcd.setCursor(1, 1); } else { lcd.setCursor(2, 1); }
              lcd.print(tempStepMotorMin, 1);
              lcd.setCursor(6, 1);
              lcd.write(degCelsiusChar);
              
              lcd.setCursor(9, 1);
              lcd.write(UpArrowChar);
              if(tempStepMotorMax < 0) { lcd.setCursor(10, 1); } else { lcd.setCursor(11, 1); }
              lcd.print(tempStepMotorMax, 1);
              lcd.setCursor(15, 1);
              lcd.write(degCelsiusChar);    //_-23.4C  _-23.4C    
              break;
            case 3:
              tempControllerMin = tempController;
              EEPROM.write(0, tempControllerMin);
              tempControllerMax = tempController;
              EEPROM.write(1, tempControllerMax);
              tempStepMotorMin  = tempStepMotor;
              EEPROM.write(2, tempStepMotorMin);
              tempStepMotorMax  = tempStepMotor;
              EEPROM.write(3, tempStepMotorMax);
              lcd.setCursor(1, 0);         //0123456789abcdef
              lcd.print("Temp. Min, Max"); //-Temp. Min, Max-
              lcd.setCursor(3, 1);
              lcd.print("zresetowna");     //---zresetowna---
              delay(3000);
              countSeconds = 0;
              return false; //Wyjscie przez uzytkownika
              break;
          }

          if(waitOnce) { delay(1000); waitOnce = false; } //Czekaj chwile po wejsciu do podmenu
          
          if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
            lcd.clear();
            countSeconds = 0;
            return false; //Wyjscie przez uzytkownika
          }}

          delay(200);
        }
      }
  bool Menu5() {
    bool wasPresed = false;
    int menu5Value = 1;
    lcd.clear();
    lcd.setCursor(0, 0);           //0123456789abcdef
    lcd.print("Opcje:       1/2"); //Opcje:       1/3
    lcd.setCursor(0, 1);
    lcd.print("Synch. z dzwonem");             //Synch. z zegarem
    delay(1000);
    countSeconds = 0;

    while(true) {
      if(countSeconds >= timeToExitMenuSet) {
        isStartPageFlag = true;
        return;
      }
      
      if (digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Right) == LOW) { 
        wasPresed = true;
        ++menu5Value;
        if (menu5Value > 2) { menu5Value = 1; }
        countSeconds = 0;
      }}
      if (digitalRead(Left)  == LOW) { delay(waitTimePressSwitch); if (digitalRead(Left)  == LOW) {
        wasPresed = true;
        --menu5Value;
        if (menu5Value < 1) { menu5Value = 2; }
        countSeconds = 0;
      }}
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
        wasPresed = true;
        switch(menu5Value) {
          case 1: Menu5_2(); break;
          case 2: lcd.clear(); return; break;
        }
        countSeconds = 0;
      }}
      if(wasPresed == true) {
        wasPresed = false;
        lcd.clear();
        switch(menu5Value) {
          case 1:
            lcd.setCursor(0, 0);          //0123456789abcdef
            lcd.print("Opcje:       2/3"); //Ustaw:       2/3
            lcd.setCursor(0, 1);          //0123456789abcdef
            lcd.print("Synch. z dzwonem");            //------Date------
            break;
          case 2:
            lcd.setCursor(0, 0);           //0123456789abcdef
            lcd.print("Opcje:       3/3"); //Ustaw:       3/3
            lcd.setCursor(5, 1);           //0123456789abcdef
            lcd.print("Wyjscie");          //-----Wyjscie----
            break;
        }
      }
      delay(200);
    }    
  }
    void Menu5_2() {
      lcd.clear();
      lcd.setCursor(0,0);                   //0123456789abcdef
      lcd.print("Synch. z dzwonem");        //Synch. z zegarem
      lcd.setCursor(0, 1);
      if(useSynchroWithBellFlag == false) {                     
        lcd.print("Tak       -> Nie");      //Tak       -> Nie
      }
      else {
        lcd.print("Tak <-       Nie");      //Tak <-       Nie
      }
      countSeconds = 0;
      delay(1000);
      
      while(true) {      
        if(digitalRead(Left) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Left) == LOW) {
          useSynchroWithBellFlag = true;
          countSeconds = 0;
          lcd.setCursor(4, 1);   //0123456789abcdef
          lcd.print("<-      "); //Tak <-       Nie
        }}
        if(digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Right) == LOW) {
          useSynchroWithBellFlag = false;
          countSeconds = 0;
          lcd.setCursor(4, 1);   //0123456789abcdef
          lcd.print("      ->"); //Tak       -> Nie
        }}
        if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
          lcd.setCursor(0, 1);
          lcd.print("                ");
          break;
        }}
      }
   
      EEPROM.write(5, useSynchroWithBellFlag);
      
      lcd.setCursor(0,1);           //0123456789abcdef
      lcd.print("   Zapisano!    "); //   Zapisano!    
      delay(3000);
      lcd.clear();
    }

void ReadTempController() {
  tempController = clock.readTemperature();
  if(tempController < tempControllerMin) {
    tempControllerMin = tempController;
    EEPROM.write(0, tempControllerMin);
  }
  if(tempController > tempControllerMax) {
    tempControllerMax = tempController;
    EEPROM.write(1, tempControllerMax);
  }
}
void ReadTempStepMotor() {
  sensors.requestTemperatures();
  delay(20);
  tempStepMotor = sensors.getTempCByIndex(0);
  if(tempStepMotor < tempStepMotorMin) {
    tempStepMotorMin = tempStepMotor;
    EEPROM.write(2, tempStepMotorMin);
  }
  if(tempStepMotor > tempStepMotorMax) {
    tempStepMotorMax = tempStepMotor;
    EEPROM.write(3, tempStepMotorMax);
  }  
}

void EditLocalTime(int cursorPosition, bool isUp) {
  switch(cursorPosition) {
    case 1:
      if(isUp == true) { ++Hour; }
      else { --Hour; }

      if (Hour > 23) { Hour = 0; }
      if (Hour < 0) { Hour = 23; }
    break;
    case 2:
      if(isUp == true) { ++Minute; }
      else { --Minute; }

      if (Minute > 59) { Minute = 0; }
      if (Minute < 0) { Minute = 59; }
    break;
    case 3:
      if(isUp == true) { ++Second; }
      else { --Second; }

      if (Second > 59) { Second = 0; }
      if (Second < 0) { Second = 59; }
    break;
  }
}

int computeMaxDaysOfMonth(int month) {
  int maxDay = 31;
  if(month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) { maxDay = 31; }
  if(month == 4 || month == 6 || month == 9 || month == 11) { maxDay = 30; }
  if(month == 2) { maxDay = 29; }
  return maxDay;
}

void EditLocalDate(int cursorPosition, bool isUp) {
  int maxDay = 31;
  switch(cursorPosition) {
    case 1:
      if(isUp == true) { ++Day; }
      else { --Day; }
      maxDay = computeMaxDaysOfMonth(Month);
      if (Day > maxDay) { Day = 1; }
      if (Day < 1) { Day = maxDay; }
      break;
    case 2:
      if(isUp == true) { ++Month; }
      else { --Month; }
      if (Month > 12) { Month = 1; }
      if (Month < 1) { Month = 12; }
      maxDay = computeMaxDaysOfMonth(Month);
      if(Day > maxDay) { Day = maxDay; }
      break;
    case 3:
      if(isUp == true) { ++Year; }
      else { --Year; }
      if (Year > 2200) { Year = 2200; }
      if (Year < 1993) { Year = 1993; }
      break;
  }
}

void ReadTimeDate() {
  dt = clock.getDateTime();

  Hour = dt.hour; Minute = dt.minute; Second = dt.second;
  Year = dt.year; Month = dt.month; Day = dt.day;
}

void turnStepMotorBySteps(int steps, bool dir) {  // steps - ile krokow, dir - 0-zgodnie z ruchem wskazowek, 1-przeciwnie
  digitalWrite(SMC_Dir, dir);  //Godz ++
  digitalWrite(SMC_Sleep, HIGH);

  for(int i = 0; i < steps; i++) {
    digitalWrite(SMC_Step, HIGH);
    delay(10);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
    digitalWrite(SMC_Step, LOW);
    delay(10);
  }
}


uint8_t hour12(uint8_t hour24) {
  if (hour24 >= 12) {
    return (hour24 - 12);
  }
  return hour24;
}

void SetActualTime(bool isConnectedWiFi) {
  bool chosenValue = true;
  int pressTime = 9;
  uint8_t convertedHour = 0;
  int imp = 0;
  bool showString = false;

  if(isConnectedWiFi == false) {
    lcd.clear();
    lcd.setCursor(2,0);       //0123456789abcdef
    lcd.print("Ustaw zegar"); //--Ustaw zegar---
    lcd.setCursor(0, 1);
    lcd.print("Tak <- " + String(pressTime) + "s    Nie"); //Tak <- 9s -> Nie
    delay(1000);
  }
  
  countSeconds = 0;
  if(isConnectedWiFi == false) {
    while(true) {
      if(digitalRead(Left) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Left) == LOW) {
        chosenValue = true;
        countSeconds = 0;
        lcd.setCursor(4, 1);   //0123456789abcdef
        lcd.print("<-      "); //Tak <-       Nie
      }}
      if(digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Right) == LOW) {
        chosenValue = false;
        countSeconds = 0;
        lcd.setCursor(4, 1);   //0123456789abcdef
        lcd.print("      ->"); //Tak       -> Nie
      }}
      if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
        lcd.setCursor(0, 1);
        lcd.print("                ");
        break;
      }}
  
      lcd.setCursor(7, 1);
      lcd.print(String(pressTime - countSeconds) + "s");
  
      if(countSeconds >= pressTime) {
        break;
      }
      delay(200);
    }
  }

  if(isConnectedWiFi == true) {
    chosenValue = true;
  }
  
  if(chosenValue == true) {
    ReadTimeDate(); //Odczyt godziny z RTC

    nextFullHour = Hour + 1;  //ustawiam kolejna pelna godzine
    if(nextFullHour > 23) { nextFullHour = 0; }
    
    Second = 0;
    if(isConnectedWiFi == false) {  //nie wyswietlam jesli podlaczony do WiFi
      lcd.clear();
      ShowTimeOnLCD(0, 0, true);
    }
    Hour = 0; Minute = 0;
    if(dt.hour <= 6) { Hour = 0; }  //Obrot w prawo
    else if( (dt.hour > 6) && (dt.hour < 12) ) { Hour = 11; } //Obrot w lewo
    else if( (dt.hour >= 12) && (dt.hour <= 18) ) { Hour = 12; } //Obrot w prawo
    else if(dt.hour > 18) { Hour = 23; } //Obrot w lewo
    
    //if(dt.hour >= 12) { Hour = 12; }
    if(isConnectedWiFi == false) {
      ShowTimeOnLCD(0, 1, true);
    }
      
    convertedHour = hour12(dt.hour);
    //Serial.print("Godz ");
    //Serial.println(convertedHour);

    //Nowa funkcja
    if(convertedHour <= 6) {
      uint8_t count = 0;
      
      bool lastMinuteFlag = false;
      //Serial.println("Ustawiam godzine");
      lastMinuteFlag = !digitalRead(halotron_m);  //Aktywne 0, czyli neguje zeby bylo 1 jak jest pelna godz

      digitalWrite(SMC_Dir, HIGH);  //Godz ++
      countSeconds = 0;
      
      for(int i = 0; i < convertedHour; i++) {  //Ustawiam zgrubnie godziny
        //Serial.println("Ustawiam godz " + String(i+1));
        bool isHour = false;
        
        while(true) {
          bool actualMinuteFlag = !digitalRead(halotron_m);
          if(lastMinuteFlag == false && actualMinuteFlag == true) { //spelnienie warunku zbocza narastajacego
            isHour = true;
            //Serial.println("Teraz godz");
          }
          lastMinuteFlag = actualMinuteFlag;  //Aktywne 0, czyli neguje zeby bylo 1 jak jest pelna godz

          if(isHour == true) {
            //Serial.println("Teraz godz wyjscie");
            break;
          }
          
          digitalWrite(SMC_Step, HIGH);
          delay(10);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
          digitalWrite(SMC_Step, LOW);
          delay(10);

          if(isConnectedWiFi == false) {
            ShowTimeOnLCD(0, 1, true);
        
            if(countSeconds >= 1) {
              countSeconds = 0;
              lcd.setCursor(9,1);     //0123456789abcdef
              if(showString == false) {
                showString = true;       
                lcd.print("Czekaj!"); //---------Czekaj!
              }
              else if(showString == true) {
                showString = false;
                lcd.print("       "); //---------_______
              }
            }
          }
        }
        Hour += 1;
        if(isConnectedWiFi == false) {
          ShowTimeOnLCD(0, 1, true);
        }
      }
      //Serial.println("Ustawiam minute");
      
      delay(3000);
      nextStep = !digitalRead(positionStepPin);
      
      for(int i = 0; i < dt.minute; i++) {  //Ustawiam minuty
        while(true) {
          actualStep = digitalRead(positionStepPin);
          if(actualStep == nextStep) {
            delay(100);  //Czekam az tarcza sie uspokoi
            actualStep = digitalRead(positionStepPin);
            if(actualStep == nextStep) {  //Faktycznie zmienilo sie pole to zmien stan nastepny i wyjdz
              nextStep = !actualStep;
              break;
            }
          }
          digitalWrite(SMC_Step, HIGH);
          delay(50);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
          digitalWrite(SMC_Step, LOW);
          delay(50);

          if(isConnectedWiFi == false) {
            ShowTimeOnLCD(0, 1, true);
        
            if(countSeconds >= 1) {
              countSeconds = 0;
              lcd.setCursor(9,1);     //0123456789abcdef
              if(showString == false) {
                showString = true;       
                lcd.print("Czekaj!"); //---------Czekaj!
              }
              else if(showString == true) {
                showString = false;
                lcd.print("       "); //---------_______
              }
            }
          }
        }
        Minute += 1;
        if(isConnectedWiFi == false) {
          ShowTimeOnLCD(0, 1, true);
        }
      }
    }
    else {
      uint8_t count = 0;
      
      bool lastMinuteFlag = false;
      //Serial.println("Ustawiam godzine");
      lastMinuteFlag = !digitalRead(halotron_m);  //Aktywne 0, czyli neguje zeby bylo 1 jak jest pelna godz

      digitalWrite(SMC_Dir, LOW);  //Godz ++
      countSeconds = 0;
      
      for(int i = 0; i < (12 - convertedHour - 1); i++) {  //Ustawiam zgrubnie godziny
        //Serial.println("Ustawiam godz " + String(i+1));
        bool isHour = false;
        
        while(true) {
          bool actualMinuteFlag = !digitalRead(halotron_m);
          if(lastMinuteFlag == false && actualMinuteFlag == true) { //spelnienie warunku zbocza narastajacego
            isHour = true;
            //Serial.println("Teraz godz");
          }
          lastMinuteFlag = actualMinuteFlag;  //Aktywne 0, czyli neguje zeby bylo 1 jak jest pelna godz

          if(isHour == true) {
            //Serial.println("Teraz godz wyjscie");
            break;
          }
          
          digitalWrite(SMC_Step, HIGH);
          delay(10);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
          digitalWrite(SMC_Step, LOW);
          delay(10);

          if(isConnectedWiFi == false) {
            ShowTimeOnLCD(0, 1, true);
        
            if(countSeconds >= 1) {
              countSeconds = 0;
              lcd.setCursor(9,1);     //0123456789abcdef
              if(showString == false) {
                showString = true;       
                lcd.print("Czekaj!"); //---------Czekaj!
              }
              else if(showString == true) {
                showString = false;
                lcd.print("       "); //---------_______
              }
            }
          }
        }
        Hour -= 1;
        if(Hour < 0) { Hour = 23; }
        if(isConnectedWiFi == false) {
          ShowTimeOnLCD(0, 1, true);
        }
      }
      //Serial.println("Ustawiam minute");
      
      delay(2000);
      nextStep = !digitalRead(positionStepPin);
      
      for(int i = 0; i < (60 - dt.minute); i++) {  //Ustawiam minuty
        while(true) {
          actualStep = digitalRead(positionStepPin);
          if(actualStep == nextStep) {
            delay(100);  //Czekam az tarcza sie uspokoi
            actualStep = digitalRead(positionStepPin);
            if(actualStep == nextStep) {  //Faktycznie zmienilo sie pole to zmien stan nastepny i wyjdz
              nextStep = !actualStep;
              break;
            }
          }
          digitalWrite(SMC_Step, HIGH);
          delay(50);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
          digitalWrite(SMC_Step, LOW);
          delay(50);

          if(isConnectedWiFi == false) {
            ShowTimeOnLCD(0, 1, true);
        
            if(countSeconds >= 1) {
              countSeconds = 0;
              lcd.setCursor(9,1);     //0123456789abcdef
              if(showString == false) {
                showString = true;       
                lcd.print("Czekaj!"); //---------Czekaj!
              }
              else if(showString == true) {
                showString = false;
                lcd.print("       "); //---------_______
              }
            }
          }
        }
        Minute -= 1;
        if(Minute < 0) { Minute = 59; }
        if(isConnectedWiFi == false) {
          ShowTimeOnLCD(0, 1, true);
        }
      }
    }
    //END

    
    //imp = (400 * convertedHour) + ((dt.minute * 7) - floor(dt.minute / 3.0));

    //digitalWrite(SMC_Sleep, LOW);
    if(isConnectedWiFi == false) {
      lcd.setCursor(9,1);   //0123456789abcdef
      lcd.print("Gotowe!"); //---------Gotowe!
      delay(3000);
      lcd.clear();
    }
  }
  else {
    lcd.setCursor(0,1);            //0123456789abcdef
    lcd.print("   Anulowano!   "); //   Anulowano!   
    delay(3000);
    lcd.clear();
  }

  //aktualna pozycja
  actualStep = digitalRead(positionStepPin);
  nextStep = !actualStep;

  timeToTurnOffBacklight = 0;
  timeToTurnOffBacklightFlag = false;
  digitalWrite(SMC_Dir, HIGH);  //Godz ++
}

bool ResetClockToDefaultPosition(bool isFirstReset) {
  int stepsOfMotor = 0;
  bool showString = false;
  
  digitalWrite(SMC_Sleep, HIGH);
  digitalWrite(SMC_Dir, HIGH);  //Godz ++
  countSeconds = 0;

  lcd.setCursor(0,1);
  lcd.print("                ");
  delay(3000);
  
  while(digitalRead(halotron_h)==HIGH || digitalRead(halotron_m)==HIGH) {
    //Serial.println(stepsOfMotor);
    
    digitalWrite(SMC_Step, HIGH);
    stepsOfMotor += 1;
    delay(5);        //Mozna regulowac predkosc obrotu walu silnika (timeStep)
    digitalWrite(SMC_Step, LOW);
    delay(35);

    if(stepsOfMotor >= 5600) {  //rozdzielczosc(400krokow) x 14 godz
      digitalWrite(SMC_Sleep, LOW); //Sleep SMC to save energy

      if(isFirstReset == true) {
        lcd.clear();
        lcd.setCursor(5,0);         //0123456789abcdef
        lcd.print("Error!");        //-----Error!-----
        lcd.setCursor(0,1);         
        lcd.print("Code 1");       //Zbyt duza liczba krokow silnika podczas resetowania zegara
        while(true) {}
      }
      else {
        lcd.clear();
        lcd.setCursor(0,0);         //0123456789abcdef
        lcd.print("Error: Code 1"); //Error: Code 1---
        lcd.setCursor(2,1);         //0123456789abcdef
        lcd.print("Nacisnij OK");   //--Nacisnij OK---

        while(true) {
          if (digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if (digitalRead(Menu) == LOW) {
            lcd.clear();
            return false;
          }} 
        }
      }
    }

    if(countSeconds >= 1) {
      countSeconds = 0;
      lcd.setCursor(4,1);
      if(showString == false) {
        showString = true;       
        lcd.print("Czekaj!");       //----Czekaj!-----
      }
      else if(showString == true) {
        showString = false;
        lcd.print("       ");       //----_______-----
      }
    }
  }
  return true;
}

bool ResetClock(bool isFirstReset) {
  bool chosenValue = true;
  int pressTime = 9;

  lcd.clear();
  lcd.setCursor(2,0);           //0123456789abcdef
  lcd.print("Reset zegara");    //--Reset zegara---
  lcd.setCursor(0, 1);
  lcd.print("Tak <- " + String(pressTime) + "s    Nie"); //Tak <- 9s -> Nie
  countSeconds = 0;
  delay(1000);

  while(true) {
    if(digitalRead(Left) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Left) == LOW) {
      chosenValue = true;
      countSeconds = 0;
      lcd.setCursor(4, 1);   //0123456789abcdef
      lcd.print("<-      "); //Tak <-       Nie
    }}
    if(digitalRead(Right) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Right) == LOW) {
      chosenValue = false;
      countSeconds = 0;
      lcd.setCursor(4, 1);   //0123456789abcdef
      lcd.print("      ->"); //Tak       -> Nie
    }}
    if(digitalRead(Menu) == LOW) { delay(waitTimePressSwitch); if(digitalRead(Menu) == LOW) {
      lcd.setCursor(0, 1);
      lcd.print("                ");
      break;
    }}

    lcd.setCursor(7, 1);
    lcd.print(String(pressTime - countSeconds) + "s");

    if(countSeconds >= pressTime) {
      break;
    }
    delay(200);
  }
  if(chosenValue == true) {
    chosenValue = ResetClockToDefaultPosition(isFirstReset);
    if(chosenValue) {
      lcd.setCursor(0,1);             //0123456789abcdef
      lcd.print("    Gotowe!     ");  //    Gotowe!  
    }
    else {
      lcd.setCursor(0,1);             //0123456789abcdef
      lcd.print(" Niepowodzenie! ");  // Niepowodzenie! 
    }
    delay(3000);
    lcd.clear();
  }
  else {
    lcd.setCursor(0,1);            //0123456789abcdef
    lcd.print("   Anulowano!   "); //   Anulowano!   
    delay(3000);
    lcd.clear();
  }

  actualStep = digitalRead(positionStepPin);  //Zapamietuje nowy stan tarczy
  nextStep = !actualStep;

  return chosenValue;
}

void ClockSynchro() {
  synchroWithBellIsActive = true;
}

void Counter_SQW() {
  ++countSeconds;
  ++countSecondsSteps;
  ++countSecondsTemp;
  ++timeToExitMenu;
  ++timeToTurnOffBacklight;
  ++timeToSendDataToESP;
  
  if(countSecondsSteps >= 60) {
    ++minuteHasPassedFlag;
    countSecondsSteps = 0;
  }

  if(countSecondsTemp >= timeToMeasureTemps) {  //30
    measureTempFlag = true;
    countSecondsTemp = 0;
  }

  if(isActiveMENU == false) { timeToExitMenu = 0; }         //Jesli NIE jestem w MENU to nie odliczam czasu do wyjscia z MENU
  if(isActiveMENU == true) { timeToTurnOffBacklight = 0; }  //Jesli jestem w MENU to nie odliczam czasu na zgaszanie podswietlania
  
  if(timeToExitMenu >= timeToExitMenuSet) {
    timeToExitMenuFlag = true;
    timeToExitMenu = 0;
    //Serial.println("Exit MENU");
  }

  if(timeToTurnOffBacklight >= timeToTurnOffBacklightSet) {
    timeToTurnOffBacklightFlag = true;
    timeToTurnOffBacklight = 0;
  }

  if(timeToSendDataToESP >= timeToSendDataToESPSet) {
    timeToSendDataToESPFlag = true;
    timeToSendDataToESP = 0;
  }
}

void ShowTimeOnLCD(int col, int row, bool showSeconds) {  // (Kolumna, Wiersz)
  String stringTime = "";

  if(Hour < 10)   { stringTime += "0"; }
  stringTime += String(Hour) + ":";
  if(Minute < 10) { stringTime += "0"; }
  stringTime += String(Minute);
  if(showSeconds == true) {
    stringTime += ":";
    if(Second < 10) { stringTime += "0"; }
    stringTime += String(Second);
  }

  lcd.setCursor(col, row);
  lcd.print(stringTime);   //00:00:00
}
void ShowDateOnLCD(int col, int row) {
  String stringDate = "";

  if(Day < 10)   { stringDate += "0"; }
  stringDate += String(Day) + ".";
  if(Month < 10) { stringDate += "0"; }
  stringDate += String(Month) + ".";
  stringDate += String(Year);

  lcd.setCursor(col, row);
  lcd.print(stringDate);   //01.01.2000
}
void ShowTempOnLCD(int col, int row, float temp, int whatTemp) {  // s - silnik, k - kontroler
  lcd.setCursor(col, row);    //0123456789abcdef
  lcd.write(ThermometerChar); //00:00   Ts 24,2C
  lcd.setCursor((col+1), row);
  if(whatTemp == 0) { lcd.print("s "); }
  if(whatTemp == 1) { lcd.print("k "); }
  lcd.setCursor((col+3), row);
  lcd.print(temp,1);
  lcd.setCursor((col+7), row);
  lcd.write(degCelsiusChar);
}
void ShowMenuHeader(int MenuVal) {
  lcd.setCursor(0, 0);                                                      //0123456789abcdef
  lcd.print("MENU         " + String(MenuVal) + "/" + String(maxItemMenu)); //MENU         1/4
}


//END
