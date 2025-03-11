#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>           //file system for esp
#include <max6675.h>          //thermocouple library
#include <PID_v1.h>           //Pid control library
#include <Arduino_JSON.h>     //JSON support library to record chart values

#define MISO 13               //max6675 MISO
#define SCK 12                //max6675 CLK
#define CS 14                 //max6675 CS
#define BUZZER  9             //pin for buzzer positive terminal
#define BUZZERNOTE 1000 
#define SSR 11                //pin for SSR relay contolled with pwm
#define FAN 10                //pin for FAn control mosfet with pwm
#define ANALOGWRITERESOLUTION 12 //bit resolution for analogWrite() function
#define MODE 0                //mode 0->the ideal reflow curve transition is istantaneous
#define RAMPANGLE 3           //mode 1->the ideal reflow curve transition is a ramp of angle RAMPANGLE (Celsius/seconds).
                              //Note that mode 1 is currently under test, mode 0 is reccomended

//function declarations
void notFound(AsyncWebServerRequest *);
String sendValuesToChart();
void checkBuzzer();
void startBuzzer();
void getTemperature();
void stateRoutine();
void calcSetpoint();
void wifiConnect();
void pid_setup();

class reflowProfile{          //reflowProfile is a class that controls the parameters of the reflow curve
  public:
  float preHeat;
  float preHeatTime;
  float peakTemp;
};

//generic parameters for pid system control
double setpoint,input;

//parameter for heater pid control
double output_heater;
double kp_heater=0.55;
double ki_heater=0.0013;
double kd_heater=0.005;
double lastHeaterOutput=0;

//parameters for fan pid control
double output_fan;
double kp_fan=0.55;
double ki_fan=0.1;
double kd_fan=0.1;
double lastFanOutput=0;

PID pid_heater(&input,&output_heater,&setpoint,kp_heater,ki_heater,kd_heater,DIRECT);
PID pid_fan(&input,&output_fan,&setpoint,kp_fan,ki_fan,kd_fan,REVERSE);

reflowProfile reflow;

const char* ssid = "TP-Link_413B";
const char* password = "74518531";

//variables to store the reflow values got from html /get
const char* input_t1="input_t1";
const char* input_time="input_time";
const char* input_t2="input_t2";

//webserver handling objects
JSONVar readings;
AsyncWebServer server(80);
AsyncEventSource events("/events");

//timers to regulate the buzzer start and stop
const long buzzTime=500;
bool buzzState=0;
unsigned long lastBuzzStart=0;

//timers to regulate the temperature readings from max6675
const long temperatureReadDelay=300;
unsigned long lastTemperatureRead=0;

//default values to rflow profile
float defaultPeakTemp=100;
float defaultPreHeat=50;
float defaultPreHeatTime=60; //seconds

//states handling avariables
int lastState=-1;
int currentState=0;
unsigned long state1start;
unsigned long state2start;
unsigned long state3start;
unsigned long state4start;
unsigned long state5start;
const long state1lenght=10000;

const int offTemp=20; //celsius, default setpoint temperature when the system is idle


//timers to regulate the sending of data to the html chart
unsigned long lastSendToChart = 0;
unsigned long sendToChartDelay = 1000;

MAX6675 thermocouple(SCK,CS,MISO);  //max6675 thermocouple object


void setup(){
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  Serial.begin(115200);
  wifiConnect();
  pid_setup();
  analogWriteResolution(int(ANALOGWRITERESOLUTION));
  reflow.peakTemp=defaultPeakTemp;
  reflow.preHeat=defaultPreHeat;
  reflow.preHeatTime=defaultPreHeatTime;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request){
    String t1;
    String t1_param;
    String time;
    String time_param;
    Serial.println("Recieved get call");
    t1=request->getParam(input_t1)->value();
    t1_param=input_t1;

    time=request->getParam(input_time)->value();
    time_param=input_time;
    reflow.preHeat=atoi(t1.c_str());
    reflow.preHeatTime=atoi(time.c_str());
    Serial.print("pre heat: ");
    Serial.println(reflow.preHeat);
    Serial.print("Time: ");
    Serial.println(reflow.preHeatTime);
    currentState=1;
    Serial.print("Starting Reflow: ");
    startBuzzer();
    state1start=millis();
    request->send(200);
  });

  server.serveStatic("/", SPIFFS, "/");

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = sendValuesToChart();
    request->send(200, "application/json", json);
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  server.begin();
}

void loop(){
  getTemperature();
  checkBuzzer();
  calcSetpoint();
    stateRoutine();
  if ((millis() - lastSendToChart) > sendToChartDelay) {
    // Send Events to the client with the Sensor Readings Every 10 seconds
    events.send("ping",NULL,millis());
    events.send(sendValuesToChart().c_str(),"new_readings" ,millis());
    lastSendToChart = millis();
  }
}



//custom built functions section


void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}


String sendValuesToChart(){
  readings["idealTemp"] = String(setpoint);
  readings["realTemp"] = String(input);
  String jsonString = JSON.stringify(readings);
  return jsonString;
}


void checkBuzzer(){
  unsigned long currentTime=millis();
  if(buzzState==1&&currentTime-lastBuzzStart>buzzTime){
    buzzState=0;
    noTone(BUZZER);
  }
  return;
}

