#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#define BUFF_MAX 100

//Definiowanie komend
#define getTimeDateCommand  A
#define getTimeCommand      B
#define getDateCommand      C
#define getTemps            D
#define getTempSilnika      E
#define getTempSterownika   F

struct structTimeDate {
  int hour;
  int minute;
  int day;
  int month;
  int year;
};
struct structTemp {
  float Tactual;
  float Tmin;
  float Tmax;
};

enum possibleMenuItems {
  startMenuPage,
  setTimeMenuPage,
  setDateMenuPage
};

//SSID and Password to your ESP Access Point
const char* ssid     = "Zegar (192.168.4.1)";
const char* password = "";  //otwarta siec

const char* passwordLogin = "xxxxxxxxxxx";
bool isSigned = false;

//Flagi do wysłania danych do Sterownika
bool receiveFromControllerFlag = false;
bool sendToControllerFlag = false;
bool loginToControllerFlag = false;

bool sendLoginToControllerFlag = false;
bool receivedLoginToControllerFlag = true;

bool sendSetClockFlag = false;
bool receivedSetClockFlag = true;

bool sendSetDateFlag = false;
bool receivedSetDateFlag = true;

bool sendRotateOnceLeft = false;
bool sendRotateOnceRight = false;
bool sendRotateContinuousLeft = false;
bool sendRotateContinuousRight = false;
bool sendStopRotate = false;

bool sendResetMinMaxTemp = false;   //Wyslij polecenie resetowania min max temp


//END

ESP8266WebServer server(80); //Server on port 80

structTimeDate timeDateClock  = {0, 0, 1, 1, 2000};   //Godzina pokazywana na tarczy zegara
structTimeDate timeDateActual = {0, 0, 1, 1, 2000};   //Godzina w ukadzie RTC
structTimeDate timeDateHelp   = {0, 0, 0, 0, 0};        //Godzina pomocnicza, uzywana podczas zmiany czasu lub daty, zeby nie zmieniacz godzin aktualnych zegara

structTemp tempSilnika    = {20.0, 20.0, 20.0};          //Temperatura silnika
structTemp tempSterownika = {20.0, 20.0, 20.0};         //Temperatura wewnatrz sterownika

possibleMenuItems menuItems = startMenuPage;

int statusSetDate = 0;          // 0 - Czekaj, 1 - Gotowe, 2 - Error
bool toggleStatusName = false;  //false - napis nie widoczny, true - napis widoczny

String convertTimeToString(int hour, int minute) {          //Konwersja godziny do ciagu znakow
  String timeToString = "";
  
  if(hour   < 10) { timeToString += "0" + String(hour);    }
  else            { timeToString += String(hour);          }
  if(minute < 10) { timeToString += ":0" + String(minute); }
  else            { timeToString += ":"  + String(minute); }

  return timeToString;
}
String convertDateToString(int day, int month, int year) {  //Konwersja daty do ciagu znakow 
  String dateToString = "";
  
  if(day   < 10) { dateToString += "0" + String(day); }
  else           { dateToString += String(day); }
  if(month < 10) { dateToString += ".0" + String(month); }
  else           { dateToString += "."  + String(month); }
  dateToString += "." + String(year);

  return dateToString;
}
String convertTempToString(float temp) {                    //Konwersja temperatury do ciagu znakow
  return String(temp);
}

int computeMaxDaysOfMonth(int month) {
  int maxDay = 31;

  if(month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) {
    maxDay = 31;
  }
  if(month == 4 || month == 6 || month == 9 || month == 11) {
    maxDay = 30;
  }
  if(month == 2) {
    maxDay = 29;
  }

  return maxDay;
}

String getDataFromSerial() {
  String data = "";
  char inBuffer[BUFF_MAX];
  
  if(Serial.available() > 0) {
    byte sizeBuffer = Serial.readBytesUntil('\n', inBuffer, BUFF_MAX);
    //incomingByte = Serial.read();
  
    // say what you got:
    Serial.print("I received: ");
    Serial.write(inBuffer);
    data = String(inBuffer);
  }
  return data;
}

String generateStartPage(bool isWrongPassword) {  //Generowanie strony startowej lub info o zlym logowaniu
  String html = "";

  html += "<!DOCTYPE html>";
  html +=   "<html>";
  html +=     "<head>"; 
  html +=       "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=       "<title>Zegar - Strona startowa</title>";
  html +=       "<style>";
  html +=         "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=         "h1      { font-size: 20px; }";
  html +=         "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=         "h3      { font-size: 40px; margin: 10px;}";
  html +=         ".button { background-color: #4CAF50; border: none; color: white; padding: 10px 30px;";
  html +=                   "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=       "</style>";
  html +=     "</head>";
  html +=     "<body>";
  html +=       "<h1>Parafia pw. Matki Boskiej Częstochowskiej<br>w Solcu Nowym</h1>";
  html +=       "<h2>ZEGAR MECHANICZNY </h2>";
  
  html +=       "<h3>" + convertTimeToString(timeDateClock.hour, timeDateClock.minute) + "</h3>";
  html +=       "<h3>" + convertDateToString(timeDateClock.day, timeDateClock.month, timeDateClock.year) + "</h3>";
  html +=       "<h3><br></h3>";
  html +=       "<table align=center>";
  html +=         "<tr>";
  html +=           "<td align=center colspan=\"2\">Wpisz hasło, aby się zalogować</td>";
  html +=         "</tr>";
  html +=         "<tr>";
  html +=           "<td valign=middle>";
  html +=             "<form action=\"/L\" method=\"post\">";
  html +=               "<input type=\"password\" name=\"myPsw\" id=\"myPsw\">";
  html +=               "<input type=\"submit\" value=\"Zaloguj\" class=\"button\">";
  html +=             "</form>";
  html +=           "</td>";
  html +=         "</tr>";
  html +=       "<table>";
  html +=     "</body>";
  if(isWrongPassword == true) {
    html +=   "<p style=\"color:red;\"><b>Błędne hasło!</b></p>";
    server.arg("myPsw") == "";
    sendLoginToControllerFlag = true;
    loginToControllerFlag = false;
  }
  else {
    html +=   "<p><br></p>";
  }
  html += "<p><br></p>";
  html += "<p style=\"font-size: 10px\">Autor: mgr inż. Mateusz</p>";
  html +=   "</html>";
  
  return html;
}

