
/*
 * fermentation controller_V.01.ino
 *
 * Created: 6/12/2015 2:09:31 PM
 * Author: cgriffin
 
 							
							
**************PIN MAPPINGS***************
A5-->I2C --expansion needs	
A4-->I2C --expansion needs	
A3-->MAX_SS					
A2-->Up button				 
A1-->Down button			 	
A0-->Mode button			 	
D13->SCK
D12->MISO
D11->MOSI
D10->ETH_CS			
D9-->RED_LED
D8-->1-wire
D7-->AC Relay
D6-->GREEN_LED
D5-->BLUE_LED
D4-->SD_CS
D3-->Heater relay
D2-->Glycol Pump relay
D1-->TX
D0-->RX
**************PIN MAPPINGS***************
 */ 


/********************************************************************************
Includes
********************************************************************************/
#define DallasOneWire
#define ETH
#define DEBUG
	//#define DHCP
#define Max
#define Relays
#define Buttons
#define SerialPrint
#define EEprom
#define Mysql
#define Experimental
#include <SPI.h>
#include <Ethernet.h>
#include <bitBangedSPI.h>
#include <MAX7219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <utility/w5100.h>
/********************************************************************************
Constants
********************************************************************************/
//OUTPUTS
//indicators or tricolor led
#define RED_LED 9
#define GREEN_LED 6
#define BLUE_LED 5


#ifdef Relays
#define AC_PIN 7
#define GLYCOL_PIN 3
#define HEAT_PIN 2
#endif

#define MAX_SS A3
//INPUTS
#define MODE_RESET_BUTTON A0
#define UP_BUTTON A2
#define DOWN_BUTTON A1
//SENSORS
#ifdef DallasOneWire
#define SENSORNUM 2   //change this to expected sensor quantity
#define ONE_WIRE_BUS 8
#define TEMPERATURE_PRECISION 12
#endif
/********************************************************************************
Macros and Defines
********************************************************************************/
////////////////////////////////////////ethernet/////////////////////////////////////////
//#ifdef ETH
byte mac[] = { 0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed };  //required   will not work inside eth scaffolding
byte ip[] = { 192, 168, 1, 105 };		//failover ip
byte DNS[] = { 4, 2, 2, 4 };			//failover dns
byte gateway[] = { 192, 168, 1, 254 };  //failover gateway
byte subnet[] = { 255, 255, 255, 0 };		//failover subnet
//#endif

////////////////////////////////////////MAX display////////////////////////////////////////
const byte chips = 1;
MAX7219 display (chips, MAX_SS);
//MAX7219 display (chips, MAX_SS, 11, 13); //to bitbang any port 
//MAX7219 display (chips, MAX_SS); //for when you want to use real SPI bus

/********************************************************************************
Function Prototypes
********************************************************************************/
#ifdef DallasOneWire
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
#endif
/********************************************************************************
Global Variables
********************************************************************************/
#ifdef Mysql
EthernetClient client;
char server[] = "192.168.1.87"; // IP Address (or name) of server to dump data to
#endif

#ifdef DallasOneWire
float glycolTempC, glycolTempF, fermTempC, fermTempF;
String glycolAddress, fermAddress;
int numberOfDevices;
DeviceAddress /*glycolThermometer, fermThermometer,*/ tempDeviceAddress;
#endif
/*testing*/
///////////////////////////////////////////////////////////////////
unsigned long debouncemillis;
unsigned long holddowncycle;


int modeState, moderesetbuttonpushCounter; //these count button pushes
int upholddownCounter, downholddownCounter, moderesetholddownCounter; //this is the counter used to detect push and hold

boolean buttonclear;
boolean defaultbuttonState;
boolean lastdefaultbuttonState;
int defaultbuttonpushCounter;


byte setfermTempF = 83;
int setglycolTempF = 20;
int setcoldcrashTempF = 32;

int ontime = 30000;