void startBuzzer(){
  tone(BUZZER,BUZZERNOTE);
  buzzState=1;
  lastBuzzStart=millis();
  return;
}

void getTemperature(){
  unsigned long currentTime=millis();
  if(currentTime-lastTemperatureRead>temperatureReadDelay){
    lastTemperatureRead=currentTime;
    input=thermocouple.readCelsius();
  }
  return;
}

void stateRoutine(){
  unsigned long currentTime=millis();
  switch (currentState)
  {
  case 0: //reflow not started yet
    if(lastState!=0){
      lastState=0;
    }
    break;

  case 1: // waiting for state1lenght to start the actual reflow
    if(currentTime-state1start>=state1lenght){
      currentState=2;
      state2start=currentTime;
    }
    if(lastState!=1){
      lastState=1;
      Serial.println("Entered state 1");
    }
    break;

  case 2: //preheating the iron to reflow.preHeat temperature
    if(lastState!=2){
      setpoint=reflow.preHeat;
      lastState=2;
      Serial.println("Entered state 2");
    }
    pid_heater.Compute();
    analogWrite(SSR,output_heater);
    if(output_heater!=lastHeaterOutput){
      Serial.print("Heater Output: ");
      Serial.println(output_heater);
      lastHeaterOutput=output_heater;
      }
    pid_fan.Compute();
    analogWrite(FAN,output_fan);
    if(output_fan!=lastFanOutput){
    Serial.print("Fan Output: ");
    Serial.println(output_fan);
    lastFanOutput=output_fan;
    }
    if(input>reflow.preHeat-5){ //the change of state triggers when the temperature gets 5 celsius lower than the preHeat setpoint
      currentState=3; 
      state3start=currentTime;
    }
    break;

  case 3:   //keeping preheat for the selected time  
    if(lastState!=3){
      lastState=3;
      Serial.println("Entered state 3");
    }
    pid_heater.Compute();
    pid_fan.Compute();
    analogWrite(SSR,output_heater);
    analogWrite(FAN,output_fan);
    if(currentTime-state3start>=reflow.preHeatTime){
      currentState=4;
    }
    break;
  
  case 4:   //heating to peakTemp
    if(lastState!=4){
      lastState=4;
      Serial.println("Entered state 4");
    }
    pid_heater.Compute();
    pid_fan.Compute();
    analogWrite(SSR,output_heater);
    analogWrite(FAN,output_fan);
    if(input>reflow.peakTemp-5){
      currentState=5;
    }    
    break;

    case 5: //waiting the system to cool down
    if(lastState!=5){
      lastState=5;
      Serial.println("Entered state 5");
    }
    pid_heater.Compute();
    pid_fan.Compute();
    analogWrite(SSR,output_heater);
    analogWrite(FAN,output_fan);
    if(input<40){
      currentState=0;
    }    
    break;

  default:
    break;
  }
}

void calcSetpoint(){
  unsigned long currentTime=millis();
    switch(currentState){
      case 0:
        setpoint=offTemp;
        break;

      case 1:
        setpoint=offTemp;
        break;

      case 2:
        if(MODE==1){
          setpoint=offTemp+(currentTime-state2start)*RAMPANGLE/1000;
          if(setpoint>reflow.preHeat) {
            setpoint=reflow.preHeat;
          }
        }
        else if(MODE==0){
          setpoint=reflow.preHeat;
        }
        break;

      case 3:
        setpoint=reflow.preHeat;
        break;

      case 4:
        if(MODE==1){
          setpoint=reflow.preHeat+(currentTime-state4start)*RAMPANGLE/1000;
          if(setpoint>reflow.peakTemp){
            setpoint=reflow.peakTemp;
          }
        }
        else if(MODE==0){
          setpoint=reflow.peakTemp;
        }   
        break;
        
        case 5:
          if(MODE==1){
            setpoint=reflow.peakTemp-(currentTime-state5start)*RAMPANGLE/1000;
            if(setpoint<offTemp){
              setpoint=offTemp;
            }
          }
          else if(MODE==0){
            setpoint=offTemp;
          }
          break;

      default:
        break;
    }
  }


void wifiConnect(){
//implementa funzione che si connette al wifi tra quelli saklvati che prende meglio, lista wifi da file wifiNetworks.h
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
if (WiFi.waitForConnectResult() != WL_CONNECTED) {
  Serial.println("Connecting...");
  return;
}
Serial.println();
Serial.print("IP Address: ");
Serial.println(WiFi.localIP());
}

void pid_setup(){
  pid_heater.SetOutputLimits(0,2^ANALOGWRITERESOLUTION);
  pid_fan.SetOutputLimits(0,2^ANALOGWRITERESOLUTION);
  pinMode(SSR,OUTPUT);
  digitalWrite(SSR,LOW);
  pinMode(FAN,OUTPUT);
  digitalWrite(FAN,LOW);
  pid_heater.SetMode(AUTOMATIC); 
  pid_fan.SetMode(AUTOMATIC); 
}

/*
Da fare:
Crea funzione wifiConnect
Sistemare void setup
Scrivere commento legenda stati 
Separare HTML e JS
Test pid fan e calibrazionui(da fare a casa)
aggiorna html con form peaktemp
aggiorna html con blocco pulsante se i campi di input non sono validi
sistema in calcsetpoint l'assegnazione multipla del setpoint quando resta costante
capire come settare res analogWrite con def
*/
