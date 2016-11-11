#ifndef __CELLULARHELPER_H
#define __CELLULARHELPER_H

#include "Particle.h"

// Class for quering infromation directly from the ublox SARA modem

/**
 * Base class for all responses. Contains the result code from
 */
class CellularHelperCommonResponse {
public:
	/**
	 * Result code from Cellular.command()
	 * https://docs.particle.io/reference/firmware/electron/#command-
	 *
	 * NOT_FOUND = 0
	 * WAIT = -1 // TIMEOUT
	 * RESP_OK = -2
	 * RESP_ERROR = -3
	 * RESP_PROMPT = -4
	 * RESP_ABORTED = -5
	 */
	int result;

	virtual int parse(int type, const char *buf, int len) = 0;
};

/**
 * Used for commands that return a string before the +, like AT+CGMM
 */
class CellularHelperStringResponse : public CellularHelperCommonResponse {
public:
	String string;

	virtual int parse(int type, const char *buf, int len);

	static const int CRLF_STATE = 0;
	static const int NEW_LINE_DATA_STATE = 1;
	static const int DATA_STATE = 2;
	static const int IGNORE_REMAINDER_STATE = 3;
	static const int K_WAIT_STATE = 4;

private:
	int state = CRLF_STATE;
};

/**
 * Used for commands that return a result after the +, like AT+UDOPN
 *
 * Classes like CellularHelperRSSIQualResponse and CellularHelperEnvironmentResponse
 * subclass this to add additional parsing
 */
class CellularHelperPlusStringResponse : public CellularHelperCommonResponse{
public:
	String string;

	virtual int parse(int type, const char *buf, int len);

	String getDoubleQuotedPart() const;

	virtual void parseLine();

	static const int CRLF_STATE = 0;
	static const int NEW_LINE_DATA_STATE = 1;
	static const int DATA_STATE = 2;
	static const int IGNORE_REMAINDER_STATE = 3;
	static const int K_WAIT_STATE = 4;
	static const int WAIT_FOR_COLON_STATE = 5;
	static const int SKIP_SPACE_STATE = 6;

private:
	int state = CRLF_STATE;
};

/**
 * Used to hold the results of the AT+CSQ command that returns the RSSI and qual values
 */
class CellularHelperRSSIQualResponse : public CellularHelperPlusStringResponse {
public:
	int rssi = 0;
	int qual = 0;


};

/**
 * Used to hold the results for one cell (service or neighbor) from the AT+CGED command
 */
class CellularHelperEnvironmentCellData {
public:
	int mcc = 65535; 		// Mobile Country Code, range 0 - 999 (3 digits). Other values are to be considered invalid / not available
	int mnc = 255; 			// Mobile Network Code, range 0 - 999 (1 to 3 digits). Other values are to be considered invalid / not available
	int lac; 				// Location Area Code, range 0h-FFFFh (2 octets)
	int ci; 				// Cell Identity: 2G cell: range 0h-FFFFh (2 octets); 3G cell: range 0h-FFFFFFFh (28 bits)
	int bsic; 				// Base Station Identify Code, range 0h-3Fh (6 bits) [2G]
	int arfcn; 				// Absolute Radio Frequency Channel Number, range 0 - 1023 [2G]
							// The parameter value also decodes the band indicator bit (DCS or PCS) by means of the
							// most significant byte (8 means 1900 band) (i.e. if the parameter reports the value 33485, it corresponds to 0x82CD, in the most significant byte there is the band indicator bit, so the <arfcn> is 0x2CD (717) and belongs to 1900 band).
	int rxlev;				// Received signal level on the cell, range 0 - 63; see the 3GPP TS 05.08 [2G]
	bool isUMTS = false;	// RAT is GSM (false) or UMTS (true)
	int dlf;				// Downlink frequency. Range 0 - 16383 [3G]
	int ulf;				// Uplink frequency. Range 0 - 16383 [3G]
	String band;			// Human readable frequency band ("GSM 900" for example)

	inline bool isValid() const {
		return (mcc <= 999);
	}
	void parse(const char *str);
	void addKeyValue(const char *key, const char *value);
	void postProcess();
	String toString() const;
};

/**
 * Used to hold the results from the AT+CGED command
 */
class CellularHelperEnvironmentResponse : public CellularHelperPlusStringResponse {
public:
	static const size_t MAX_NEIGHBOR_CELLS = 5;

	CellularHelperEnvironmentCellData service;
	CellularHelperEnvironmentCellData neighbor[MAX_NEIGHBOR_CELLS];
	int curDataIndex = -1;

	virtual void parseLine();
	void postProcess();
	void serialDebug() const;
};


/**
 * Class for calling the ublox SARA modem directly
 */
class CellularHelperClass {
public:
	/**
	 * Used for commands that return a string before the +, like AT+CGMM
	 */
	template<typename... Targs>
	inline void stringResponseCommand(CellularHelperStringResponse &resp, system_tick_t timeout_ms, const char* format, Targs... Fargs) const {
		resp.result = cellular_command(responseCallback, &resp, timeout_ms, format, Fargs...);
	}

