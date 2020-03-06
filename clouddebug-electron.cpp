#include "Particle.h"

#include "CellularHelper.h"

// In order to use this with full debugging, you need a debug build of system firmware
// as well as this app.

// Adds debugging output over Serial USB
// ALL_LEVEL, TRACE_LEVEL, DEBUG_LEVEL, INFO_LEVEL, WARN_LEVEL, ERROR_LEVEL, PANIC_LEVEL, NO_LOG_LEVEL
SerialLogHandler logHandler(LOG_LEVEL_INFO);

// STARTUP(cellular_credentials_set("broadband", "", "", NULL));

SYSTEM_MODE(SEMI_AUTOMATIC);
// SYSTEM_THREAD(ENABLED);

/* Function prototypes -------------------------------------------------------*/
int tinkerDigitalRead(String pin);
int tinkerDigitalWrite(String command);
int tinkerAnalogRead(String pin);
int tinkerAnalogWrite(String command);
void buttonHandler();
void enableTraceLogging();
void runModemTests();
void runCellularTests();
void runTowerTest();
void printCellData(CellularHelperEnvironmentCellData *data);


/* Constants -----------------------------------------------------------------*/
const unsigned long STARTUP_WAIT_TIME_MS = 5000;
const unsigned long COMMAND_WAIT_TIME_MS = 10000;
const unsigned long MODEM_ON_WAIT_TIME_MS = 4000;
const size_t INPUT_BUF_SIZE = 256;

/* Global Variables-----------------------------------------------------------*/

enum State { STARTUP_STATE,
	COMMAND_STATE, COMMAND_WAIT_STATE,
	ENTER_APN_STATE, ENTER_KEEPALIVE_STATE,
	CELLULAR_ON_STATE, CELLULAR_ON_WAIT_STATE, MODEM_REPORT_STATE,
	CONNECT_WAIT_STATE, CONNECT_REPORT_STATE,
	CLOUD_CONNECT_WAIT_STATE, CLOUD_CONNECTED_STATE,
	TOWER_REPORT_STATE,
	DISCONNECT_STATE, IDLE_STATE };
State state = STARTUP_STATE;
State postCellularOnState;
unsigned long stateTime = 0;
CellularHelperEnvironmentResponseStatic<32> envResp;
size_t inputBufferOffset;
char inputBuffer[INPUT_BUF_SIZE];
int keepAlive = 0;
bool buttonClicked = false;

/* This function is called once at start up ----------------------------------*/
void setup()
{
	Serial.begin(9600);

	System.on(button_click, buttonHandler);

	// When using a 3rd-party SIM card, be sure to set the keepAlive
	//Particle.keepAlive(60);

	//Setup the Tinker application here

	//Register all the Tinker functions
	Particle.function("digitalread", tinkerDigitalRead);
	Particle.function("digitalwrite", tinkerDigitalWrite);

	Particle.function("analogread", tinkerAnalogRead);
	Particle.function("analogwrite", tinkerAnalogWrite);
}

