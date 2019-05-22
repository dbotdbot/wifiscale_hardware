#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <string>


//Web server variables
//Wifi login
const char* ssid = "Stormhold";
const char* password = "itsonthefridge";
//Set Web server port
WiFiServer server(88); 
//Store HTTP request
String HTTPRequest;



//define pins for their functions
#define tarePin 3
#define sendPin 14
#define leftPin 0
#define rightPin 2

//Set the I2C id and LCD size
LiquidCrystal_I2C lcd(0x27, 16, 2);  

// initialize Scale object
// pin 13 for DOUT and 12 for clk
HX711 scale(13,12);

//variables for scale
int weight = 0;
int lastWeight = 1;  //set to one to ensure LCD updates on first boot
int foodPos = 0;
int lastFoodPos = 1;
String currentFood = "";
String foodName[4] = {"Milo", "Coffee", "Tea", "Sugar"};

//variables for wifi
bool sendJson = false;
WiFiClient client = server.available();

//variables for jsonPost
const int capacity = JSON_OBJECT_SIZE(3) + 70;            
StaticJsonDocument<capacity> doc;                 //make our json doc which will hold the json to send

void connectToWifi(){
  noInterrupts();
  lcd.setCursor(0, 1);
  lcd.print("Connecting to Wifi now");

  //Setup Wifi connection
  Serial.println("Setting up Wifi now");
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(100);
    Serial.print ("/");
  }
  Serial.println("!");
  Serial.println("Wifi is now connected!");
  Serial.print("IP address is ");
  Serial.print(WiFi.localIP());
  Serial.println("");
  Serial.println("WifiSetup before turning interupts back on");
  interrupts();
  Serial.println("Wifi Setup after turning interupts back on");
}

void jsonPOST(String weight, String foodtype){
  noInterrupts();
  Serial.println("In jsonPOST method");
  Serial.print("Weight is ");
  Serial.println(weight);
  Serial.print("Food type is ");
  Serial.println(foodtype);
  HTTPClient http;

  doc["timestamp"].set("09/05/2017 18:00:00");
  doc["weight"].set(weight);
  doc["foodtype"].set(foodtype);

  String jsonString;
  serializeJson(doc, jsonString);              //
  //serializeJson(doc, Serial);

  Serial.println(jsonString);
  

  http.begin("http://192.168.0.151:8090/postjson");

  //http.begin("http://127.0.0.1:8090/postjson");

  http.addHeader("Content-Type", "application/json");

  //serializeJsonPretty(doc, http);

  int httpCode = http.POST(jsonString);            //Send the request
  //String payload = http.getString();    //Get the response payload
 
  Serial.println(httpCode);   //Print HTTP return code
  //Serial.println(payload);    //Print request response payload
 
  http.end();  //Close connectio
  interrupts();
}

//Interrupts for the buttons
//all interupts are using some generic time since last interupt to filter out switch bouncing
void tareInterrupt(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200){
    Serial.println("Tare Button pressed!!!!!!");            //The tare method is really a bit to long for an ISR but hmmm it seems to work for the moment
    scale.tare();
    Serial.println("Tare finished");
  }
  last_interrupt_time = interrupt_time;
}

void sendInterrupt(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200){
    Serial.println("Send Button pressed!!!!!!");
    //code to trigger a Http push request (or set a flag to do it) will go here
    sendJson = true;
    //jsonPOST();
    //Serial.println("send finished");
  }
  last_interrupt_time = interrupt_time;
}

void leftInterrupt(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200){
    Serial.println("Left Button pressed!!!!!!");
    foodPos -= 1;
    if(foodPos < 0){
      foodPos = 0;
    }
    Serial.println("Left finished");
  }
  last_interrupt_time = interrupt_time;
}

void rightInterrupt(){
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200){
    Serial.println("Right Button pressed!!!!!!");
    foodPos += 1;
    if(foodPos > 3){
      foodPos = 3;
    }
    Serial.println("Right finished");
  }
  last_interrupt_time = interrupt_time;
}



void setup() {
  //setup serial
  Serial.begin(9600);
  //setup comunication with the lcd
  Wire.begin(D2,D1);
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  Serial.println("LCD setup finished");

  //scale setup
  int calibrationfactor = 2067;           //this is slightly off but with my 3d printed case prob as accurate as i will get it untill 
  scale.set_scale(calibrationfactor);     //i put more thought into its phyiscal construction
  scale.tare();

  //setup buttons
  pinMode(tarePin, INPUT_PULLUP);
  pinMode(sendPin, INPUT_PULLUP);
  pinMode(leftPin, INPUT_PULLUP);
  pinMode(rightPin, INPUT_PULLUP);

  //attach interupts for buttons
  attachInterrupt(digitalPinToInterrupt(tarePin), tareInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(sendPin), sendInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(leftPin), leftInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(rightPin), rightInterrupt, FALLING);

  //Welcome Message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wifi Scale");
  lcd.setCursor(0, 1);
  lcd.print("Connecting to Wifi now");

  //Setup Wifi connection
  /*Serial.println("Setting up Wifi now");
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(100);
    Serial.print ("/");
  }
  Serial.println("!");
  Serial.println("Wifi is now connected!");
  Serial.print("IP address is ");
  Serial.print(WiFi.localIP());
  Serial.println("");
  server.begin();

  */
  delay(2000);
}



void loop() {

  

  //only update food type message if different from last reading
  if(foodPos != lastFoodPos){
    // set cursor to first column, first row
    lcd.setCursor(0, 0);
    lcd.print("                ");        //clear this row before writing to it   
    lcd.setCursor(0, 0);                  //set cursor back to RHS
    // print food type message
    String food = foodName[foodPos];
    currentFood = food;
    //for debug
    Serial.print("Food = ");
    Serial.println(food);
    //Print out message
    lcd.print("Food = ");
    lcd.print(food);
  }
  lastFoodPos = foodPos;
  // update weight
  weight = scale.get_units(5);
  
  //for debuging weight output
  //Serial.print("weight = ");
  //Serial.println(weight);

  //only update LCD if weight has changed from last reading
  if (weight != lastWeight){ 
    lcd.setCursor(0,1);
    lcd.print("                ");         //clear this row before writing to it
    lcd.setCursor(0,1);                    //set cursor back to RHS
    lcd.print("Weight = ");
    lcd.print(weight, 10);
    lcd.print("g");
    Serial.println("updated weight");
  }
  
  lastWeight = weight;


  ////Test code to check scale is working
  /*
  Serial.print("ADC read: \t");
  Serial.println(scale.read());			// print a raw reading from the ADC
  Serial.print("scale.get_value: \t");    //take 5 readings and minus tare weight (if there is any)
  Serial.println(scale.get_value(5));
  Serial.print("scale.get_units: \t");
  Serial.println(scale.get_units(5), 1);
  */

  //check if need to send json
  if (sendJson == true){
    //code to connect to Wifi
    connectToWifi();
    Serial.println("Out of connect to wifi");
    //code to send jSon request
    char strWeight[20] = {};
    Serial.println("after initialize char array");
    itoa(weight, strWeight, 10);
    jsonPOST(strWeight, currentFood);
    sendJson = false;
  }
}