boolean tempScale;
int oldmodeState;
int oldsetcoldcrashTemp;
int oldsetfermTemp;
int oldmoderesetbuttonPushCounter;
#define EEPROM_SAVE_DELAY 5000			//time after button release before eprom save
/////////////////////////////////////////////////////////////////////////////
#ifdef Relays
boolean acState, heaterState, glycolState; 
#endif


boolean upbuttonState, downbuttonState, moderesetbuttonState;			//captures the current state of the button;
boolean lastupbuttonState, lastdownbuttonState, lastmoderesetbuttonState;  //the last state of the button;
//ARRAYS FOR MULTIPLE SENSORS
float tempArrayC[SENSORNUM], tempArrayF[SENSORNUM];
String stringaddressArray[SENSORNUM][16];
char charaddressArray[SENSORNUM][16];
////////////////looping variables////////////
#define ONE_SECOND 1000
unsigned long currentMillis; //main clock
unsigned long previousOneSecondMillis; //one second
unsigned long previousTenSecondMillis; //ten second
unsigned long buttonMillis;
unsigned long sametimebuttonMillis;
unsigned long eepromsavedMillis;
unsigned long repressdetect;
/********************************************************************************
Main
********************************************************************************/

void setup(){
	//*************Routine Peripheral Initialization**************************
	Serial.begin(115200);
	Serial.println("Serial started");
		display.begin ();
		display.sendString ("Frnntrlr");
		delay(1000);
		display.sendString ("        "); //clears display without turning it off
	Serial.println("Display started");
	
		pinMode (MODE_RESET_BUTTON, INPUT);
		pinMode (UP_BUTTON, INPUT);
		pinMode (DOWN_BUTTON, INPUT);
		digitalWrite(MODE_RESET_BUTTON,LOW);
		digitalWrite(UP_BUTTON, LOW);
		digitalWrite(DOWN_BUTTON, LOW);
	Serial.println("Inputs started");
	#ifdef Relays
		pinMode (AC_PIN, OUTPUT);
		pinMode (GLYCOL_PIN, OUTPUT);
		pinMode (HEAT_PIN, OUTPUT);
	#endif
		#define RED_LED 9
		#define GREEN_LED 6
		#define BLUE_LED 5
		#define MEGA_SPI_EN 53
		pinMode(MEGA_SPI_EN, OUTPUT);
		pinMode(RED_LED, OUTPUT);
		pinMode(BLUE_LED, OUTPUT);
		pinMode(GREEN_LED, OUTPUT);
#ifdef EEprom
	eepromRetrieve();
#endif

	
	#ifdef ETH
	boolean DHCPtrigger = 0;    //scaffolding trigger for ETH/DHCP
	
	#ifdef DHCP
	if (Ethernet.begin(mac) == 0) {
		Serial.println("DHCP Failed");
		}else{
		Serial.println("Success! DHCP IP is");
		Serial.println(Ethernet.localIP());
		DHCPtrigger = 1;
	}
	#endif

	//Failover to static ip
	if (DHCPtrigger == 0){
		Ethernet.begin(mac, ip, DNS, gateway, subnet);
		Serial.print("My IP is: ");
		for (byte printarray = 0; printarray < 4; printarray++) {
			// print the value of each byte of the IP address:
			Serial.print(Ethernet.localIP()[printarray], DEC);
			Serial.print(".");
		}
		Serial.println();
	}

	#endif

W5100.setRetransmissionCount(2);

Dallas_One_Wire_setup();
	sensors.setWaitForConversion(false);  // makes it async
	gatherSensorData();   //preload temperature data before start
	
	Serial.println("Connecting to temperature database");
	webDump();
}

