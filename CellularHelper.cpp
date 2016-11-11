#include "Particle.h"

#include "CellularHelper.h"

// Updates to this code are here:
// https://github.com/rickkas7/electron_cellular

CellularHelperClass CellularHelper;


String CellularHelperClass::getOperatorName(int operatorNameType) const {
	String result;

	// The default is OPERATOR_NAME_LONG_EONS (9).
	// If the EONS name is not available, then the other things tried in order are:
	// NITZ, CPHS, ROM
	// So basically, something will be returned

	CellularHelperPlusStringResponse resp;

	plusStringResponseCommand(resp, DEFAULT_TIMEOUT, "AT+UDOPN=%d\r\n", operatorNameType);

	if (resp.result == RESP_OK) {
		result = resp.getDoubleQuotedPart();
	}

	return result;
}

CellularHelperRSSIQualResponse CellularHelperClass::getRSSIQual() const {
	CellularHelperRSSIQualResponse resp;
	plusStringResponseCommand(resp, DEFAULT_TIMEOUT, "AT+CSQ\r\n");

	if (resp.result == RESP_OK) {
		if (sscanf(resp.string.c_str(), "%d,%d", &resp.rssi, &resp.qual) != 2) {
			// Failed to parse result
			resp.result = RESP_ERROR;
		}
	}
	return resp;
}


CellularHelperEnvironmentResponse CellularHelperClass::getEnvironment(int mode) const {
	CellularHelperEnvironmentResponse resp;
	plusStringResponseCommand(resp, DEFAULT_TIMEOUT, "AT+CGED=%d\r\n", mode);

	if (resp.result == RESP_OK) {
		resp.postProcess();
	}
	return resp;
}

bool CellularHelperClass::ping(const char *addr) const {
	CellularHelperStringResponse resp;

	stringResponseCommand(resp, DEFAULT_TIMEOUT, "AT+UPING=\"%s\"\r\n", addr);

	return resp.result == RESP_OK;
}

IPAddress CellularHelperClass::dnsLookup(const char *hostname) const {
	IPAddress result;

	CellularHelperPlusStringResponse resp;
	plusStringResponseCommand(resp, DEFAULT_TIMEOUT, "AT+UDNSRN=0,\"%s\"\r\n", hostname);

	if (resp.result == RESP_OK) {
		String quotedPart = resp.getDoubleQuotedPart();
		int addr[4];
		if (sscanf(quotedPart.c_str(), "%u.%u.%u.%u", &addr[0], &addr[1], &addr[2], &addr[3]) == 4) {
			result = IPAddress(addr[0], addr[1], addr[2], addr[3]);
		}
	}

	return result;
}


// [static]
int CellularHelperClass::responseCallback(int type, const char* buf, int len, void *param) {
	CellularHelperCommonResponse *presp = (CellularHelperCommonResponse *)param;

	return presp->parse(type, buf, len);
}


int CellularHelperStringResponse::parse(int type, const char *buf, int len) {

	for(const char *cur = buf; cur < &buf[len]; cur++) {
		switch(state) {
		case CRLF_STATE:
			if (*cur == '\r' || *cur == '\n') {
				// Still in this state
				break;
			}
			state = NEW_LINE_DATA_STATE;
			// FALL THROUGH

		case NEW_LINE_DATA_STATE:
			if (*cur == '+') {
				state = IGNORE_REMAINDER_STATE;
				break;
			}
			else
			if (*cur == 'O') {
				state = K_WAIT_STATE;
				break;
			}
			state = DATA_STATE;
			// FALL THROUGH

		case DATA_STATE:
			if (*cur == '\r' || *cur == '\n') {
				state = CRLF_STATE;
				break;
			}
			string.concat(*cur);
			break;

		case IGNORE_REMAINDER_STATE:
			break;

		case K_WAIT_STATE:
			if (*cur == 'K') {
				state = IGNORE_REMAINDER_STATE;
				break;
			}
			// Got an O at the beginning of the line, but not the K, so put the O
			// in the buffer and re-parse this character
			string.concat('O');
			state = DATA_STATE;
			cur--;
			break;
		}
	}
	return WAIT;
}


