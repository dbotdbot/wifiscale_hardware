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
const char* ssid = "WifiSSID";
const char* password = "PASWORD";
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
const int numberOfFoodItems = 4;
String foodName[numberOfFoodItems] = {"Milo", "Coffee", "Tea", "Sugar"};

//variables for wifi
bool sendJson = false;
WiFiClient client = server.available();

//variables for jsonPost
const int capacity = JSON_OBJECT_SIZE(3) + 70;    //+70 from arduinoJson helper (calcs space needed for strints in http request)        
StaticJsonDocument<capacity> doc;                 //make our json doc which will hold the json to send

void connectToWifi(){                             //Method to connect to Wifi, also displays message telling user the scale is attempting to connect
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
  interrupts();
}

void jsonPOST(String weight, String foodtype){
  noInterrupts();
  Serial.print("Weight is ");
  Serial.println(weight);
  Serial.print("Food type is ");
  Serial.println(foodtype);
  HTTPClient http;

  doc["timestamp"].set("09/05/2017 18:00:00");       //will be removed in later revisions
  doc["weight"].set(weight);
  doc["foodtype"].set(foodtype);

  String jsonString;
  serializeJson(doc, jsonString);              
  
  Serial.println(jsonString);
  
  http.begin("http://192.168.0.151:8090/postjson");

  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(jsonString);            //Send the request
  //String payload = http.getString();             //Get the response (usually too big causing esp to crash)
 
  Serial.println(httpCode);                        //Print HTTP return code
  //Serial.println(payload);                       //Print request response payload
 
  http.end();                                      //Close connectio
  interrupts();
}

//Interrupts for the buttons
//all interupts are using a timeout logic since last interupt to filter out switch bouncing
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
    //Set flag for sendJson method to be executed in main loop
    sendJson = true;
  }
  last_interrupt_time = interrupt_time;
}

void leftInterrupt(){                                          //change food possition with logic for end of list
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

void rightInterrupt(){                                         //change food possition with logic for end of list
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 200){
    Serial.println("Right Button pressed!!!!!!");
    foodPos += 1;
    if(foodPos > (numberOfFoodItems - 1)){
      foodPos = (numberOfFoodItems - 1);
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

  //check if need to send json
  if (sendJson == true){
    //code to connect to Wifi
    connectToWifi();
    //code to send jSon request
    char strWeight[20] = {};
    itoa(weight, strWeight, 10);
    jsonPOST(strWeight, currentFood);
    sendJson = false;
  }
}