void loop(){
	currentMillis = millis();
Serial.println (moderesetbuttonpushCounter);
#ifdef Buttons
	readButtons();
	moderesetButton();
	upButton();
	downButton();
	sametimeButton();
	variableConstraints();
#endif 
	whatMode();
#ifdef Relays
	
	relayOutput();
#endif
	if ((currentMillis - previousTenSecondMillis) > (ONE_SECOND * 10))
	{
		webDump();
		//client.stop();
		previousTenSecondMillis = currentMillis;
	}

	 if(currentMillis - previousOneSecondMillis > ONE_SECOND) {
		 //do one second stuff here
		// serialprintrelayStates();
		gatherSensorData();
	#ifdef SerialPrint
	serialprintinfo();
	#endif
		 previousOneSecondMillis = currentMillis;
	 }
	///one second loop end///
	
}

#ifdef SerialPrint

void serialprintinfo(){
	switch (modeState){
		case 0:
			Serial.println ("Fermentation Mode");
			break;
		
		case 1:
			Serial.println ("Cold Crash Mode");
			break;
			
		case 2:
			Serial.println ("Off Mode");
			break;
	}
	Serial.print ("Fermentation Tank Temperature: ");
	Serial.println (fermTempF);
	Serial.print ("Glycol Tank Temperature: ");
	Serial.println(glycolTempF);
	Serial.print ("Set Fermentation Temperature: ");
	Serial.println(setfermTempF);
	serialprintrelayStates();
}

void serialprintrelayStates(){
	if (glycolState == 1) 
	{
		Serial.println("Glycol on");
		}
		else
		{
		Serial.println("Glycol off");
	}
	if (heaterState == 1)
	{
		Serial.println("Heater on");
		}
		else
		{
		Serial.println("Heater off");
	}
	if (acState == 1)
	{
		Serial.println("AC on");
		}
		else
		{
		Serial.println("AC off");
	}
	Serial.println();
	
	///////////////////////////////testing string zone///////////////////////
	char string[20];
	stringaddressArray[0][0].toCharArray(string,20);
	 //
	//Serial.println(string);
	//////////////////////////////////////////////////////////////////
	
}
	
#endif

#ifdef Max
void temptoMax(void) //This sends data to the max7219 7seg display
{
	char format[8];
	char string[5];
	int setfermTempC = (DallasTemperature::toCelsius(setfermTempF));
	int setglycolTempC = (DallasTemperature::toCelsius(setglycolTempF));
	int setcoldcrashTempC = (DallasTemperature::toCelsius(setcoldcrashTempF));
	
	if (tempScale == 0) //if tempscale is set to F 
	{
		dtostrf (fermTempF,3,2,string);
		if (modeState == 0)
		{
			sprintf(format, "%d %sF",setfermTempF, string);
		}
		if (modeState == 1)
		{
			sprintf(format, "%d %sF",setcoldcrashTempF, string);
		}
	
		display.sendString(format);
	}
	else               //if tempscale is set to C
	{	
		dtostrf (fermTempC,3,2,string);
		if (modeState == 0)
		{
			sprintf(format, "%d %sC",setfermTempC, string);
		}
		if (modeState == 1)
		{
			sprintf(format, "%-2d %sC",setcoldcrashTempC, string);
		}
		
		display.sendString(format);
	}
}  
#endif

#ifdef DallasOneWire
/////////////////////////////////Dallas 1Wire functions////////////////////////////

void gatherSensorData()  //this fills the requisite variables with the appropriate values
{
	//	sensors.setWaitForConversion(false);  // makes it async
		sensors.requestTemperatures();
		//sensors.setWaitForConversion(true);
	
	
	glycolTempC = (sensors.getTempCByIndex(0)); 
	fermTempC = (sensors.getTempCByIndex(1)); 
	for (int i = 0; i < SENSORNUM; i++)
	tempArrayC[i] = (sensors.getTempCByIndex(i));
	fermTempF = (DallasTemperature::toFahrenheit(fermTempC));  
	glycolTempF = (DallasTemperature::toFahrenheit(glycolTempC));
	
	

}