void handleRoot() { //Strona startowa
  sendLoginToControllerFlag = true;
  loginToControllerFlag = false;

  sendRotateOnceLeft = false;
  sendRotateOnceRight = false;
  sendRotateContinuousLeft = false;
  sendRotateContinuousRight = false;
  sendStopRotate = true;
  
  server.arg("myPsw") == "";
  isSigned = false;
  
  //disablePagesServer();
  
  String html = generateStartPage(false);
  server.send(200, "text/html", html);
}

void handleL() {    //Strona po zalogowaniu, opcje: Ustawienia, Diagnostyka lub Wylogowanie
  sendLoginToControllerFlag = true;
  loginToControllerFlag = true;
  
  Serial.print("Haslo: ");
  Serial.println(server.arg("myPsw"));
  
  String html = "";

  if(server.arg("myPsw") == passwordLogin) { isSigned = true; }
  
  if(isSigned == true) {
    //enablePagesServer();
    
    html += "<!DOCTYPE html>";
    html +=   "<html>";
    html +=     "<head>";
    html +=       "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
    html +=       "<title>Zegar - MENU</title>";
    html +=       "<style>";
    html +=         "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
    html +=         "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
    html +=         "h3      { font-size: 20px; margin: 10px;}";
    html +=         ".button { width: 200px; height: 40px; background-color: #4CAF50; border: none; color: white;";
    html +=                   "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
    html +=       "</style>";
    html +=     "</head>";
    
    html +=     "<body>";
    html +=       "<h2>ZEGAR MECHANICZNY</h2>";
    html +=       "<p></p>";
    html +=       "<form action=\"/U\" method=\"post\">";
    html +=         "<input type=\"submit\" value=\"Ustawienia\" class=\"button\">";
    html +=       "</form>";
    html +=       "<p></p>";
    html +=       "<form action=\"/D\" method=\"post\">";
    html +=         "<input type=\"submit\" value=\"Diagnostyka\" class=\"button\">";
    html +=       "</form>";
    html +=       "<p></p>";
    html +=       "<form action=\"/\" method=\"post\">";
    html +=         "<input type=\"submit\" value=\"Wyloguj\" class=\"button\" style=\"width: 100px;\">";
    html +=       "</form>";
    html +=     "</body>";
    html +=   "</html>";
  }
  else{
    //disablePagesServer();
    html = "";
    html = generateStartPage(true);   
  }
  server.send(200, "text/html", html); //Send web page
}

void handleU() {    //Strona Ustawień
  String html = "";
  
  html += "<!DOCTYPE html>";
  html += "<html>";
  html +=   "<head>";
  html +=     "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=     "<title>Zegar - Ustawienia</title>";
  html +=     "<style>";
  html +=       "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=       "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=       ".button { width: 200px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                 "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=     "</style>";
  html +=   "</head>";
  
  html +=   "<body>";
  html +=     "<h2>ZEGAR MECHANICZNY</h2>";
  html +=     "<p></p>";
  html +=     "<form action=\"/U1\" method=\"post\">";
  html +=       "<input type=\"submit\" value=\"Ustaw godzinę\" class=\"button\">";
  html +=     "</form>";
  html +=     "<p></p>";
  html +=     "<form action=\"/U2\" method=\"post\">";
  html +=       "<input type=\"submit\" value=\"Ustaw datę\" class=\"button\">";
  html +=     "</form>";
  html +=     "<p></p>";
//  html +=     "<form action=\"/U3\" method=\"post\">";
//  html +=       "<input type=\"submit\" value=\"Zresetuj zegar\" class=\"button\">";
//  html +=     "</form>";
//  html +=     "<p></p>";
  html +=     "<form action=\"/U4\" method=\"post\">";
  html +=       "<input type=\"submit\" value=\"Test silnika\" class=\"button\">";
  html +=     "</form>";
  html +=     "<p></p>";
  html +=     "<form action=\"/L\" method=\"post\">";
  html +=       "<input type=\"submit\" value=\"Powrót\" class=\"button\" style=\"width: 100px;\">";
  html +=     "</form>";
  html +=   "</body>";
  html += "</html>";
  
  server.send(200, "text/html", html);

  timeDateHelp = timeDateClock;
  sendRotateOnceLeft = false;
  sendRotateOnceRight = false;
  sendRotateContinuousLeft = false;
  sendRotateContinuousRight = false;
  sendStopRotate = true;
}