/* This function loops forever --------------------------------------------*/
void loop() {
	int serialData;


	switch(state) {

	case STARTUP_STATE:
		if (millis() - stateTime >= STARTUP_WAIT_TIME_MS) {
			state = COMMAND_STATE;
			stateTime = millis();
		}
		break;

	case COMMAND_STATE:
		Serial.println("clouddebug: press letter corresponding to the command");
		Serial.println("a - enter APN for 3rd-party SIM card");
		Serial.println("k - set keep-alive value");
		Serial.println("c - show carriers at this location");
		Serial.println("t - run normal tests (occurs automatically after 10 seconds)");
		Serial.println("or tap the MODE button once to show carriers");
		state = COMMAND_WAIT_STATE;
		stateTime = millis();
		break;

	case COMMAND_WAIT_STATE:
		if (buttonClicked) {
			buttonClicked = false;

			Serial.printlnf("starting carrier report...");
			postCellularOnState = TOWER_REPORT_STATE;
			state = CELLULAR_ON_STATE;
			stateTime = millis();
		}
		else
		if (millis() - stateTime >= COMMAND_WAIT_TIME_MS) {
			// No command entered in the timeout time
			Serial.printlnf("starting tests...");
			postCellularOnState = MODEM_REPORT_STATE;
			state = CELLULAR_ON_STATE;
			stateTime = millis();
			break;
		}
		while(Serial.available()) {
			serialData = Serial.read();
			switch(serialData) {
			case 'A':
			case 'a':
				Serial.print("enter APN: ");
				state = ENTER_APN_STATE;
				inputBufferOffset = 0;
				break;

			case 'K':
			case 'k':
				Serial.print("enter keep-alive value in seconds (typically 30 to 1380): ");
				state = ENTER_KEEPALIVE_STATE;
				inputBufferOffset = 0;
				break;

			case 'C':
			case 'c':
				Serial.printlnf("starting carrier report...");
				postCellularOnState = TOWER_REPORT_STATE;
				state = CELLULAR_ON_STATE;
				stateTime = millis();
				break;

			case 'T':
			case 't':
				Serial.printlnf("starting tests...");
				postCellularOnState = MODEM_REPORT_STATE;
				state = CELLULAR_ON_STATE;
				stateTime = millis();
				break;

			case '\r':
			case '\n':
				// Repeat help
				state = COMMAND_STATE;
				break;
			}
		}
		break;

	case ENTER_KEEPALIVE_STATE:
	case ENTER_APN_STATE:
		while(Serial.available()) {
			serialData = Serial.read();
			if (serialData == '\r' || serialData == '\n') {
				if (inputBufferOffset > 0) {
					inputBuffer[inputBufferOffset] = 0;
					Serial.println();

					if (state == ENTER_APN_STATE) {
						cellular_credentials_set(inputBuffer, "", "", NULL);
						Serial.printlnf("APN set to %s", inputBuffer);
					}
					else {
						keepAlive = atoi(inputBuffer);
						if (keepAlive >= 10) {
							Serial.printlnf("keepalive will be set to %d", keepAlive);
						}
					}
					state = COMMAND_STATE;
					stateTime = millis();
				}
			}
			else
			if (inputBufferOffset < (INPUT_BUF_SIZE - 1)) {
				inputBuffer[inputBufferOffset++] = serialData;
			}
		}
		break;

	case CELLULAR_ON_STATE:
		Serial.println("turning cellular on...");
		Cellular.on();
		state = CELLULAR_ON_WAIT_STATE;
		stateTime = millis();
		break;

	case CELLULAR_ON_WAIT_STATE:
		if (millis() - stateTime >= MODEM_ON_WAIT_TIME_MS) {
			state = postCellularOnState;
		}
		break;

	case MODEM_REPORT_STATE:
		// Print things like the ICCID
		runModemTests();

		// Turn trace level on now. We leave it off for carrier report as it prints too much junk.
		enableTraceLogging();

		Serial.println("attempting to connect to the cellular network...");
		Cellular.connect();

		state = CONNECT_WAIT_STATE;
		stateTime = millis();
		break;

	case CONNECT_WAIT_STATE:
		if (Cellular.ready()) {
			unsigned long elapsed = millis() - stateTime;

			Serial.printlnf("connected to the cellular network in %lu milliseconds", elapsed);
			state = CONNECT_REPORT_STATE;
			stateTime = millis();
		}
		else
		if (Cellular.listening()) {
			Serial.println("entered listening mode (blinking dark blue) - probably no SIM installed");
			state = IDLE_STATE;
		}

		break;

	case TOWER_REPORT_STATE:
		runTowerTest();
		state = COMMAND_STATE;
		break;

	case CONNECT_REPORT_STATE:
		{
			Serial.println("connected to cellular network!");
			runCellularTests();

			Serial.println("connecting to cloud");
			Particle.connect();
		}

		state = CLOUD_CONNECT_WAIT_STATE;
		break;

	case CLOUD_CONNECT_WAIT_STATE:
		if (Particle.connected()) {
			Serial.println("connected to the cloud!");
			state = CLOUD_CONNECTED_STATE;

			if (keepAlive >= 10) {
				Log.info("keepAlive set to %d", keepAlive);
				Particle.keepAlive(keepAlive);
			}

		}
		break;

	case IDLE_STATE:
	case CLOUD_CONNECTED_STATE:
		break;

	}

}

void buttonHandler() {
	buttonClicked = true;
}

void enableTraceLogging() {
	Log.info("enabling trace logging");

	// Get log manager's instance
	auto logManager = LogManager::instance();

	// Configure and register log handler dynamically
	auto handler = new StreamLogHandler(Serial, LOG_LEVEL_TRACE);
	logManager->addHandler(handler);
}