#endif
///////////////////////////////////////////////////////////////////////////////////////
/*********************************BUTTON FUNCTIONS*************************************/
#ifdef Buttons

void readButtons(){   //30ms loop to read buttons
	
	if(currentMillis - buttonMillis > 30) 
	{
		upbuttonState = digitalRead(UP_BUTTON);
		downbuttonState = digitalRead(DOWN_BUTTON);
		moderesetbuttonState = digitalRead(MODE_RESET_BUTTON);
		buttonMillis = currentMillis;
		
	}
}

void moderesetButton(){
		if (moderesetbuttonState != lastmoderesetbuttonState){
			if (moderesetbuttonState == HIGH) {
				moderesetbuttonpushCounter++;
				modeState++;
			}
			lastmoderesetbuttonState = moderesetbuttonState;
			holddowncycle = currentMillis;
		}
		if ((moderesetbuttonState == HIGH) && (moderesetbuttonState == lastmoderesetbuttonState) && ((currentMillis - holddowncycle) >= (ONE_SECOND * 5))){
		holddowncycle = currentMillis;
		reset();
		}
	}
	

void upButton(){
	if (upbuttonState == 0){
		//	Serial.println("upbutton released");
		lastupbuttonState = upbuttonState;
		upholddownCounter = 0;
		}else{
		if  (upbuttonState != lastupbuttonState){  //upbutton pressed
			debouncemillis = currentMillis;
			holddowncycle = currentMillis;
			if (upbuttonState != lastupbuttonState){									//upButton still pressed after 10ms
			//				Serial.println("upbutton pressed");
			upordown(1);
			lastupbuttonState = upbuttonState;
			}
		}
	}
		if (upbuttonState == HIGH){
			debouncemillis = currentMillis;
		}
		if ((currentMillis - debouncemillis) > 10){
			if (upbuttonState == HIGH){
			lastupbuttonState = upbuttonState;
			upordown(1);
				
			}
		}
	
 if (((upbuttonState == HIGH) && (upbuttonState == lastupbuttonState) && ((currentMillis - holddowncycle) > 500))){
			holddowncycle = currentMillis;
			upordown(1);
		}
	}
	
void downButton(){
		
	if (downbuttonState == 0){
		//Serial.println("downbutton released");
		lastdownbuttonState = downbuttonState;
		downholddownCounter = 0;
		}else{
		if  ((downbuttonState != lastdownbuttonState) && ((currentMillis - debouncemillis) > 10)){
			debouncemillis = currentMillis;
			holddowncycle = currentMillis;
			if (downbuttonState != lastdownbuttonState){
				//					Serial.println("downbutton pressed");
				upordown(-1);
				lastdownbuttonState = downbuttonState;
			}
		}
	}
	if (downbuttonState == HIGH){
		debouncemillis = currentMillis;
	}
	if ((currentMillis - debouncemillis) > 10){
		if (downbuttonState != lastdownbuttonState){
			upordown(-1);
			lastdownbuttonState = downbuttonState;
		}
	}
		
		

		
	/////////////////////////////////////////////////////////////////
	if (((downbuttonState == HIGH) && (downbuttonState == lastdownbuttonState) && ((currentMillis - holddowncycle) > 500))){
		holddowncycle = currentMillis;
		upordown(-1);
	}
}


	

void upordown(int x)
{
	switch (modeState)
	{
		case 0:
		setfermTempF = (setfermTempF + x);
		break;
		case 1:
		setcoldcrashTempF = (setcoldcrashTempF + x);
		break;
	}
}


void sametimeButton()
{
	if ((upbuttonState == 1) && (downbuttonState == 1))
		if((currentMillis - sametimebuttonMillis) > 1000)
	{
		tempScale = !tempScale;
		sametimebuttonMillis = currentMillis;
	}
	
}



