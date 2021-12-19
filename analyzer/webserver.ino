const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";
const char* PARAM_VOLTAGE = "endvoltage";
const char* PARAM_CURRENT = "testcurrent";
const char* PARAM_RUN = "cmdrun";
const char* PARAM_STOP= "cmdstop";


char html_page[2048];
char status_data[1024];
// HTML web page to handle 3 input fields (input1, input2, input3)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    input1: <input type="text" name="input1">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get">
    input2: <input type="text" name="input2">
    <input type="submit" value="Submit">
  </form><br>
  <form action="/get">
    input3: <input type="text" name="input3">
    <input type="submit" value="Submit">
  </form>
</body></html>)rawliteral";



const char html_header[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Battery Tester control web page</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="30">
  </head><body>
)rawliteral";

const char html_footer[] PROGMEM = R"rawliteral(
<form action="/get">
   <input type="submit" name="cmdrun" Value="Run">
</form>
<form action="/get">
   <input type="submit" name="cmdstop" Value="Stop">
</form>
<a href="/csv">Download CSV((0-999)</a>
<a href="/csv2">Download CSV (1000-1999)</a>
</body></html>)rawliteral";

const char form_begin[] PROGMEM = R"rawliteral(
<form action="/get">
 )rawliteral";

const char form_voltage_end[] PROGMEM = R"rawliteral(
<input type="text" name="endvoltage">
    <input type="submit" value="Submit">
  </form><br>)rawliteral";  

const char form_current_end[] PROGMEM = R"rawliteral(
<input type="text" name="testcurrent">
    <input type="submit" value="Submit">
  </form><br>)rawliteral";  

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void prepare_main_page(void){
  print_status();
  sprintf(html_page,"%s %s Test end voltage(mV): %f %s\
  %s Discharge current(mA): %f %s \
  %s \
  %s",
  html_header,form_begin,VoltageEndSP,form_voltage_end,
  form_begin,CurrentSP,form_current_end,
  status_data,
  html_footer);
}

char* print_status(void){
  sprintf(status_data,"Current system status: <br>\
  Batt voltage: %f, Load voltage: %f <br>\
  Discharge current %f, Capacity: %f <br>\
  Energy: %f <br>\
  DAC value: %d <br>\
  Int. Resistance: %f <br>\
  Status (1 - running, 0 - stopped): %d <br>\
  Values in archive: %d <br>\
  <br>",
  vbat,vload,iload,capacity,energy,DAC_val,int_resistance,run,arch_ptr);
}

void setup_server(void)
{
  
  prepare_main_page();
  // Send web page with input fields to client
  /*server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });*/

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", html_page);
  });
  
  server.on("/csv", HTTP_GET, [](AsyncWebServerRequest *request){
    //print_csv();
    int i,n;
    AsyncResponseStream *response = request->beginResponseStream("text/csv");
    response->addHeader("content-disposition","attachment; filename=battery_data.csv");
    //response->print("Content-type: text/csv");
    response->printf("voltage(mV);capacity(mA*h);power(mW*h);int. resistance\r\n");
    if(arch_ptr<1000)
    {
      n=arch_ptr;
    }
    else
    {
      n=1000;
    }
    for (i=0;i<n;i++)
    {
      response->printf("%d;%d;%d;%f\r\n",voltage_arch[i],capacity_arch[i],energy_arch[i],int_resistance_arch[i]);
        yield();
    }

    request->send(response);
    //request->send_P(200, "text/html", html_page);
  });

  server.on("/csv2", HTTP_GET, [](AsyncWebServerRequest *request){
    //print_csv();
    int i,n;
    AsyncResponseStream *response = request->beginResponseStream("text/csv");
    response->addHeader("content-disposition","attachment; filename=battery_data2.csv");
    //header('Content-Disposition: attachment; filename="downloaded.pdf"');
    //response->print("Content-type: text/csv");
    response->printf("voltage(mV);capacity(mA*h);power(mW*h);int. resistance\r\n");
    if(arch_ptr<2000)
    {
      n=arch_ptr;
    }
    else
    {
      n=2000;
    }
    for (i=1000;i<n;i++)
    {
      response->printf("%d;%d;%d;%f\r\n",voltage_arch[i],capacity_arch[i],energy_arch[i],int_resistance_arch[i]);
        yield();
    }

    request->send(response);
    //request->send_P(200, "text/html", html_page);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    float new_val;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputParam = PARAM_INPUT_3;
    }
    else if (request->hasParam(PARAM_VOLTAGE)) {
      inputMessage = request->getParam(PARAM_VOLTAGE)->value();
      new_val=inputMessage.toFloat();
      if(new_val<0){
        new_val=0;
      }
      if(new_val>1500){
        new_val=1500;
      }
      VoltageEndSP=new_val;
      inputParam = PARAM_VOLTAGE;
    }
    else if (request->hasParam(PARAM_CURRENT)) {
      inputMessage = request->getParam(PARAM_CURRENT)->value();
      new_val=inputMessage.toFloat();
      if(new_val<50){
        new_val=50;
      }
      if(new_val>1000){
        new_val=1500;
      }
      CurrentSP=new_val;
      inputParam = PARAM_CURRENT;
    }
    else if (request->hasParam(PARAM_RUN)) {
      inputMessage = request->getParam(PARAM_RUN)->value();
      inputParam = PARAM_RUN;
      //Start the discharge process
      memset(voltage_arch,0,sizeof(voltage_arch));
      memset(capacity_arch,0,sizeof(capacity_arch));
      memset(energy_arch,0,sizeof(energy_arch));
      memset(int_resistance_arch,0,sizeof(int_resistance_arch));
      voltage_arch[0]=vbat;
      arch_ptr=1;
      DAC_val=50; //Initial DAC value.  Not zero to speed up current ramp up from 0 to setpoint
      capacity=0;
      energy=0;
      int_resistance=0;
      run=1;
    }
    else if (request->hasParam(PARAM_STOP)) {
      inputMessage = request->getParam(PARAM_STOP)->value();
      inputParam = PARAM_STOP;
      //stop the process
      run=0;
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
                                     + inputParam + ") with value: " + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");

    prepare_main_page();
  });
  server.onNotFound(notFound);
  server.begin();
}

void handle_server(void)
{
  
}