void runModemTests() {
	Serial.printlnf("deviceID=%s", System.deviceID().c_str());

	Serial.printlnf("manufacturer=%s", CellularHelper.getManufacturer().c_str());

	Serial.printlnf("model=%s", CellularHelper.getModel().c_str());

	Serial.printlnf("firmware version=%s", CellularHelper.getFirmwareVersion().c_str());

	Serial.printlnf("ordering code=%s", CellularHelper.getOrderingCode().c_str());

	Serial.printlnf("IMEI=%s", CellularHelper.getIMEI().c_str());

	Serial.printlnf("IMSI=%s", CellularHelper.getIMSI().c_str());

	Serial.printlnf("ICCID=%s", CellularHelper.getICCID().c_str());

}


// Various tests to find out information about the cellular network we connected to
void runCellularTests() {

	Serial.printlnf("operator name=%s", CellularHelper.getOperatorName().c_str());

	CellularHelperRSSIQualResponse rssiQual = CellularHelper.getRSSIQual();
	Serial.printlnf("rssi=%d, qual=%d", rssiQual.rssi, rssiQual.qual);

	// First try to get info on neighboring cells. This doesn't work for me using the U260
	envResp.clear();
	CellularHelper.getEnvironment(5, envResp);

	if (envResp.resp != RESP_OK) {
		// We couldn't get neighboring cells, so try just the receiving cell
		CellularHelper.getEnvironment(3, envResp);
	}
	envResp.logResponse();

}

class OperatorName {
public:
	int mcc;
	int mnc;
	String name;
};

class CellularHelperCOPNResponse : public CellularHelperCommonResponse {
public:
	CellularHelperCOPNResponse();

	void requestOperator(CellularHelperEnvironmentCellData *data);
	void requestOperator(int mcc, int mnc);
	void checkOperator(char *buf);
	const char *getOperatorName(int mcc, int mnc) const;

	virtual int parse(int type, const char *buf, int len);

	// Maximum number of operator names that can be looked up
	static const size_t MAX_OPERATORS = 16;

private:
	size_t numOperators;
	OperatorName operators[MAX_OPERATORS];
};
CellularHelperCOPNResponse copnResp;


CellularHelperCOPNResponse::CellularHelperCOPNResponse() : numOperators(0) {

}

void CellularHelperCOPNResponse::requestOperator(CellularHelperEnvironmentCellData *data) {
	if (data && data->isValid(true)) {
		requestOperator(data->mcc, data->mnc);
	}
}

void CellularHelperCOPNResponse::requestOperator(int mcc, int mnc) {
	for(size_t ii = 0; ii < numOperators; ii++) {
		if (operators[ii].mcc == mcc && operators[ii].mnc == mnc) {
			// Already requested
			return;
		}
	}
	if (numOperators < MAX_OPERATORS) {
		// There is room to request another
		operators[numOperators].mcc = mcc;
		operators[numOperators].mnc = mnc;
		numOperators++;
	}
}

const char *CellularHelperCOPNResponse::getOperatorName(int mcc, int mnc) const {
	for(size_t ii = 0; ii < numOperators; ii++) {
		if (operators[ii].mcc == mcc && operators[ii].mnc == mnc) {
			return operators[ii].name.c_str();
		}
	}
	return "unknown";
}


void CellularHelperCOPNResponse::checkOperator(char *buf) {
	if (buf[0] == '"') {
		char *numStart = &buf[1];
		char *numEnd = strchr(numStart, '"');
		if (numEnd && ((numEnd - numStart) == 6)) {
			char temp[4];
			temp[3] = 0;
			strncpy(temp, numStart, 3);
			int mcc = atoi(temp);

			strncpy(temp, numStart + 3, 3);
			int mnc = atoi(temp);

			*numEnd = 0;
			char *nameStart = strchr(numEnd + 1, '"');
			if (nameStart) {
				nameStart++;
				char *nameEnd = strchr(nameStart, '"');
				if (nameEnd) {
					*nameEnd = 0;

					// Log.info("mcc=%d mnc=%d name=%s", mcc, mnc, nameStart);

					for(size_t ii = 0; ii < numOperators; ii++) {
						if (operators[ii].mcc == mcc && operators[ii].mnc == mnc) {
							operators[ii].name = String(nameStart);
						}
					}
				}
			}
		}
	}
}