void reset(){
	{
	Serial.println("clearing eeprom");
	for (int i = 0; i < 512; i++)
	EEPROM.write(i, 0);		}
	// Restarts program from beginning but does not reset the peripherals and registers
	asm volatile ("  jmp 0");
}


#endif

/*********************************BEGIN LOGIC FUNCTIONS*************************************/


void variableConstraints(){
	if (setfermTempF > 89)
		setfermTempF = 76;
	
	if (setfermTempF < 76)
		setfermTempF = 89;
	

	if (setcoldcrashTempF > 40)
		setcoldcrashTempF = 25;
	
	if (setcoldcrashTempF < 25)
		setcoldcrashTempF = 40;
		
	if (modeState > 2)
		modeState = 0;
}


/********************************* Modes of operation*************************************/
#ifdef Relays
void whatMode(){
	switch (modeState) {
		case 0:
		fermentation();
		heartBeat(GREEN_LED);		//this is an LED in sinewave to show the micro is functioning
		digitalWrite(BLUE_LED,LOW);
			#ifdef Max
			temptoMax();
			#endif

		
		

		break;
		case 1:
		coldcrash();
		heartBeat(BLUE_LED);		//this is an LED in sinewave to show the micro is functioning
		digitalWrite(GREEN_LED,LOW);
		#ifdef Max
		temptoMax();
		#endif


		break;
		case 2:
		off();
		break;
	}
	
	eepromSave();
}

void off(){
	glycolState = 0;
	heaterState = 0;
	acState = 0;
	digitalWrite(BLUE_LED, LOW);
	digitalWrite(RED_LED, LOW);
	digitalWrite(GREEN_LED, LOW);
	display.sendString("        ");
}


void fermentation(){
unsigned long glycolstartTime;
unsigned long heaterstartTime;

	if (fermTempF > setfermTempF) {
		glycolState = 1;
		glycolstartTime = currentMillis;

		}else{
		if ((currentMillis - glycolstartTime) > ontime) {
			glycolState = 0;
		}
	}

	if (fermTempF < setfermTempF){
		heaterState = 1;
		heaterstartTime = currentMillis;
		}else{
		if ((currentMillis - heaterstartTime) > ontime) {
			heaterState = 0;
		}
	}
	
	// glycol tank
	if (glycolTempF > (setglycolTempF + 20)){
		acState = 1;
		}else{
		acState = 0;
	}
	//if fermentation temp sensor is more than set temp turn on glycol pump for x seconds
	//if glycol tank is more than set temp turn on ac switch
	//if fermentation tank is less than set temp turn on heaterpin
}


void coldcrash(){

	heaterState = 0;
	if (fermTempF > (setcoldcrashTempF + 4)){ 
		glycolState = 1;
		}else{
		glycolState = 0;
	}
	
	
	// glycol tank
	if (glycolTempF > (setglycolTempF + 4)){
		acState = 1;
		}else{
		acState = 0;
	}
	
	
	//if fermentation temp sensor is more than set temp turn on glycol pump for x seconds
	//if glycol tank is more than set temp turn on ac switch
	//if fermentation tank is less than set temp turn on heaterpin
}
#endif
/*********************************END LOGIC FUNCTIONS*************************************/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////OTHER FUNCTIONS///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**********************************BEGIN EEPROM FUNCTIONS**************************************************/


void eepromSave(){			//////////This saves the current values of these settings into eeprom


	if ((upbuttonState == HIGH) || (downbuttonState == HIGH) || (moderesetbuttonState == HIGH)){
		repressdetect = currentMillis;
		}
		else
		{
		if ((currentMillis - repressdetect) > EEPROM_SAVE_DELAY)
		{  
			if (oldsetfermTemp != setfermTempF){
					EEPROM.write (200, setfermTempF);
					Serial.println("EEPROM WRITE");
					flashDisplay();
					oldsetfermTemp = setfermTempF;
			}
			
			if (oldsetcoldcrashTemp != setcoldcrashTempF){
					EEPROM.write (201, setcoldcrashTempF);
					Serial.println("EEPROM WRITE");
					flashDisplay();
					oldsetcoldcrashTemp = setcoldcrashTempF;
			}
			
			if (oldmodeState != modeState){
					EEPROM.write (202, modeState);
					Serial.println("EEPROM WRITE");
					flashDisplay();
					oldmodeState = modeState;
			}
		}
	}
}

