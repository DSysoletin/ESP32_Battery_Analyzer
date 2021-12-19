// Load Wi-Fi library
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Replace with your network credentials
const char* ssid = "YourSSID";
const char* password = "YourPassword";

//display stuff
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Set web server port number to 80
AsyncWebServer server(80);

const int adcPin_vbatt = 35;
const int adcPin_vload = 32;
#define DELTA_FL 10
#define DAC1 25

#define ADC_VMAX 1900.0

#define RLOAD 0.22

int delta_buf[DELTA_FL];
int delta_buf_p=0;
int adcVal = 0;

int adc_batt,adc_load=0;


int DAC_val=0;

float vbat,vload,vdelta,iload;
float capacity=0;
float energy=0;
float int_resistance=0;

//Setpoints
float VoltageEndSP=700,CurrentSP=100; 

//Capacity count stuff
TaskHandle_t TaskCounter;
int run=0; 

//Data archive
int voltage_arch[2000],capacity_arch[2000],energy_arch[2000];
float int_resistance_arch[2000];
int arch_ptr=0;

void handle_server(void);
  
void setup() {
  // put your setup code here, to run once:
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(3);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println(F("Init..."));
  display.display();
  
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  Serial.begin(115200);
  delay(1000);

  xTaskCreatePinnedToCore(
                    counterTask,   /* Task function. */
                    "Capacity_cntr",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &TaskCounter,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  // Connect to Wi-Fi network with SSID and password
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1); 
  display.println(F("Connecting to:"));
  display.println(F(ssid));
  display.display();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1); 
  display.println(F("Connected. IP:"));
  display.println((WiFi.localIP().toString()));
  display.display();
  //server.begin();
  setup_server();
  digitalWrite(2, LOW);
}

void measure_int_resistance(void)
{
  int dac_val_buffer=0;
  float voltage_open=0,voltage_load=0;
  //turn the load off
  //dac_val_buffer = DAC_val;
  //DAC_val = 0;
  dacWrite(DAC1, 0);

  delay(500);
  voltage_open=float(get_adc(adcPin_vbatt))*(ADC_VMAX/4096.0);
  dacWrite(DAC1, 255);
  delay(500);
  voltage_load=float(get_adc(adcPin_vbatt))*(ADC_VMAX/4096.0);
  dacWrite(DAC1, DAC_val);
  Serial.print("U open:");Serial.print(voltage_open);
  Serial.print(", U load:");Serial.println(voltage_load);
  int_resistance=(RLOAD*(voltage_open-voltage_load))/voltage_load;
  Serial.print("R int:");Serial.println(int_resistance,5);
}

int get_adc(int pin)
{
  unsigned int adc_mean=0,num_samples=10;
  int i;
  adc_mean=0;
  analogSetAttenuation(ADC_6db);
  for(i=num_samples;i>0;i--)
  {
    //adc_raw[i]=analogRead(adcPin);
    adc_mean+=analogRead(pin);
    //delay(100);
  }
  adc_mean=adc_mean/num_samples;
  return(adc_mean);
}

int filter_delta(int delta_raw)
{
   int i,sum=0;
   
   delta_buf[delta_buf_p++]=delta_raw;

   if(delta_buf_p==DELTA_FL){
    delta_buf_p=0;
   }
   for(i=0;i<DELTA_FL;i++)
   {
    sum+=delta_buf[i];
   }
   return(sum/DELTA_FL);
}

void regulator(int sp, int pv, int db)
{

  if(pv<sp-db){
    if(DAC_val<255)
    {
      DAC_val++;
    }
  }
  if(pv>sp+db){
    if(DAC_val>0)
    {
      DAC_val--;
    }
  }
  dacWrite(DAC1, DAC_val);
}

//Run capacity counter at separate core to ensure equal intervals between counts
void counterTask( void * pvParameters)
{
  for(;;){
    if(run==1){
      capacity+=(iload/3600);
      energy+=((iload*vbat)/3600000);
    }
     
    delay(1000);
  }
}