int CellularHelperPlusStringResponse::parse(int type, const char *buf, int len) {

	for(const char *cur = buf; cur < &buf[len]; cur++) {
		switch(state) {
		case CRLF_STATE:
			if (*cur == '\r' || *cur == '\n') {
				// Still in this state
				break;
			}
			state = NEW_LINE_DATA_STATE;
			// FALL THROUGH

		case NEW_LINE_DATA_STATE:
			if (*cur == '+') {
				state = WAIT_FOR_COLON_STATE;
				break;
			}
			else
			if (*cur == 'O') {
				state = K_WAIT_STATE;
				break;
			}
			state = DATA_STATE;
			// FALL THROUGH

		case DATA_STATE:
			if (*cur == '\r' || *cur == '\n') {
				if (string.length() > 0) {
					parseLine();
				}
				state = CRLF_STATE;
				break;
			}
			string.concat(*cur);
			break;

		case IGNORE_REMAINDER_STATE:
			break;

		case K_WAIT_STATE:
			if (*cur == 'K') {
				state = IGNORE_REMAINDER_STATE;
				break;
			}
			// Got an O at the beginning of the line, but not the K, so put the O
			// in the buffer and re-parse this character
			string.concat('O');
			state = DATA_STATE;
			cur--;
			break;

		case WAIT_FOR_COLON_STATE:
			if (*cur == ':') {
				state = SKIP_SPACE_STATE;
			}
			break;

		case SKIP_SPACE_STATE:
			if (*cur == ' ') {
				break;
			}
			// Reparse the non-space character
			state = DATA_STATE;
			cur--;
			break;
		}
	}
	return WAIT;
}

// Subclasses will override this to handle each line of multi-line data
void CellularHelperPlusStringResponse::parseLine() {
}


String CellularHelperPlusStringResponse::getDoubleQuotedPart() const {
	String result;
	bool inQuoted = false;

	for(size_t ii = 0; ii < string.length(); ii++) {
		char ch = string.charAt(ii);
		if (ch == '"') {
			inQuoted = !inQuoted;
		}
		else {
			if (inQuoted) {
				result.concat(ch);
			}
		}
	}

	return result;
}


void CellularHelperEnvironmentCellData::parse(const char *str) {
	char *mutableCopy = strdup(str);

	char *pair = strtok(mutableCopy, ",");
	while(pair) {
		// Remove leading spaces caused by ", " combination
		while(*pair == ' ') {
			pair++;
		}

		char *colon = strchr(pair, ':');
		if (colon != NULL) {
			*colon = 0;
			const char *key = pair;
			const char *value = ++colon;

			addKeyValue(key, value);
		}

		pair = strtok(NULL, ",");
	}


	free(mutableCopy);
}

void CellularHelperEnvironmentCellData::addKeyValue(const char *key, const char *value) {
	if (strcmp(key, "RAT") == 0) {
		isUMTS = (strstr(value, "UMTS") != NULL);
	}
	else
	if (strcmp(key, "MCC") == 0) {
		mcc = atoi(value);
	}
	else
	if (strcmp(key, "MNC") == 0) {
		mnc = atoi(value);
	}
	else
	if (strcmp(key, "LAC") == 0) {
		lac = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(key, "CI") == 0) {
		ci = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(key, "BSIC") == 0) {
		bsic = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(key, "Arfcn") == 0) {
		arfcn = atoi(value);
	}
	else
	if (strcmp(key, "Arfcn_ded") == 0 || strcmp(key, "RxLevSub") == 0 || strcmp(key, "t_adv") == 0) {
		// Ignored 2G fields
	}
	else
	if (strcmp(key, "RxLev") == 0) {
		rxlev = (int) strtol(value, NULL, 16); // hex
	}
	else
	if (strcmp(key, "DLF") == 0) {
		dlf = atoi(value);
	}
	else
	if (strcmp(key, "ULF") == 0) {
		ulf = atoi(value);
	}
	else {
		Serial.printlnf("unknown key=%s value=%s", key, value);
	}

}