void flashDisplay()	//this function flashes the max display 3 times to note eeprom save
{
	for (int x = 0; x < 3; x++)
	{
		delay(200);
		display.setIntensity(0);
		delay(200);
		display.setIntensity(15);
		
	}

	}
	
void eepromRetrieve(void)
{
	
//if eeprom space is not empty then pull variable out of eeprom

		setfermTempF = EEPROM.read(200);
		oldsetfermTemp = setfermTempF;
		Serial.print("FERM_TEMP ");
		Serial.println(EEPROM.read(200));
	
		setcoldcrashTempF = EEPROM.read(201);
		oldsetcoldcrashTemp = setcoldcrashTempF;
		Serial.print("CC_TEMP ");
		Serial.println(EEPROM.read(201));
	
		modeState = EEPROM.read(202);
		oldmodeState = modeState;
		Serial.print("MODE ");
		Serial.println(EEPROM.read(202));
		
}
/**********************************END EEPROM FUNCTIONS**************************************************/

void heartBeat(int led){
	int period = 2000;
	int value;
	value = 128+127*cos(2*PI/period*currentMillis);
	analogWrite(led, value);           // sets the value (range from 0 to 255)
	// sets the value (range from 0 to 255)
}
///////////////////////////////////////////////////
#ifdef Relays
void relayOutput()
{
	digitalWrite(AC_PIN, acState);
	digitalWrite(HEAT_PIN, heaterState);
	digitalWrite(GLYCOL_PIN, glycolState);
}
#endif

///////////////////////////////EXPERIMENT/////////////////////////////////////////
void Dallas_One_Wire_setup()
{	
	sensors.begin();  // Start up the library
	numberOfDevices = sensors.getDeviceCount();  // Grab a count of devices on the wire
	Serial.println("Locating devices");	// locate devices on the bus
	Serial.print("Found ");
	Serial.print(numberOfDevices, DEC);
	Serial.println(" devices.");
	
	if (SENSORNUM == numberOfDevices)
	{
		Serial.println("Correct sensor numbers");
	}
	else
	{
		Serial.println("wrong # of sensors, adjust SENSORNUM");
	}
	// ///////////////Loop through each device, print out address////////////////////////////////
	for(int i=0; i<numberOfDevices; i++){
		if(sensors.getAddress(tempDeviceAddress, i))       // Search the wire for address
		{
			Serial.print("Found device ");
			Serial.print(i + 1, DEC);
			Serial.print(" with address: ");
			stringaddressArray[i][0] = (printAddress(tempDeviceAddress));
			//charaddressArray[i][0] = StringstringaddressArray[i][0]);
			//addressArray[i][0] = tempDeviceAddress;
			Serial.println();
			// set the resolution to 12 bit (Each Dallas/Maxim device is capable of several different resolutions)
			sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
			Serial.print("Resolution set to: ");
			Serial.print(sensors.getResolution(tempDeviceAddress), DEC);
			Serial.println();
			}else{
			Serial.print("Found ghost ");
			Serial.print(i, DEC);
			Serial.print(" Check power and cabling");
		}
	}
}
	
	String printAddress(DeviceAddress deviceAddress)
	{
		String myString;
		for (uint8_t i = 0; i < 8; i++)
		{
			if (deviceAddress[i] < 16){
				Serial.print("0");			
				myString = myString += "0";	
			}
			Serial.print(deviceAddress[i], HEX);
			myString = myString += (String(deviceAddress[i], HEX));
		}	
		myString.toUpperCase();
		return myString;
	}
	
	/* function to capture a device address to a string variable which is more suited for displaying*/
	String captureAddress(DeviceAddress deviceAddress)
	{
		String myString;
		
		for (uint8_t i = 0; i < 8; i++)
		{
			if (deviceAddress[i] < 16){
				myString = myString += "0";
			}
			myString = myString += (String(deviceAddress[i], HEX));  //this fills up myString with the characters in the deviceaddress array
		}
		myString.toUpperCase();
		return myString;
	}