int CellularHelperCOPNResponse::parse(int type, const char *buf, int len) {
	if (enableDebug) {
		logCellularDebug(type, buf, len);
	}
	if (type == TYPE_PLUS) {
		// Copy to temporary string to make processing easier
		char *copy = (char *) malloc(len + 1);
		if (copy) {
			strncpy(copy, buf, len);
			copy[len] = 0;

			/*
			 	0000018684 [app] INFO: +COPN: "901012","MCP Maritime Com"\r\n
				0000018694 [app] INFO: cellular response type=TYPE_PLUS len=28
				0000018694 [app] INFO: \r\n
				0000018695 [app] INFO: +COPN: "901021","Seanet"\r\n
				0000018705 [app] INFO: cellular response type=TYPE_ERROR len=39
				0000018705 [app] INFO: \r\n
			 *
			 */

			char *start = strstr(copy, "\n+COPN: ");
			if (start) {
				start += 8; // length of COPN string

				char *end = strchr(start, '\r');
				if (end) {
					*end = 0;

					checkOperator(start);
				}
			}

			free(copy);
		}
	}
	return WAIT;
}

void printCellData(CellularHelperEnvironmentCellData *data) {
	const char *whichG = data->isUMTS ? "3G" : "2G";

	// Serial.printlnf("mcc=%d mnc=%d", data->mcc, data->mnc);

	const char *operatorName = copnResp.getOperatorName(data->mcc, data->mnc);

	Serial.printlnf("%s %s %s %d bars", whichG, operatorName, data->getBandString().c_str(), data->getBars());
}


void runTowerTest() {
	envResp.clear();
	envResp.enableDebug = true;

	const char *model = CellularHelper.getModel().c_str();
	if (strncmp(model, "SARA-R4", 7) == 0) {
		Serial.println("Carrier report not available on LTE (SARA-R4)");
		return;
	}


	Serial.println("looking up operators (this may take up to 3 minutes)...");


	// Command may take up to 3 minutes to execute!
	envResp.resp = Cellular.command(CellularHelperClass::responseCallback, (void *)&envResp, 360000, "AT+COPS=5\r\n");
	if (envResp.resp == RESP_OK) {
		envResp.logResponse();

		copnResp.requestOperator(&envResp.service);
		if (envResp.neighbors) {
			for(size_t ii = 0; ii < envResp.numNeighbors; ii++) {
				copnResp.requestOperator(&envResp.neighbors[ii]);
			}
		}
	}


	Serial.printlnf("looking up operator names...");

	copnResp.enableDebug = false;
	copnResp.resp = Cellular.command(CellularHelperClass::responseCallback, (void *)&copnResp, 120000, "AT+COPN\r\n");

	Serial.printlnf("results...");

	printCellData(&envResp.service);
	if (envResp.neighbors) {
		for(size_t ii = 0; ii < envResp.numNeighbors; ii++) {
			if (envResp.neighbors[ii].isValid(true /* ignoreCI */)) {
				printCellData(&envResp.neighbors[ii]);
			}
		}
	}
}



/*******************************************************************************
 * Function Name  : tinkerDigitalRead
 * Description    : Reads the digital value of a given pin
 * Input          : Pin
 * Output         : None.
 * Return         : Value of the pin (0 or 1) in INT type
                    Returns a negative number on failure
 *******************************************************************************/
int tinkerDigitalRead(String pin)
{
	//convert ascii to integer
	int pinNumber = pin.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber < 0 || pinNumber > 7) return -1;

	if(pin.startsWith("D"))
	{
		pinMode(pinNumber, INPUT_PULLDOWN);
		return digitalRead(pinNumber);
	}
	else if (pin.startsWith("A"))
	{
		pinMode(pinNumber+10, INPUT_PULLDOWN);
		return digitalRead(pinNumber+10);
	}
#if Wiring_Cellular
	else if (pin.startsWith("B"))
	{
		if (pinNumber > 5) return -3;
		pinMode(pinNumber+24, INPUT_PULLDOWN);
		return digitalRead(pinNumber+24);
	}
	else if (pin.startsWith("C"))
	{
		if (pinNumber > 5) return -4;
		pinMode(pinNumber+30, INPUT_PULLDOWN);
		return digitalRead(pinNumber+30);
	}
#endif
	return -2;
}