void CellularHelperEnvironmentCellData::postProcess() {
	//
	if (isUMTS) {
		// 3G radio
		if (ulf >= 0 && ulf <= 124) {
			band = "GSM 900";
		}
		else
		if (ulf >= 128 && ulf <= 251) {
			band = "GSM 850";
		}
		else
		if (ulf >= 512 && ulf <= 885) {
			band = "DCS 1800";
		}
		else
		if (ulf >= 975 && ulf <= 1023) {
			band = "ESGM 900";
		}
		else
		if (ulf >= 33280 && ulf <= 33578) {
			band = "PCS 1900";
		}
		else
		if (ulf >= 1312 && ulf <= 1513) {
			band = "UMTS 1700";
		}
		else
		if (ulf >= 2712 && ulf <= 2863) {
			band = "UMTS 900";
		}
		else
		if (ulf >= 4132 && ulf <= 4233) {
			band = "UMTS 850";
		}
		else
		if ((ulf >= 4162 && ulf <= 4188) || (ulf >= 20312 && ulf <= 20363)) {
			band = "UMTS 800";
		}
		else
		if (ulf >= 9262 && ulf <= 9538) {
			band = "UMTS 1900";
		}
		else
		if (ulf >= 9612 && ulf <= 9888) {
			band = "UMTS 2100";
		}
		else {
			Serial.printlnf("unknown UMTS band %d", ulf);
			band = "3G unknown";
		}
	}
	else {
		// 2G, use arfcn

		if (arfcn >= 0 && arfcn <= 124) {
			band = "GSM 900";
		}
		else
		if (arfcn >= 129 && arfcn <= 251) {
			band = "GSM 850";
		}
		else
		if (arfcn >= 512 && arfcn <= 885) {
			band = "DCS 1800 ";
		}
		else
		if (arfcn >= 975 && arfcn <= 1024) {
			band = "EGSM 900";
		}
		else
		if (arfcn >= 33280 && arfcn <= 33578) {
			band = "PCS 1900";
		}
		else {
			Serial.printlnf("unknown GSM band %d", arfcn);
			band = "2G unknown";
		}
	}
}

String CellularHelperEnvironmentCellData::toString() const {
	String common = String::format("mcc=%d, mnc=%d, lac=%x ci=%x band=%s", mcc, mnc, lac, ci, band.c_str());

	if (isUMTS) {
		return String::format("rat=UMTS %s dlf=%d ulf=%d", common.c_str(), dlf, ulf);
	}
	else {
		return String::format("rat=GSM %s bsic=%x arfcn=%x rxlev=%d", common.c_str(), bsic, arfcn, rxlev);
	}
}


void CellularHelperEnvironmentResponse::parseLine() {
	const char *str = string.c_str();

	// Serial.printlnf("parsing line %s", str);

	if (strstr(str, "Service") != NULL) {
		// Service Cell:
		curDataIndex = -1;
	}
	else
	if (strstr(str, "Neighbour") != NULL) {
		// Neighbour Cell 1:
		// Yes, the modem spells it the British way.
		curDataIndex++;
	}
	else {
		if (curDataIndex < 0) {
			service.parse(str);
		}
		else
		if ((size_t)curDataIndex < MAX_NEIGHBOR_CELLS) {
			neighbor[curDataIndex].parse(str);
		}
	}

	string = "";
}
void CellularHelperEnvironmentResponse::postProcess() {
	service.postProcess();
	for(size_t ii = 0; ii < MAX_NEIGHBOR_CELLS; ii++) {
		if (neighbor[ii].isValid()) {
			neighbor[ii].postProcess();
		}
	}

}


void CellularHelperEnvironmentResponse::serialDebug() const {
	Serial.printlnf("service %s", service.toString().c_str());
	for(size_t ii = 0; ii < MAX_NEIGHBOR_CELLS; ii++) {
		if (neighbor[ii].isValid()) {
			Serial.printlnf("neighbor %d %s", ii, neighbor[ii].toString().c_str());
		}
	}

}