#ifdef Mysql

void phpPost(){
		char string[17];
		char buf [100];
		char ip[16];
			sprintf(ip, "%d.%d.%d.%d", Ethernet.localIP()[0],Ethernet.localIP()[1],Ethernet.localIP()[2],Ethernet.localIP()[3]);
			//Serial.println(ip);
					
	for (int i = 0; i<SENSORNUM; i++)
	{
						
	if (i == 0) //if this is the first sensor
	{
		snprintf(buf,200,"GET /testserver/arduino_temperatures/add_data.php?"); //drop in the GET line
		client.print(buf);
		Serial.println(buf);
		buf[0] = 0; //reuse variable
	}			
	if (i > 0) {
		client.print ("&");
		Serial.print ("&");
	}   // add ampersand if not first sensor
	stringaddressArray[i][0].toCharArray(string,17);
	char tempFloat[7];
	dtostrf(tempArrayC[i],6,3,tempFloat);
	sprintf(buf,"serial%d=%s&temperature%d=%s", i+1, string, i+1, tempFloat);
	Serial.print(buf);
	client.print(buf);
	if (i >= (numberOfDevices - 1)){
		buf[0] = 0;
		Serial.println("");
			sprintf(buf," HTTP/1.1\nHost: %s\nHost: %s\nConnection: close\n", ip, server );
			Serial.println(buf);
			client.println(buf);
			buf[0] = 0;


		}	
	}
	
}


void webDump(){
	

	if (client.connect(server, 80)){
		
		#ifdef DEBUG
		Serial.println("-> Connected");  // only use serial when debugging
		#endif
		digitalWrite(RED_LED, HIGH);
		digitalWrite(GREEN_LED, LOW);
		digitalWrite(BLUE_LED, LOW);
		phpPost();
		digitalWrite(RED_LED, LOW);
		if (modeState == 0){
		heartBeat(GREEN_LED);
		}
		if (modeState == 1){
		heartBeat(BLUE_LED);
		}
	
		
	}
	else
	{
		#ifdef DEBUG
		Serial.println(client.connected());
		Serial.println("--> connection failed !!");  // only use serial when debugging
		#endif
		}
client.stop();  //client stop for all
	}
#endif

#ifdef Experimental

uint8_t get_next_count(const uint8_t count_limit) {
	// n cells to use --> 1/n wear per cll --> n times the life time
	const uint16_t cells_to_use = 128;
	
	// default cell to change
	uint8_t change_this_cell  = 0;
	// value of the default cell
	uint8_t change_value = EEPROM.read(change_this_cell);
	
	// will be used to aggregate the count_limit
	// must be able to hold values up to cells_to_use*255 + 1
	uint32_t count = change_value;
	
	for (uint16_t cell = 1; cell < cells_to_use; ++cell) {
		uint8_t value = EEPROM.read(cell);
		
		// determine current count by cummulating all cells
		count += value;
		
		if (value != change_value) {
			// at the same time find at least one cell that differs
			change_this_cell = cell;
		}
	}
	
	// Either a cell differs from cell 0 --> change it
	// Otherwise no cell differs from cell 0 --> change cell 0
	
	// Since a cell might initially hold a value of -1 the % operator must be applied twice
	EEPROM.write(change_this_cell, (EEPROM.read(change_this_cell) % count_limit + 1) % count_limit);
	
	// return the new count
	return (count + 1) % count_limit;
}


#endif