/*******************************************************************************
 * Function Name  : tinkerDigitalWrite
 * Description    : Sets the specified pin HIGH or LOW
 * Input          : Pin and value
 * Output         : None.
 * Return         : 1 on success and a negative number on failure
 *******************************************************************************/
int tinkerDigitalWrite(String command)
{
	bool value = 0;
	//convert ascii to integer
	int pinNumber = command.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber < 0 || pinNumber > 7) return -1;

	if(command.substring(3,7) == "HIGH") value = 1;
	else if(command.substring(3,6) == "LOW") value = 0;
	else return -2;

	if(command.startsWith("D"))
	{
		pinMode(pinNumber, OUTPUT);
		digitalWrite(pinNumber, value);
		return 1;
	}
	else if(command.startsWith("A"))
	{
		pinMode(pinNumber+10, OUTPUT);
		digitalWrite(pinNumber+10, value);
		return 1;
	}
#if Wiring_Cellular
	else if(command.startsWith("B"))
	{
		if (pinNumber > 5) return -4;
		pinMode(pinNumber+24, OUTPUT);
		digitalWrite(pinNumber+24, value);
		return 1;
	}
	else if(command.startsWith("C"))
	{
		if (pinNumber > 5) return -5;
		pinMode(pinNumber+30, OUTPUT);
		digitalWrite(pinNumber+30, value);
		return 1;
	}
#endif
else return -3;
}

/*******************************************************************************
 * Function Name  : tinkerAnalogRead
 * Description    : Reads the analog value of a pin
 * Input          : Pin
 * Output         : None.
 * Return         : Returns the analog value in INT type (0 to 4095)
                    Returns a negative number on failure
 *******************************************************************************/
int tinkerAnalogRead(String pin)
{
	//convert ascii to integer
	int pinNumber = pin.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits
	if (pinNumber < 0 || pinNumber > 7) return -1;

	if(pin.startsWith("D"))
	{
		return -3;
	}
	else if (pin.startsWith("A"))
	{
		return analogRead(pinNumber+10);
	}
#if Wiring_Cellular
	else if (pin.startsWith("B"))
	{
		if (pinNumber < 2 || pinNumber > 5) return -3;
		return analogRead(pinNumber+24);
	}
#endif
	return -2;
}

/*******************************************************************************
 * Function Name  : tinkerAnalogWrite
 * Description    : Writes an analog value (PWM) to the specified pin
 * Input          : Pin and Value (0 to 255)
 * Output         : None.
 * Return         : 1 on success and a negative number on failure
 *******************************************************************************/
int tinkerAnalogWrite(String command)
{
	String value = command.substring(3);

	if(command.substring(0,2) == "TX")
	{
		pinMode(TX, OUTPUT);
		analogWrite(TX, value.toInt());
		return 1;
	}
	else if(command.substring(0,2) == "RX")
	{
		pinMode(RX, OUTPUT);
		analogWrite(RX, value.toInt());
		return 1;
	}

	//convert ascii to integer
	int pinNumber = command.charAt(1) - '0';
	//Sanity check to see if the pin numbers are within limits

	if (pinNumber < 0 || pinNumber > 7) return -1;

	if(command.startsWith("D"))
	{
		pinMode(pinNumber, OUTPUT);
		analogWrite(pinNumber, value.toInt());
		return 1;
	}
	else if(command.startsWith("A"))
	{
		pinMode(pinNumber+10, OUTPUT);
		analogWrite(pinNumber+10, value.toInt());
		return 1;
	}
	else if(command.substring(0,2) == "TX")
	{
		pinMode(TX, OUTPUT);
		analogWrite(TX, value.toInt());
		return 1;
	}
	else if(command.substring(0,2) == "RX")
	{
		pinMode(RX, OUTPUT);
		analogWrite(RX, value.toInt());
		return 1;
	}
#if Wiring_Cellular
	else if (command.startsWith("B"))
	{
		if (pinNumber > 3) return -3;
		pinMode(pinNumber+24, OUTPUT);
		analogWrite(pinNumber+24, value.toInt());
		return 1;
	}
	else if (command.startsWith("C"))
	{
		if (pinNumber < 4 || pinNumber > 5) return -4;
		pinMode(pinNumber+30, OUTPUT);
		analogWrite(pinNumber+30, value.toInt());
		return 1;
	}
#endif
else return -2;
}