void handleU1() {   //Strona ustawiania czasu
  String html = "";
  
  html += "<!DOCTYPE html>";
  html += "<html>";
  html +=   "<head>";
  html +=     "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=     "<title>Zegar - Ustaw godzinę</title>";
  html +=     "<style>";
  html +=       "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=       "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=       "h3      { font-size: 40px; margin: 10px;}";
  html +=       ".button { width: 70px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                 "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=       "table, th, td {border: 0px solid black; border-collapse: collapse; padding: 10px 10px;}";
  html +=     "</style>";
  html +=   "</head>";
     
  html +=   "<body>";
  html +=     "<h2>ZEGAR MECHANICZNY</h2>";
  html +=     "<p></p>";
  html +=     "<table align=center>";
  html +=       "<tr>";
  html +=         "<td align=center colspan=\"3\"><b>Ustaw godzinę</b></td>";
  html +=       "</tr>";
  html +=       "<tr>";
  html +=         "<td align=center colspan=\"3\"><h3>" + convertTimeToString(timeDateHelp.hour, timeDateHelp.minute) + "</h3></td>";
  html +=       "</tr>";
  html +=       "<tr>";
  html +=         "<td>";
  html +=           "<table style=\"border: 2px solid black;\">";
  html +=             "<tr>";
  html +=               "<td>";
  html +=                 "<form action=\"/U1Gu\" method=\"post\">";
  html +=                   "<input type=\"submit\" value=\"&#x25B2;\" class=\"button\" style=\"width: 40px;\">";
  html +=                 "</form>";
  html +=               "</td>";
  html +=             "</tr>";
  html +=             "<tr>";
  html +=               "<td>";
  html +=                 "<form action=\"/U1Gd\" method=\"post\">";
  html +=                   "<input type=\"submit\" value=\"&#x25BC;\" class=\"button\" style=\"width: 40px;\">";
  html +=                 "</form>";
  html +=               "</td>";
  html +=             "</tr>";
  html +=           "</table>";
  html +=         "</td>";
  html +=         "<td style=\"width: 10px;\"></td>";
  html +=         "<td>";
  html +=           "<table style=\"border: 2px solid black;\">";
  html +=             "<tr>";
  html +=               "<td>";
  html +=                 "<form action=\"/U1Mu\" method=\"post\">";
  html +=                   "<input type=\"submit\" value=\"&#x25B2;\" class=\"button\" style=\"width: 40px;\">";
  html +=                 "</form>";
  html +=               "</td>";
  html +=             "</tr>";
  html +=             "<tr>";
  html +=               "<td>";
  html +=                 "<form action=\"/U1Md\" method=\"post\">";
  html +=                   "<input type=\"submit\" value=\"&#x25BC;\" class=\"button\" style=\"width: 40px;\">";
  html +=                 "</form>";
  html +=               "</td>";
  html +=             "</tr>";
  html +=           "</table>";
  html +=         "</td>";
  html +=       "</tr>";
  html +=     "</table>";
  html +=     "<p></p>";
  html +=     "<table align=center>";
  html +=       "<tr>";
  html +=         "<td>";
  html +=           "<form action=\"/U1Ok\" method=\"post\">";
  html +=             "<input type=\"submit\" value=\"OK\" class=\"button\" style=\"width: 100px;\">";
  html +=           "</form>";
  html +=         "</td>";
  html +=         "<td style=\"width: 40px;\"></td>";
  html +=         "<td>";
  html +=           "<form action=\"/U\" method=\"post\">";
  html +=             "<input type=\"submit\" value=\"Anuluj\" class=\"button\" style=\"width: 100px;\">";
  html +=           "</form>";
  html +=         "</td>";
  html +=       "</tr>";
  html +=     "</table>";
  html +=   "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}
void handleU1Gu() { //Zwiekszanie godziny
  timeDateHelp.hour += 1;
  if(timeDateHelp.hour > 23) { timeDateHelp.hour = 0; }
  handleU1();
}
void handleU1Gd() { //Zmniejszania godziny
  timeDateHelp.hour -= 1;
  if(timeDateHelp.hour < 0) { timeDateHelp.hour = 23; }
  handleU1();
}
void handleU1Mu() { //Zwiekszanie minuty
  timeDateHelp.minute += 1;
  if(timeDateHelp.minute > 59) { timeDateHelp.minute = 0; }
  handleU1();
}
void handleU1Md() { //Zmniejszanie minuty
  timeDateHelp.minute -= 1;
  if(timeDateHelp.minute < 0) { timeDateHelp.minute = 59; }
  handleU1();
}
void handleU1Ok() { //Zatwierdzenie ustawienia czasu
  String html = "";

  sendSetClockFlag = true;
  
  html += "<!DOCTYPE html>";
  html += "<html>";
  html +=   "<head>";
  html +=     "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=     "<title>Zegar - Ustaw godzinę</title>";
  html +=     "<style>";
  html +=       "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=       "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=       "h3      { font-size: 40px; margin: 10px;}";
  html +=       ".button { width: 70px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                 "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=       "span  { color: #606060; width: 70px; height: 40px;}";
  html +=       "table, th, td {border: 0px solid black; border-collapse: collapse; padding: 10px 10px;}";
  html +=     "</style>";
  html +=   "</head>";

  html +=   "<body>";
  html +=     "<h2>ZEGAR MECHANICZNY</h2>";
  html +=     "<p></p>";
  html +=     "<p style=\"color: red;\"><b>Procedura ustawienia godziny:</b></p>";
  html +=     "<p><b style=\"color: red;\">1.</b> Reset zegara do godziny 12:00</p>";
  html +=     "<p>(wskazanie referencyjne).</p>";
  html +=     "<p></p>";
  html +=     "<p><b style=\"color: red;\">2.</b> Przerwa 5 sekund.</p>";
  html +=     "<p></p>";
  html +=     "<p><b style=\"color: red;\">3.</b> Ustawienie właściwej godziny.</p>";
  html +=     "<p><br></p>";
  html +=     "<p style=\"color: red;\">Jeśli procedura ustawienia godziny została zakończona, kliknij przycisk 'OK'.</p>";
  html +=     "<p><br></p>";
  html +=     "<form action=\"/U\" method=\"post\">";
  html +=       "<input type=\"submit\" value=\"OK\" class=\"button\" style=\"width: 100px;\">";
  html +=     "</form>";
  html +=   "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void handleU2() {   //Strona ustawiania daty
  String html = "";
  
  html += "<!DOCTYPE html>";
  html += "<html>";
  html +=   "<head>";
  html +=     "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=     "<title>Zegar - Ustaw datę</title>";
  html +=     "<style>";
  html +=       "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=       "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=       "h3      { font-size: 40px; margin: 10px;}";
  html +=       ".button { width: 70px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                 "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=       "table, th, td {border: 0px solid black; border-collapse: collapse; padding: 10px 10px;}";
  html +=     "</style>";
  html +=   "</head>";

  html +=   "<body>";
  html +=   "<h2>ZEGAR MECHANICZNY</h2>";
  html +=   "<p></p>";
  html +=   "<table align=center>";
  html +=     "<tr>";
  html +=       "<td align=center colspan=\"5\"><b>Ustaw datę</b></td>";
  html +=     "</tr>";
  html +=     "<tr>";
  html +=       "<td align=center colspan=\"5\"><h3>" + convertDateToString(timeDateHelp.day, timeDateHelp.month, timeDateHelp.year) + "</h3></td>";
  html +=     "</tr>";
  html +=     "<tr>";
  html +=       "<td>";
  html +=         "<table style=\"border: 2px solid black;\">";
  html +=           "<tr>";
  html +=             "<td>";
  html +=               "<form action=\"/U2Du\" method=\"post\">";
  html +=                 "<input type=\"submit\" value=\"&#x25B2;\" class=\"button\" style=\"width: 40px;\">";
  html +=               "</form>";
  html +=             "</td>";
  html +=           "</tr>";
  html +=           "<tr>";
  html +=             "<td>";
  html +=               "<form action=\"/U2Dd\" method=\"post\">";
  html +=                 "<input type=\"submit\" value=\"&#x25BC;\" class=\"button\" style=\"width: 40px;\">";
  html +=               "</form>";
  html +=             "</td>";
  html +=           "</tr>";
  html +=         "</table>";
  html +=       "</td>";
  html +=       "<td style=\"width: 10px;\"></td>";
  html +=       "<td>";
  html +=         "<table style=\"border: 2px solid black;\">";
  html +=           "<tr>";
  html +=             "<td>";
  html +=               "<form action=\"/U2Mu\" method=\"post\">";
  html +=                 "<input type=\"submit\" value=\"&#x25B2;\" class=\"button\" style=\"width: 40px;\">";
  html +=               "</form>";
  html +=             "</td>";
  html +=           "</tr>";
  html +=           "<tr>";
  html +=             "<td>";
  html +=               "<form action=\"/U2Md\" method=\"post\">";
  html +=                 "<input type=\"submit\" value=\"&#x25BC;\" class=\"button\" style=\"width: 40px;\">";
  html +=               "</form>";
  html +=             "</td>";
  html +=           "</tr>";
  html +=         "</table>";
  html +=       "</td>";
  html +=       "<td style=\"width: 10px;\"></td>";
  html +=       "<td>";
  html +=         "<table style=\"border: 2px solid black;\">";
  html +=           "<tr>";
  html +=             "<td>";
  html +=               "<form action=\"/U2Yu\" method=\"post\">";
  html +=                 "<input type=\"submit\" value=\"&#x25B2;\" class=\"button\" style=\"width: 40px;\">";
  html +=               "</form>";
  html +=             "</td>";
  html +=           "</tr>";
  html +=           "<tr>";
  html +=             "<td>";
  html +=               "<form action=\"/U2Yd\" method=\"post\">";
  html +=                 "<input type=\"submit\" value=\"&#x25BC;\" class=\"button\" style=\"width: 40px;\">";
  html +=               "</form>";
  html +=             "</td>";
  html +=           "</tr>";
  html +=         "</table>";
  html +=       "</td>";
  html +=     "</tr>";
  html +=   "</table>";
  html +=   "<p></p>";
  html +=   "<table align=center>";
  html +=     "<tr>";
  html +=       "<td>";
  html +=         "<form action=\"/U2Ok\" method=\"post\">";
  html +=           "<input type=\"submit\" value=\"OK\" class=\"button\" style=\"width: 100px;\">";
  html +=         "</form>";
  html +=       "</td>";
  html +=       "<td style=\"width: 40px;\"></td>";
  html +=       "<td>";
  html +=         "<form action=\"/U\" method=\"post\">";
  html +=           "<input type=\"submit\" value=\"Anuluj\" class=\"button\" style=\"width: 100px;\">";
  html +=         "</form>";
  html +=       "</td>";
  html +=     "</tr>";
  html +=   "</table>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}
void handleU2Du() { //Zwiekszanie dnia
  int maxDay = 31;
  timeDateHelp.day += 1;
  maxDay = computeMaxDaysOfMonth(timeDateHelp.month);
  if(timeDateHelp.day > maxDay) { timeDateHelp.day = 1; }
  handleU2();
}
void handleU2Dd() { //Zmniejszania dnia
  int maxDay = 31;
  timeDateHelp.day -= 1;
  maxDay = computeMaxDaysOfMonth(timeDateHelp.month);
  if(timeDateHelp.day < 1) { timeDateHelp.day = maxDay; }
  handleU2();
}
void handleU2Mu() { //Zwiekszanie miesiaca
  int maxDay = 31;
  timeDateHelp.month += 1;
  maxDay = computeMaxDaysOfMonth(timeDateHelp.month);   //Czy to nie powinno byc linie nizej?
  if(timeDateHelp.month > 12) { timeDateHelp.month = 1; }
  if(timeDateHelp.day > maxDay) { timeDateHelp.day = maxDay; }
  handleU2();
}
void handleU2Md() { //Zmniejszania miesiaca
  int maxDay = 31;
  timeDateHelp.month -= 1;
  maxDay = computeMaxDaysOfMonth(timeDateHelp.month);   //Czy to nie powinno byc linie nizej?
  if(timeDateHelp.month < 1) { timeDateHelp.month = 12; }
  if(timeDateHelp.day > maxDay) { timeDateHelp.day = maxDay; }
  handleU2();
}
void handleU2Yu() { //Zwiekszanie roku
  timeDateHelp.year += 1;
  if(timeDateHelp.year > 2200) { timeDateHelp.year = 2200; }
  handleU2();
}
void handleU2Yd() { //Zmniejszania roku
  timeDateHelp.year -= 1;
  if(timeDateHelp.year < 1993) { timeDateHelp.year = 1993; }
  handleU2();
}
void handleU2Ok() { //Zatwierdzenie ustawienia daty
  String html = "";
  
  sendSetDateFlag = true;

  html += "<!DOCTYPE html>";
  html += "<html>";
  html +=   "<head>";
  html +=     "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=     "<title>Zegar - Ustaw datę</title>";
  html +=     "<style>";
  html +=       "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=       "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=       "h3      { font-size: 40px; margin: 10px;}";
  html +=       ".button { width: 70px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                 "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=       "span  { color: #606060; width: 70px; height: 40px;}";
  html +=       "table, th, td {border: 0px solid black; border-collapse: collapse; padding: 10px 10px;}";
  html +=     "</style>";
  html +=   "</head>";

  html +=   "<body>";
  html +=     "<h2>ZEGAR MECHANICZNY</h2>";
  html +=     "<p></p>";
  html +=     "<p>Data została ustawiona, kliknij przycisk 'OK'.</p>";
  html +=     "<p><br></p>";
  html +=     "<form action=\"/U\" method=\"post\">";
  html +=       "<input type=\"submit\" value=\"OK\" class=\"button\" style=\"width: 100px;\">";
  html +=     "</form>";
  html +=   "</body>";
  html += "</html>";
  
  server.send(200, "text/html", html);
}

void handleU3() {
  String html = "";
  
  html += "<!DOCTYPE html>";
  html += "<html>";
  html +=   "<head>";
  html +=     "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=     "<title>Zegar - Zresetuj zegar</title>";
  html +=     "<style>";
  html +=       "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=       "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=       "h3      { font-size: 40px; margin: 10px;}";
  html +=       ".button { width: 100px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                 "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=       "table, th, td {border: 0px solid black; border-collapse: collapse; padding: 10px 10px;}";
  html +=     "</style>";
  html +=   "</head>";

  html +=   "<body>";
  html +=     "<h2>ZEGAR MECHANICZNY</h2>";
  html +=     "<p></p>";
  html +=     "<p style=\"color: red;\"><b>Czy na pewno zresetować zegar mechaniczny</b></p>";
  html +=     "<p style=\"color: red;\"><b>do godziny 00:00?</b></p>";
  html +=     "<p></p>";
  html +=     "<table align=center>";
  html +=       "<tr>";
  html +=         "<td>";
  html +=           "<form action=\"/U3Ok\" method=\"post\">";
  html +=             "<input type=\"submit\" value=\"OK\" class=\"button\">";
  html +=           "</form>";
  html +=         "</td>";
  html +=         "<td style=\"width: 40px;\"></td>";
  html +=         "<td>";
  html +=           "<form action=\"/U\" method=\"post\">";
  html +=             "<input type=\"submit\" value=\"Anuluj\" class=\"button\">";
  html +=           "</form>";
  html +=         "</td>";
  html +=       "</tr>";
  html +=     "</table>";
  html +=   "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

String generateTestMotorPage() {
  String html = "";
  
  html += "<!DOCTYPE html>";
  html += "<html>";
  html +=   "<head>";
  html +=     "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=     "<title>Zegar - Test silnika</title>";
  html +=     "<style>";
  html +=       "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=       "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=       "h3      { font-size: 40px; margin: 10px;}";
  html +=       ".button { width: 40px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                 "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=       "table, th, td {border: 0px solid black; border-collapse: collapse; padding: 10px 10px;}";
  html +=     "</style>";
  html +=   "</head>";

  html +=   "<body>";
  html +=     "<h2>ZEGAR MECHANICZNY</h2>";
  html +=     "<p></p>";
  html +=     "<table align=center>";
  html +=       "<tr>";
  html +=         "<td align=center style=\"padding: 20px 10px;\"><b>Test silnika<br></b></td>";
  html +=       "</tr>";
  
  html +=       "<tr>";
  html +=         "<td align=center style=\"padding: 1px 1px;\"><b>Obrót silnika o 1 minutę</b></td>";
  html +=       "</tr>";
  html +=       "<tr>";
  html +=         "<td>";
  html +=           "<table style=\"border: 2px solid black;\">";
  html +=             "<tr>";
  html +=               "<td>";
  html +=                 "<form action=\"/U4SL\" method=\"post\">";
  html +=                   "<input type=\"submit\" value=\"<\" class=\"button\">";
  html +=                 "</form>";
  html +=               "</td>";
  html +=               "<td style=\"width: 10px;\"></td>";
  html +=               "<td style=\"width: 64px;\"></td>";
  html +=               "<td style=\"width: 10px;\"></td>";
  html +=               "<td>";
  html +=                 "<form action=\"/U4SR\" method=\"post\">";
  html +=                   "<input type=\"submit\" value=\">\" class=\"button\">";
  html +=                 "</form>";
  html +=               "</td>";
  html +=             "</tr>";
  html +=           "</table>";
  html +=         "</td>";
  html +=       "</tr>";
  
//  html +=       "<tr>";
//  html +=         "<td align=center style=\"padding: 1px 1px;\"><b>Obrót silnika ciągły</b></td>";
//  html +=       "</tr>";
//  html +=       "<tr>";
//  html +=         "<td>";
//  html +=           "<table style=\"border: 2px solid black;\">";
//  html +=             "<tr>";
//  html +=               "<td>";
//  html +=                 "<form action=\"/U4CL\" method=\"post\">";
//  if(sendRotateContinuousLeft == false) { html += "<input type=\"submit\" value=\"&#x22D8;\" class=\"button\" style=\"background-color: #4CAF50;\">"; }
//  else                                  { html += "<input type=\"submit\" value=\"&#x22D8;\" class=\"button\" style=\"background-color: #214D22;\">"; }
//  html +=                 "</form>";
//  html +=               "</td>";
//  html +=               "<td style=\"width: 10px;\"></td>";
//  html +=               "<td>";
//  html +=                 "<form action=\"/U4Stop\" method=\"post\">";
//  if(sendStopRotate == false) { html += "<input type=\"submit\" value=\"Stop\" class=\"button\" style=\"width: 60px; background-color: #4CAF50;\">"; }
//  else                        { html += "<input type=\"submit\" value=\"Stop\" class=\"button\" style=\"width: 60px; background-color: #214D22;\">"; }
//  html +=                 "</form>";
//  html +=               "</td>";
//  html +=               "<td style=\"width: 10px;\"></td>";
//  html +=               "<td>";
//  html +=                 "<form action=\"/U4CR\" method=\"post\">";
//  if(sendRotateContinuousRight == false) { html += "<input type=\"submit\" value=\"&#x22D9;\" class=\"button\" style=\"background-color: #4CAF50;\">"; }
//  else                                   { html += "<input type=\"submit\" value=\"&#x22D9;\" class=\"button\" style=\"background-color: #214D22;\">"; }
//  html +=                 "</form>";
//  html +=               "</td>";
//  html +=             "</tr>";
//  html +=           "</table>";
//  html +=         "</td>";
//  html +=       "</tr>";
  
  html +=     "</table>";
  html +=     "<p></p>";
  html +=     "<form action=\"/U\" method=\"post\">";
  html +=       "<input type=\"submit\" value=\"Powrót\" class=\"button\" style=\"width: 100px;\">";
  html +=     "</form>";
  html +=   "</body>";
  html += "</html>";

  return html;
}
void handleU4() {
  String html = generateTestMotorPage();
  server.send(200, "text/html", html);
}
void handleU4SL() {
  sendRotateOnceLeft = true;
  sendRotateOnceRight = false;
  sendRotateContinuousLeft = false;
  sendRotateContinuousRight = false;
  sendStopRotate = false;
  handleU4();
}
void handleU4SR() {
  sendRotateOnceLeft = false;
  sendRotateOnceRight = true;
  sendRotateContinuousLeft = false;
  sendRotateContinuousRight = false;
  sendStopRotate = false;
  handleU4();
}
void handleU4CL() {
  sendRotateOnceLeft = false;
  sendRotateOnceRight = false;
  sendRotateContinuousLeft = true;
  sendRotateContinuousRight = false;
  sendStopRotate = false;
  handleU4();
}
void handleU4Stop() {
  sendRotateOnceLeft = false;
  sendRotateOnceRight = false;
  sendRotateContinuousLeft = false;
  sendRotateContinuousRight = false;
  sendStopRotate = true;
  handleU4();
}
void handleU4CR() {
  sendRotateOnceLeft = false;
  sendRotateOnceRight = false;
  sendRotateContinuousLeft = false;
  sendRotateContinuousRight = true;
  sendStopRotate = false;
  handleU4();
}

void handleD() {
  String html = "";
  
  html += "<!DOCTYPE html>";
  html +=   "<html>";
  html +=     "<head>";
  html +=       "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> <link rel=\"icon\" href=\"data:,\">";
  html +=       "<title>Zegar - Diagnostyka</title>";
  html +=       "<style>";
  html +=         "html    { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}";
  html +=         "h2      { font-size: 30px; text-decoration: underline double #4CAF50;}";
  html +=         ".button { width: 200px; height: 40px; background-color: #4CAF50; border: none; color: white;";
  html +=                   "text-decoration: none; font-size: 15px; margin: 2px; cursor: pointer;}";
  html +=         ".buttonClear { width: 80px; height: 15px; background-color: #4CAF50; border: none; color: white; padding: 1px 1px;";
  html +=                   "text-decoration: none; font-size: 10px; margin: 0px; cursor: pointer; }";
  html +=         "table, th, td {border-collapse: collapse; padding: 3px 10px;}";
  html +=       "</style>";
  html +=     "</head>";
  
  html +=     "<body> <h2>ZEGAR MECHANICZNY</h2>";
  html +=       "<table align=center>";
  html +=         "<tr>";
  html +=           "<td align=center colspan=\"4\"><b>Diagnostyka</b></td>";
  html +=         "</tr>";
//  html +=         "<tr>";
//  html +=           "<td align=left valign=middle style=\"border: 2px solid black;\"><b>Czas aktualny:</b></td>";
//  html +=           "<td colspan=\"3\" style=\"border: 2px solid black;\">" + convertTimeToString(timeDateActual.hour, timeDateActual.minute) + "</td>";
//  html +=         "</tr>";
  html +=         "<tr>";
  html +=           "<td align=left valign=middle style=\"border: 2px solid black;\"><b>Czas zegara:</b></td>";
  html +=           "<td colspan=\"3\" style=\"border: 2px solid black;\">" + convertTimeToString(timeDateClock.hour, timeDateClock.minute) + "</td>";
  html +=         "</tr>";
//  html +=         "<tr>";
//  html +=           "<td align=left valign=middle style=\"border: 2px solid black;\"><b>Różnica czasu:</b></td>";
//  html +=           "<td colspan=\"3\" style=\"border: 2px solid black;\">+2 minuty</td>";
//  html +=         "</tr>";
  html +=         "<tr>";
  html +=           "<td align=left valign=middle style=\"border: 2px solid black;\"><b>Temperatura:</b></td>";
  html +=           "<td style=\"border: 2px solid black;\"><b>aktualna</b></td>";
  html +=           "<td style=\"border: 2px solid black;\"><b>min</b></td>";
  html +=           "<td style=\"border: 2px solid black;\"><b>max</b></td>";
  html +=         "</tr>";  
  html +=         "<tr>";
  html +=           "<td align=left valign=middle style=\"border: 2px solid black;\"><b>Silnik:</b></td>";
  html +=           "<td style=\"border: 2px solid black;\">" + convertTempToString(tempSilnika.Tactual) + " &#x2103;</td>";
  html +=           "<td style=\"border: 2px solid black;\">" + convertTempToString(tempSilnika.Tmin)    + " &#x2103;</td>";
  html +=           "<td style=\"border: 2px solid black;\">" + convertTempToString(tempSilnika.Tmax)    + " &#x2103;</td>";
  html +=         "</tr>";
  html +=         "<tr>";
  html +=           "<td align=left valign=middle style=\"border: 2px solid black;\"><b>Sterownik:</b></td>";
  html +=           "<td style=\"border: 2px solid black;\">" + convertTempToString(tempSterownika.Tactual) + " &#x2103;</td>";
  html +=           "<td style=\"border: 2px solid black;\">" + convertTempToString(tempSterownika.Tmin)    + " &#x2103;</td>";
  html +=           "<td style=\"border: 2px solid black;\">" + convertTempToString(tempSterownika.Tmax)    + " &#x2103;</td>";
  html +=         "</tr>";
  html +=         "<tr>";
  html +=           "<td align=center colspan=\"2\"></td>";
  html +=           "<td align=center colspan=\"2\" style=\"border: 2px solid black;\">";
  html +=             "<form action=\"/Dc\" method=\"post\">";
  html +=               "<input type=\"submit\" value=\"Reset min/max\" class=\"buttonClear\">";
  html +=             "</form>";
  html +=           "</td>";
  html +=         "</tr>";
  html +=       "</table>";
  html +=       "<p></p>";
  html +=       "<form action=\"/L\" method=\"post\">";
  html +=         "<input type=\"submit\" value=\"Powrót\" class=\"button\" style=\"width: 100px;\">";
  html +=       "</form>";
  html +=     "</body>";
  html +=   "</html>";
  
  server.send(200, "text/html", html);
}
void handleDc() {
  sendResetMinMaxTemp = true;
  
  tempSilnika.Tmin = tempSilnika.Tactual;
  tempSilnika.Tmax = tempSilnika.Tactual;

  tempSterownika.Tmin = tempSterownika.Tactual;
  tempSterownika.Tmax = tempSterownika.Tactual;

  handleD();
}

void handleNotFound(){
  sendLoginToControllerFlag = true;
  loginToControllerFlag = false;
  
  server.arg("myPsw") == "";
  isSigned = false;
  
  String html = generateStartPage(false);
  server.send(200, "text/html", html);
  
  //server.send(404, "text/plain", "404: Not found");
}

//===============================================================
//                  SETUP
//===============================================================

void enablePagesServer() {
  server.on("/",   handleRoot); //Strona glowna
  server.on("/L",  handleL);    //
  server.on("/U",  handleU);    //Menu Ustawienia
  server.on("/U1", handleU1);   //Menu Ustawienia - Ustaw godzine
    server.on("/U1Gu", handleU1Gu);
    server.on("/U1Gd", handleU1Gd);
    server.on("/U1Mu", handleU1Mu);
    server.on("/U1Md", handleU1Md);
    server.on("/U1Ok", handleU1Ok);
  server.on("/U2", handleU2);   //Menu Ustawienia - Ustaw date
    server.on("/U2Du", handleU2Du);
    server.on("/U2Dd", handleU2Dd);
    server.on("/U2Mu", handleU2Mu);
    server.on("/U2Md", handleU2Md);
    server.on("/U2Yu", handleU2Yu);
    server.on("/U2Yd", handleU2Yd);
    server.on("/U2Ok", handleU2Ok);
//  server.on("/U3", handleU3);   //Menu Ustawienia - Zresetuj zegar
  server.on("/U4", handleU4);   //Menu Ustawienia - Test silnika
    server.on("/U4SL", handleU4SL);
    server.on("/U4SR", handleU4SR);
    server.on("/U4CL", handleU4CL);
    server.on("/U4Stop", handleU4Stop);
    server.on("/U4CR", handleU4CR);
  server.on("/D",  handleD);    //Menu Dianostyka
    server.on("/Dc", handleDc);   //Menu Clear min/max value
  server.onNotFound(handleNotFound);
}

void setup(void){
  Serial.begin(115200);
  Serial.println("");
  WiFi.mode(WIFI_AP);           //Only Access point
  WiFi.softAP(ssid, password);  //Start HOTspot removing password will disable security
 
  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpt IP:");
  Serial.println(myIP);

  enablePagesServer();

  server.begin();               //Start server
  Serial.println("HTTP server started");
}
//===============================================================
//                     LOOP
//===============================================================
void loop(void){
  String odebraneDane = "";
  
  server.handleClient();          //Handle client requests

  if(sendLoginToControllerFlag == true) {
    if(loginToControllerFlag == true) {
      Serial.println("Start");
    }
    else if(loginToControllerFlag == false) {
      Serial.println("Stop");
    }
  }

  if(sendSetClockFlag == true) {
    String msgESP = " G";

    if(timeDateHelp.hour < 10) { msgESP += "0"; msgESP += String(timeDateHelp.hour); }
    else { msgESP += String(timeDateHelp.hour); }
    msgESP += ":";
    if(timeDateHelp.minute < 10) { msgESP += "0"; msgESP += String(timeDateHelp.minute); }
    else { msgESP += String(timeDateHelp.minute); }
    Serial.println(msgESP);
    delay(500);
    
    Serial.println("SetClk");
    sendSetClockFlag = false;
  }

  if(sendRotateOnceLeft == true) {
    Serial.println(" OL");
    sendRotateOnceLeft = false;
  }
  if(sendRotateOnceRight == true) {
    Serial.println(" OR");
    sendRotateOnceRight = false;
  }
//  if(sendRotateContinuousLeft == true) {
//    Serial.println(" CL");
//    delay(200);
//  }
//  if(sendRotateContinuousRight == true) {
//    Serial.println(" CR");
//    delay(200);
//  }
//  if(sendStopRotate == true) {
//    Serial.println(" SR");
//    delay(200);
//  }
  
  if(sendResetMinMaxTemp == true) {
    Serial.println(" R");
    delay(200);
  }

  if (Serial.available() > 0) {
    char znacznikKomendy = ' ';
    
    odebraneDane = Serial.readStringUntil('\n');
    znacznikKomendy = odebraneDane.charAt(0);
    
    //Serial.println(odebraneDane);

    if(znacznikKomendy == (char)'L') {  // L0 - wylogowano lub L1 - zalogowano
      String komenda = odebraneDane.substring(1, 2);
      if(komenda == "0") {
        sendLoginToControllerFlag = false;
      }
      else if(komenda == "1") {
        sendLoginToControllerFlag = false;
      }
    }

    if(znacznikKomendy == (char)'C') {  // CL1 - ciagly obrot silnika w lewwo, CR1 - ciagly obrot silnika w prawo lub silnik stop
      String komenda = odebraneDane.substring(1, 3);
      if(komenda == "L1") {
        sendRotateContinuousLeft = false;
      }
      else if(komenda == "R1") {
        sendRotateContinuousRight = false;
      }
      else if(komenda == "S1") {
        sendStopRotate = false;
      }
    }
    
    if(znacznikKomendy == (char)'R') {  // R1 - Zresetowano min max temp
      String komenda = odebraneDane.substring(1, 2);
      if(komenda == "1") {
        sendResetMinMaxTemp = false;
      }
    }

//012345 67891111111 1112222 2222223 3333333 3344444 4444455 5555555 56
//           0123456 7890123 4567890 1234567 8901234 5678901 2345678 90
//G00:00 D00.00.2000 a000.00 b000.00 c000.00 d000.00 e000.00 f000.00
    else if(znacznikKomendy == (char)'G') {     //G00:00D00.00.2000
      String Time = odebraneDane.substring(1, 3); //0123456789ABCDEF
      timeDateClock.hour = Time.toInt();
      Time = odebraneDane.substring(4, 6);
      timeDateClock.minute = Time.toInt();
      String Date = odebraneDane.substring(7, 9); //0123456789A
      timeDateClock.day = Date.toInt();
      Date = odebraneDane.substring(10, 12);
      timeDateClock.month = Date.toInt();
      Date = odebraneDane.substring(13, 17);
      timeDateClock.year = Date.toInt();
      String TempController = odebraneDane.substring(18, 24); //0123456789A
      tempSterownika.Tactual = TempController.toFloat();
      TempController = odebraneDane.substring(25, 31);
      tempSterownika.Tmin = TempController.toFloat();
      TempController = odebraneDane.substring(32, 38);
      tempSterownika.Tmax = TempController.toFloat();
      String TempStepMotor = odebraneDane.substring(39, 45); //0123456789A
      tempSilnika.Tactual = TempStepMotor.toFloat();
      TempStepMotor = odebraneDane.substring(46, 52);
      tempSilnika.Tmin = TempStepMotor.toFloat();
      TempStepMotor = odebraneDane.substring(53, 59);
      tempSilnika.Tmax = TempStepMotor.toFloat();
    }

  }
  delay(200);
  
}