void save_to_arch(void)
{
  voltage_arch[arch_ptr]=round(vbat);
  capacity_arch[arch_ptr]=round(capacity);
  energy_arch[arch_ptr]=round(energy);
  int_resistance_arch[arch_ptr]=int_resistance;
  arch_ptr++;
}

void update_display()
{
  char buf[32];
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1); 
  //Status(run/stop), IP address
  if(run==1)
  {
    display.print(F("R IP:"));
  }
  else
  {
    display.print(F("S IP:"));
  }
  display.println((WiFi.localIP().toString()));
  //Voltage, capacity
  display.print(F("Vb:"));
  display.print(vbat);
  display.print(F(" C:"));
  display.println(capacity);
  display.print(F("Enrg:"));
  display.print(energy);
  display.print(F(" I:"));
  display.println(iload);
  display.print(F("Rint:"));
  sprintf(buf,"%f",int_resistance);
  display.println(buf);
  //display.println(int_resistance,6);
  display.display();
}

void loop() {
  static long int millis_prev;
  int delta=0,delta_filtered=0;
  static int ticks=0,minutes=0;
 
  if(millis()-millis_prev > 1000)
  {
  adc_batt=get_adc(adcPin_vbatt);
  adc_load=get_adc(adcPin_vload);
  delta=adc_batt-adc_load;
  delta_filtered=filter_delta(delta);
  
  //Serial.println(get_adc());
  //Serial.println("ADC Batt: %d , ADC Load: %d \r\n",adc_batt,adc_load);
  //simple regulator
  
  
  //dacWrite(DAC1, 60);
  vbat=(float)adc_batt*(ADC_VMAX/4096.0);
  vload=(float)adc_load*(ADC_VMAX/4096.0);
  vdelta=(float)delta_filtered*(ADC_VMAX/4096.0);
  //vdelta=(float)delta*(ADC_VMAX/4096.0);
  iload=vdelta/RLOAD;

  //Measure internal resistance, if we just enter the run state and there was no measurements yet
  if((int_resistance==0)&&(run==1))
  {
    measure_int_resistance();
  }

  if(ticks%5==0)
  {
    update_display();
    if(run==1)
    {
      regulator(CurrentSP,iload,5);
    }
  }
  
  if(ticks==59)
  {
    if(run==1)
    {
      if(arch_ptr<2000)
      {
        save_to_arch();
      }
    }

    if(minutes==10)
    {
          if(run==1)
          {
            measure_int_resistance();
            Serial.print("Rint: ");Serial.println(int_resistance);
          }
       minutes=0;
    }

    ticks=0;
    minutes++;

  }

  //Autostop when voltage is below cutoff
  if((vbat<VoltageEndSP)&&(run==1))
  {
    if(arch_ptr<2000)
    {
      save_to_arch();
    }
    run=0;
    Serial.println("Stopping discharging because low voltage!");
  }

  /*Serial.print("ADC Batt: ");Serial.print(adc_batt);Serial.print(" ADC Load: ");Serial.println(adc_load);
  Serial.print("Delta: ");Serial.print(delta);Serial.print(" Delta filtered: ");Serial.println(delta_filtered);*/
  Serial.print("V Batt: ");Serial.print(vbat);Serial.print(" V Load: ");Serial.println(vload);
  Serial.print("V Delta filtered: ");Serial.print(vdelta);Serial.print(" I load: ");Serial.println(iload);
  Serial.print("DAC_val: ");Serial.println(DAC_val);
  Serial.print("Capacity: ");Serial.print(capacity);Serial.print(" Energy: ");Serial.println(energy);

  if(run==1)
  {
    //digitalWrite(2, HIGH);
  }
  else
  {
    //digitalWrite(2, LOW);
    dacWrite(DAC1, 0);
  }

  //Update status on web page
  //print_status();
  prepare_main_page();
  //delay(100);

    ticks++;
    millis_prev = millis();
  }
  
}