	template<typename... Targs>
	inline String simpleStringResponseCommand(const char* format, Targs... Fargs) const {
		CellularHelperStringResponse resp;

		stringResponseCommand(resp, DEFAULT_TIMEOUT, "AT%s\r\n", format, Fargs...);

		if (resp.result != RESP_OK) {
			resp.string = "";
		}
		return resp.string;
	}

	/**
	 * Used for commands that return a result after the +, like AT+UDOPN
	 */
	template<typename... Targs>
	inline void plusStringResponseCommand(CellularHelperPlusStringResponse &resp, system_tick_t timeout_ms, const char* format, Targs... Fargs) const {
		resp.result = cellular_command(responseCallback, &resp, timeout_ms, format, Fargs...);
	}

	template<typename... Targs>
	inline String simplePlusStringResponseCommand(const char* format, Targs... Fargs) const {
		CellularHelperPlusStringResponse resp;

		plusStringResponseCommand(resp, DEFAULT_TIMEOUT, "AT%s\r\n", format, Fargs...);

		if (resp.result != RESP_OK) {
			resp.string = "";
		}
		return resp.string;
	}

	/**
	 * Gets the manufacturer string. For example "u-blox" (without the quotes).
	 */
	inline String getManufacturer() const {
		return simpleStringResponseCommand("+CGMI");
	}

	/**
	 * Gets the model of the modem. For example: "SARA-U260" or "SARA-G350" (without the quotes).
	 */
	inline String getModel() const {
		return simpleStringResponseCommand("+CGMM");
	}

	/**
	 * Gets the ordering code of the modem. For example "SARA-U260-00S-00" (without the quotes).
	 */
	inline String getOrderingCode() const {
		return simpleStringResponseCommand("I0");
	}

	/**
	 * Gets the modem firmware version string. For example: "23.30" (without the quotes).
	 */
	inline String getFirmwareVersion() const {
		return simpleStringResponseCommand("+CGMR");
	}

	/**
	 * Gets the IMEI value (International Mobile Equipment Identity)
	 */
	inline String getIMEI() const {
		return simpleStringResponseCommand("+CGSN");
	}

	/**
	 * Gets the IMSI value (International Mobile Subscriber Identity)
	 */
	inline String getIMSI() const {
		return simpleStringResponseCommand("+CIMI");
	}

	/**
	 * Gets the ICCID of the SIM card (Integrated Circuit Card ID)
	 */
	inline String getICCID() const {
		return simplePlusStringResponseCommand("+CCID");
	}

	/**
	 * Gets the name of the operator, for example "AT&T".
	 */
	String getOperatorName(int operatorNameType = OPERATOR_NAME_LONG_EONS) const;

	/**
	 * Get the RSSI and qual values for the receiving cell site.
	 *
	 * The qual value is always 99 for me on the G350 (2G).
	 */
	CellularHelperRSSIQualResponse getRSSIQual() const;

	/**
	 * Gets environment information
	 *
	 * There are many modes, but this method is typically used either with mode 3
	 * (receiving cell info) and 5 (receiving cell and neighboring cell info). Mode
	 * 5 doesn't work for me on the U260, but it does work on the G350.
	 */
	CellularHelperEnvironmentResponse getEnvironment(int mode) const;


	/**
	 * Pings an address. Typically a dotted octet string, but can pass a hostname as well.
	 *
	 * If you have a Particle IPAddress, you can use ipaddr.toString() to get the dotted octet string
	 * that this method wants
	 */
	bool ping(const char *addr) const;

	/**
	 * Looks up the IPAddress of a hostname
	 */
	IPAddress dnsLookup(const char *hostname) const;

	// Default timeout in milliseconds
	static const system_tick_t DEFAULT_TIMEOUT = 10000;

	/**
	 * Constants for getOperatorName. Default and recommended value is OPERATOR_NAME_LONG_EONS.
	 */
	static const int OPERATOR_NAME_NUMERIC = 0;
	static const int OPERATOR_NAME_SHORT_ROM = 1;
	static const int OPERATOR_NAME_LONG_ROM = 2;
	static const int OPERATOR_NAME_SHORT_CPHS = 3;
	static const int OPERATOR_NAME_LONG_CPHS = 4;
	static const int OPERATOR_NAME_SHORT_NITZ = 5;
	static const int OPERATOR_NAME_LONG_NITZ = 6;
	static const int OPERATOR_NAME_SERVICE_PROVIDER = 7;
	static const int OPERATOR_NAME_SHORT_EONS = 8;
	static const int OPERATOR_NAME_LONG_EONS = 9;
	static const int OPERATOR_NAME_SHORT_NETWORK_OPERATOR = 11;
	static const int OPERATOR_NAME_LONG_NETWORK_OPERATOR = 11;


protected:
	static int responseCallback(int type, const char* buf, int len, void *param);


};

extern CellularHelperClass CellularHelper;

#endif /* __CELLULARHELPER_H */
