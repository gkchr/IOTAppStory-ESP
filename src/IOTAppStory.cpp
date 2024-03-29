#include "IOTAppStory.h"

IOTAppStory::IOTAppStory(const char *compDate, const int modeButton)
: _compDate(compDate)
, _modeButton(modeButton)
{
	#if DEBUG_LVL >= 1
		Serial.begin(SERIAL_SPEED);
		while (!Serial){
			delay(10);
		}
		DEBUG_PRINT(F("\n\n\n\n\n"));
	#endif
}



/**
	THIS ONLY RUNS ON THE FIRST BOOT OF A JUST INSTALLED APP (OR AFTER RESET TO DEFAULT SETTINGS)
*/
void IOTAppStory::firstBoot(){
	
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(F(" First boot sequence..."));
	#endif
	
	if (_firstBootCallback){
		
		#if DEBUG_LVL >= 3
			DEBUG_PRINTLN(SER_CALLBACK_FIRST_BOOT);
		#endif
		_firstBootCallback();
		delay(100);
	}
	
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);
	
	// reset boardMode & bootTimes
	boardMode = 'N';
	bootTimes = 0;
	boardInfo boardInfo(bootTimes, boardMode);
	boardInfo.write();
	
	// overwrite save compile date with the current compile date
	strcpy(config.compDate, _compDate);
	
	// write config to eeprom
	this->writeConfig(config);
	
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(F(" Reset for fresh start!"));
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif
    ESP.restart();
}



void IOTAppStory::preSetAppName(String appName){
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);

	_setPreSet = false;
	SetConfigValueCharArray(config.appName, appName, 33, _setPreSet);

	if(_setPreSet){
		// write config to EEPROM
		this->writeConfig(config);
	}
}
void IOTAppStory::preSetAppVersion(String appVersion){
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);

	_setPreSet = false;
	SetConfigValueCharArray(config.appVersion, appVersion, 11, _setPreSet);

	if(_setPreSet){
		// write config to EEPROM
		this->writeConfig(config);
	}
}
void IOTAppStory::preSetDeviceName(String deviceName){
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);

	_setPreSet = false;
	SetConfigValueCharArray(config.deviceName, deviceName, STRUCT_BNAME_SIZE, _setPreSet);

	if(_setPreSet){
		// write config to EEPROM
		this->writeConfig(config);
	}
}
void IOTAppStory::preSetAutoUpdate(bool automaticUpdate){
	_updateOnBoot = automaticUpdate;
}

void IOTAppStory::preSetAutoConfig(bool automaticConfig){
	_automaticConfig = automaticConfig;
}

void IOTAppStory::preSetWifi(const char *ssid, const char *password){
	// Save Wifi presets if newer
	WiFiConnector WiFiConn;
	if(strcmp(WiFiConn.getSSIDfromEEPROM(1), ssid) != 0){
		WiFiConn.addAPtoEEPROM(ssid, password, 1);
	}
}



void IOTAppStory::setCallHome(bool callHome) {} // <----- deprecated left for compatibility. Remove with version 3.0.0

void IOTAppStory::setCallHomeInterval(unsigned long interval) {
	_callHomeInterval = interval * 1000; //Convert to millis so users can pass seconds to this function
}



void IOTAppStory::begin(const char ea){ // <----- deprecated left for compatibility. This will be removed with version 3.0.0
	this->begin();
}
void IOTAppStory::begin(){
	{
		
		// get config from EEPROM
		configStruct config;
		this->readConfig(config);
		

		
		// on first boot of the app run the firstBoot() function
		if(strcmp(config.compDate, _compDate) != 0){
			this->firstBoot();
		}

		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(FPSTR(SER_DEV));
			
			DEBUG_PRINT(SER_START);
			DEBUG_PRINT(config.appName);
			DEBUG_PRINT(F(" v"));
			DEBUG_PRINTLN(config.appVersion);

			#if DEBUG_LVL >= 2
				DEBUG_PRINTLN(FPSTR(SER_DEV));
				DEBUG_PRINTF_P(SER_MODE_SEL_BTN, _modeButton, config.deviceName, _updateOnBoot);
			#endif

			DEBUG_PRINTLN(FPSTR(SER_DEV));
		#endif
	
		// set the input pin for Config/Update mode selection
		pinMode(_modeButton, INPUT_PULLUP);
		
		// set the "hard" reset(power) pin for the Nextion display
		// and turn the display on
		#if OTA_UPD_CHECK_NEXTION == true
			pinMode(NEXT_RES, OUTPUT);
			digitalWrite(NEXT_RES, HIGH);
		#endif

		{
		// Read the "bootTime" & "boardMode" from the Non-volatile storage on ESP32 processor
		boardInfo boardInfo(bootTimes, boardMode);
		boardInfo.read();
		
		// BOOT STATISTICS read and increase boot statistics (optional)
		#if BOOTSTATISTICS == true && DEBUG_LVL >= 1
			bootTimes++;
			boardInfo.write();
			
			#if DEBUG_LVL >= 1
				printBoardInfo();
			#endif
		#endif
		}

		// --------- START WIFI --------------------------
		// Setup wifi with cred etc connect to AP
		WiFiSetupAndConnect();
		

		// Synchronize time useing SNTP. This is necessary to verify that
		// the TLS certificates offered by servers are currently valid.
		#if SNTP_INT_CLOCK_UPD == true
			if(_connected){
				this->setClock();
			}
		#endif
		
		//---------- SELECT BOARD MODE -----------------------------
		#if CFG_INCLUDE == true
			if(boardMode == 'C'){
				{
					// callback entered config mode
					if(_configModeCallback){
						_configModeCallback();
					}
					
					// notifi IAS & enduser this device went to config mode (also sends localIP)
					#if CFG_STORAGE != ST_SPIFSS && CFG_ANNOUNCE == true
						if(_connected){
							this->iasLog("1");
						}
					#endif
				#ifdef ESP32	//<<--- this is to prevent nasty async tcp errors
				}	
				#endif
					// run config server
					configServer configServer(*this, config);
					configServer.run();
				#ifdef ESP8266
				}	
				#endif
				delay(100);
				
				// notifi IAS & enduser this device has left config mode (also sends localIP)
				#if CFG_STORAGE != ST_SPIFSS && CFG_ANNOUNCE == true
					if(_connected){
						this->iasLog("0");
					}
				#endif
				
				// Restart & return to Normal Operation
				this->espRestart('N');
			}
		#endif
	}

	
	// --------- if connection & automaticUpdate Update --------------------------
	if(_connected && _updateOnBoot == true){
		this->callHome();
	}

	_buttonEntry = millis() + MODE_BUTTON_VERY_LONG_PRESS;    // make sure the timedifference during startup is bigger than 10 sec. Otherwise it will go either in config mode or calls home
	_appState = AppStateNoPress;


	#if DEBUG_FREE_HEAP == true
		DEBUG_PRINTLN(" end of IAS::begin");
		DEBUG_PRINTF(" Free heap: %u\n", ESP.getFreeHeap());
	#endif
		
	#if DEBUG_LVL >= 1
		DEBUG_PRINT(F("\n\n\n\n\n"));
	#endif
}



/** print BoardInfo */
#if DEBUG_LVL >= 1
void IOTAppStory::printBoardInfo(){
	DEBUG_PRINTF_P(SER_BOOTTIMES_UPDATE, bootTimes, boardMode);
	DEBUG_PRINTLN(FPSTR(SER_DEV));
}
#endif



/** send msg to iasLog */
void IOTAppStory::iasLog(String msg) {
	
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);
	
	// notifi IAS & enduser about the localIP
	callServer		callServer(config, U_LOGGER);
	callServer.sm(&statusMessage);
	msg.replace(" ", "_");
	msg = "msg="+msg;
	
	#if DEBUG_LVL >= 3
		DEBUG_PRINT(SER_UPDATE_IASLOG);
		if(!callServer.get(OTA_LOG_FILE, msg)){
			DEBUG_PRINTLN(SER_FAILED_COLON);
			DEBUG_PRINTLN(" " + statusMessage);
		}
	#else
		callServer.get(OTA_LOG_FILE, msg);
	#endif
}



/** Connect to Wifi AP */
void IOTAppStory::WiFiSetupAndConnect() {
	
	#if DEBUG_LVL >= 1
		DEBUG_PRINTLN(SER_CONNECTING);
	#endif
	
	// setup wifi credentials
	WiFiConnector WiFiConn;
	WiFiConn.setup();
	
	#if DEBUG_LVL >= 1
		DEBUG_PRINT(" ");
	#endif
	
	// connect to access point
	if(!WiFiConn.connectToAP(".")){
		_connected = false;
		// FAILED
		// if conditions are met, set to config mode (C)
		if(_automaticConfig || boardMode == 'C'){
			
			if(boardMode == 'N'){
				boardMode = 'C';
				boardInfo boardInfo(bootTimes, boardMode);
				boardInfo.write();
			}
			
			#if DEBUG_LVL >= 1
				DEBUG_PRINT(SER_CONN_NONE_GO_CFG);
			#endif
			
		}else{
			
			#if DEBUG_LVL >= 1
				// this point is only reached if _automaticConfig = false
				DEBUG_PRINT(SER_CONN_NONE_CONTINU);
			#endif
		}

	}else{
		_connected = true;
		// SUCCES
		// Show connection details if debug level is set
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(SER_CONNECTED);
			DEBUG_PRINT(SER_DEV_IP);
			DEBUG_PRINTLN(WiFi.localIP());
		#endif
		#if DEBUG_LVL >= 2
			DEBUG_PRINT(SER_DEV_MAC);
			DEBUG_PRINTLN(WiFi.macAddress());
		#endif

		// Register host name in WiFi and mDNS
		#if WIFI_USE_MDNS == true
			
			// wifi_station_set_hostname(config.deviceName);
			// WiFi.hostname(hostNameWifi);
			
			// get config from EEPROM
			configStruct config;
			this->readConfig(config);

			if(MDNS.begin(config.deviceName)){

				#if DEBUG_LVL >= 1
					DEBUG_PRINT(SER_DEV_MDNS);
					DEBUG_PRINT(config.deviceName);
					DEBUG_PRINT(".local");
				#endif

				#if DEBUG_LVL >= 3
					DEBUG_PRINTLN(SER_DEV_MDNS_INFO);
				#endif
				#if DEBUG_LVL == 1 || DEBUG_LVL == 2
					DEBUG_PRINTLN(F(""));
				#endif

			}else{
				#if DEBUG_LVL >= 1
					DEBUG_PRINTLN(SER_DEV_MDNS_FAIL);
				#endif
			}
		#endif
	}


	#if DEBUG_LVL >= 1
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif
}



/**
	Dusconnect wifi
*/
void IOTAppStory::WiFiDisconnect(){
	WiFi.disconnect();
	_connected = false;
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(F(" WiFi disconnected!"));
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif
}



/**
	Set time via NTP, as required for x.509 validation
*/
void IOTAppStory::setClock(){
	#if defined  ESP8266
		int retries = WIFI_CONN_MAX_RETRIES;
	#elif defined ESP32
		int retries = (WIFI_CONN_MAX_RETRIES/2);
	#endif

	configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

	#if DEBUG_LVL >= 2
		DEBUG_PRINT(SER_SYNC_TIME_NTP);
	#endif
	
	time_t now = time(nullptr);
	while (now < 8 * 3600 * 2 && retries-- > 0 ){
		delay(500);
		#if DEBUG_LVL >= 2
			DEBUG_PRINT(F("."));
		#endif
		now = time(nullptr);
	}

	if(retries > 0){
		struct tm timeinfo;
		gmtime_r(&now, &timeinfo);
		_timeSet 		= true;
		_lastTimeSet 	= millis();
		
		#if DEBUG_LVL >= 3
			DEBUG_PRINT(F("\n Current time: "));
			DEBUG_PRINT(asctime(&timeinfo));
		#endif
	}else{
		_timeSet 		= false;
		_lastTimeSet 	= 0;
		
		#if DEBUG_LVL >= 2
			DEBUG_PRINTLN(SER_FAILED_EXCL);
		#endif
	}
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(F(""));
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif
}



/**
	call home and check for updates
*/
void IOTAppStory::callHome(bool spiffs /*= true*/) {
	
	// update from IOTappStory.com
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(SER_CALLING_HOME);
	#endif

	if (_firmwareUpdateCheckCallback){
		_firmwareUpdateCheckCallback();
	}

	{
		// try to update sketch from IOTAppStory
		iotUpdater();
	}

	// try to update spiffs from IOTAppStory
	#if OTA_UPD_CHECK_SPIFFS == true
		if(spiffs){
			{
				iotUpdater(U_SPIFFS);
			}
		}
	#endif
	
	#if OTA_UPD_CHECK_NEXTION == true
		{
			iotUpdater(U_NEXTION);
		}
	#endif

	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(SER_RET_FROM_IAS);
	#endif
	#if DEBUG_LVL >= 1
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif
	
	// update last time called home
	_lastCallHomeTime = millis();
}



/**
	IOT updater
*/
bool IOTAppStory::iotUpdater(int command) {
	
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);
	yield();
	
	bool result = false;
	{
		#if DEBUG_LVL >= 2
			DEBUG_PRINT(F("\n"));
		#endif
		#if DEBUG_LVL >= 1
			DEBUG_PRINT(SER_CHECK_FOR);
		#endif
		#if DEBUG_LVL >= 1
			if(command == U_FLASH){
				DEBUG_PRINT(SER_APP_SKETCH);
				
			}else if(command == U_SPIFFS){
				DEBUG_PRINT(SER_SPIFFS);
				
			}
			#if OTA_UPD_CHECK_NEXTION == true
				else if(command == U_NEXTION){
					DEBUG_PRINT(SER_NEXTION);
				}
			#endif
		#endif
		#if DEBUG_LVL >= 2
			DEBUG_PRINT(SER_UPDATES_FROM);
		#endif
		#if DEBUG_LVL == 1
			DEBUG_PRINT(SER_UPDATES);
		#endif
		#if DEBUG_LVL >= 2
			#if HTTPS == true
				DEBUG_PRINT(F("https://"));
			#else
				DEBUG_PRINT(F("http://"));
			#endif
			DEBUG_PRINT(OTA_HOST);
			DEBUG_PRINTLN(OTA_UPD_FILE);
		#endif
		#if DEBUG_LVL == 1
			DEBUG_PRINTLN("");
		#endif

		firmwareStruct	firmwareStruct;
		callServer		callServer(config, command);
		callServer.sm(&statusMessage);
		
		
		Stream &clientStream = callServer.getStream(&firmwareStruct);
		yield();
		
		if(!firmwareStruct.success){
			#if DEBUG_LVL >= 2
				DEBUG_PRINTLN(" " + statusMessage);
			#endif
			
			return false;
		}
		
		
		if (_firmwareUpdateDownloadCallback){
			_firmwareUpdateDownloadCallback();
		}

		
		
		if(command == U_FLASH || command == U_SPIFFS){
			// sketch / spiffs
			result = espInstaller(clientStream, &firmwareStruct, UpdateESP, command);
		}
		#if OTA_UPD_CHECK_NEXTION == true
			if(command == U_NEXTION){
				// nextion display
				espInstaller(clientStream, &firmwareStruct, UpdateNextion, command);
			}
		#endif
	}
	
	if(result && (command == U_FLASH || command == U_NEXTION)){

		// succesfull update
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(SER_REBOOT_NEC);
		#endif
		
		// reboot to start the new updated firmware
		ESP.restart();
	}
	
	return true;
}



/**
	espInstaller
*/
bool IOTAppStory::espInstaller(Stream &streamPtr, firmwareStruct *firmwareStruct, UpdateClassVirt& devObj, int command) {
	devObj.sm(&statusMessage);
	bool result = devObj.prepareUpdate((*firmwareStruct).xlength, (*firmwareStruct).xmd5, command);

	if(!result){
		#if DEBUG_LVL >= 2
			DEBUG_PRINTLN(statusMessage);
		#endif
	}else{
		
		#if DEBUG_LVL >= 2
			DEBUG_PRINT(SER_INSTALLING);
		#endif
		
		yield();
		
		// Write the buffered bytes to the esp. If this fails, return false.
		{
			// create buffer for read
			uint8_t buff[OTA_BUFFER] = { 0 };
			
			// to do counter
			int updTodo = (*firmwareStruct).xlength;
			
			// Upload the received byte Stream to the device
			while(updTodo > 0 || updTodo == -1){
				
				// get available data size
				size_t size = streamPtr.available();
				
				if(size){
					// read up to 2048 byte into the buffer
					size_t c = streamPtr.readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
					
					// Write the buffered bytes to the esp. If this fails, return false.
					result = devObj.update(buff, c);
					
					if(updTodo > 0) {
						updTodo -= c;
					}

					if (_firmwareUpdateProgressCallback){
						_firmwareUpdateProgressCallback((*firmwareStruct).xlength - updTodo, (*firmwareStruct).xlength);
						//DEBUG_PRINTF(" Free heap: %u\n", ESP.getFreeHeap());
					}
				}
				delay(1);
			}
		}

		
		if(!result){
			#if DEBUG_LVL >= 2
				DEBUG_PRINT(SER_UPDATEDERROR);
				DEBUG_PRINTLN(statusMessage);
			#endif
		}else{

			// end: wait(delay) for the nextion to finish the update process, send nextion reset command and end the serial connection to the nextion
			result = devObj.end();

			if(result){
				// on succesfull firmware installation
				#if DEBUG_LVL >= 2
					DEBUG_PRINT(SER_UPDATEDTO);
					DEBUG_PRINTLN((*firmwareStruct).xname+" v"+ (*firmwareStruct).xver);
				#endif
				
				// get config from EEPROM
				configStruct config;
				this->readConfig(config);

				if(command == U_FLASH){
					// write received appName & appVersion to config
					(*firmwareStruct).xname.toCharArray(config.appName, 33);
					(*firmwareStruct).xver.toCharArray(config.appVersion, 12);
				}
				
				#if OTA_UPD_CHECK_NEXTION == true
					if(command == U_NEXTION){
						// update nextion md5
						(*firmwareStruct).xmd5.toCharArray(config.next_md5, 33);
					}
				#endif
				
				// write config to EEPROM
				this->writeConfig(config);
				

				if (_firmwareUpdateSuccessCallback){
					_firmwareUpdateSuccessCallback();
				}

			}else{
				// update failed
				#if DEBUG_LVL >= 2
					DEBUG_PRINTLN(" " + statusMessage);
				#endif
				if(_firmwareUpdateErrorCallback){
					_firmwareUpdateErrorCallback();
				}
			}
		}
		
		
	}
	return result;
}



/** 
	Add fields to the fieldStruct
*/
void IOTAppStory::addField(char* &defaultVal, const char *fieldLabel, const int length, const char type){
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);
	if(strcmp(config.compDate, _compDate) == 0){
		if(_nrXF >= MAXNUMEXTRAFIELDS){
			
			// if MAXNUMEXTRAFIELDS is reached return an error
			#if DEBUG_LVL >= 1
				DEBUG_PRINTLN(SER_PROC_ERROR);
			#endif
			
		}else{

			#if DEBUG_LVL >= 1
				// if this is the first field being processed display header
				if(_nrXF == 0){
					
					#if DEBUG_LVL >= 2
						DEBUG_PRINTLN(FPSTR(SER_DEV));
					#endif
					
					DEBUG_PRINT(SER_PROC_FIELDS);
					#if DEBUG_LVL == 1
						DEBUG_PRINTLN(F(""));
					#endif
					
					#if DEBUG_LVL >= 2
						DEBUG_PRINTLN(SER_PROC_TBL_HDR);
					#endif
				}
			#endif
			
			// init fieldStruct
			addFieldStruct fieldStruct;
			
			// calculate EEPROM addresses
			const int eepStartAddress = FIELD_EEP_START_ADDR + (_nrXF * sizeof(fieldStruct));
			const int eepEndAddress = eepStartAddress + sizeof(fieldStruct);
			const int magicBytesBegin = eepEndAddress - 3;
			int eepFieldStart;
			
			if(_nrXF == 0){
				eepFieldStart = FIELD_EEP_START_ADDR + (MAXNUMEXTRAFIELDS * sizeof(fieldStruct)) + _nrXFlastAdd;
			}else{
				eepFieldStart = _nrXFlastAdd;
			}
			_nrXFlastAdd = eepFieldStart + length + 1;
			

			#if DEBUG_LVL >= 2
				DEBUG_PRINTF_P(PSTR(" %02d | %-30s | %03d | %04d to %04d | %-30s | "), _nrXF+1, fieldLabel, length, eepFieldStart, _nrXFlastAdd, defaultVal);
			#endif
			
			// EEPROM begin
			EEPROM.begin(EEPROM_SIZE);
			
			// check for MAGICEEP to confirm the this fieldStruct is stored in EEPROM
			if(EEPROM.read(magicBytesBegin) != MAGICEEP[0]){

				#if DEBUG_LVL >= 2
					DEBUG_PRINTF_P(SER_PROC_TBL_WRITE, defaultVal);
				#endif
				
				// add values to the fieldstruct
				fieldStruct.fieldLabel 	= fieldLabel;
				fieldStruct.length 		= length;
				fieldStruct.type 		= type;
				
				// put the fieldStruct to EEPROM
				EEPROM.put(eepStartAddress, fieldStruct);
				
				// temp val buffer
				char eepVal[length+1];
				strcpy(eepVal, defaultVal);
				
				// put the field value to EEPROM
				unsigned int ee = 0;
				for(unsigned int e=eepFieldStart; e < _nrXFlastAdd; e++){
					EEPROM.write(e, eepVal[ee]);
					ee++;
				}
				
			}else{
				
				// get the fieldStruct from EEPROM
				EEPROM.get(eepStartAddress, fieldStruct);
				
				// temp val buffer
				char eepVal[length+1];
				
				// read field value from EEPROM and store it in eepVal buffer
				unsigned int ee = 0;
				for(unsigned int e=eepFieldStart; e < _nrXFlastAdd; e++){
					eepVal[ee] = EEPROM.read(e);
					ee++;
				}
				

				// compair EEPROM value with the defaultVal
				if(strcmp(eepVal, defaultVal) == 0){
					
					// EEPROM value is the same als the default value
					#if DEBUG_LVL >= 2
						DEBUG_PRINTF_P(SER_PROC_TBL_DEF, defaultVal);
					#endif
					
				}else{
					
					// EEPROM value is NOT the same als the default value
					#if DEBUG_LVL >= 2
						DEBUG_PRINTF_P(SER_PROC_TBL_OVRW, eepVal);
					#endif
					
					// workaround to prevent jiberish chars | move addField char* to char[] with v3!
					defaultVal = new char[length+1];				
					
					// update the default value with the value from EEPROM
					strcpy(defaultVal, eepVal);
				}
				
				bool putfieldStruct = false;

				// compair EEPROM fieldLabel with the current fieldLabel
				if(fieldStruct.fieldLabel != fieldLabel){
					
					// EEPROM value is NOT the same als the default value
					#if DEBUG_LVL >= 2
						DEBUG_PRINTLN("Overwritting label");
					#endif				
					
					// update the default value with the value from EEPROM
					fieldStruct.fieldLabel 	= fieldLabel;
					putfieldStruct = true;
				}
				
				// compair EEPROM fieldLabel with the current fieldLabel
				if(fieldStruct.length != length){
					
					// EEPROM value is NOT the same als the default value
					#if DEBUG_LVL >= 2
						DEBUG_PRINTLN("Overwritting length");
					#endif
					
					// update the default value with the value from EEPROM
					fieldStruct.length 		= length;
					putfieldStruct = true;
				}
				
				// compair EEPROM fieldLabel with the current fieldLabel
				if(fieldStruct.type != type){
					
					// EEPROM value is NOT the same als the default value
					#if DEBUG_LVL >= 2
						DEBUG_PRINTLN("Overwritting type");
					#endif
					
					// update the default value with the value from EEPROM
					fieldStruct.type 		= type;
					putfieldStruct = true;
				}
				
				if(putfieldStruct){
					// put the fieldStruct to EEPROM
					EEPROM.put(eepStartAddress, fieldStruct);
				}
			}

			// EEPROM end
			EEPROM.end();
			
			#if DEBUG_LVL >= 1
				DEBUG_PRINTLN("");
			#endif
			
			delay(200);
			
			// increase added xtra field count
			_nrXF++;
		}
	}
}



/** 
	convert dpins to int
*/
int IOTAppStory::dPinConv(String orgVal){
	#if defined ESP8266_OAK

		// https://github.com/esp8266/Arduino/blob/master/variants/oak/pins_arduino.h
		// DEBUG_PRINTLN("- Digistump OAK -");
		if      (orgVal == "P0"  || orgVal == "2")    return P0;
		else if (orgVal == "P1"  || orgVal == "5")    return P1;
		else if (orgVal == "P2"  || orgVal == "0")    return P2;
		else if (orgVal == "P3"  || orgVal == "3")    return P3;
		else if (orgVal == "P4"  || orgVal == "1")    return P4;
		else if (orgVal == "P5"  || orgVal == "4")    return P5;
		else if (orgVal == "P6"  || orgVal == "15")   return P6;
		else if (orgVal == "P7"  || orgVal == "13")   return P7;
		else if (orgVal == "P8"  || orgVal == "12")   return P8;
		else if (orgVal == "P9"  || orgVal == "14")   return P9;
		else if (orgVal == "P10" || orgVal == "16")   return P10;
		else if (orgVal == "P11" || orgVal == "17")   return P11;
		else                                          return P0;

	#elif defined ESP8266_WEMOS_D1MINI || defined ESP8266_WEMOS_D1MINILITE || defined ESP8266_WEMOS_D1MINIPRO

		// https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h
		// DEBUG_PRINTLN("- build-variant d1_mini -");
		if      (orgVal == "D0"  || orgVal == "16")   return D0;
		else if (orgVal == "D1"  || orgVal == "5")    return D1;
		else if (orgVal == "D2"  || orgVal == "4")    return D2;
		else if (orgVal == "D3"  || orgVal == "0")    return D3;
		else if (orgVal == "D4"  || orgVal == "2")    return D4;
		else if (orgVal == "D5"  || orgVal == "14")   return D5;
		else if (orgVal == "D6"  || orgVal == "12")   return D6;
		else if (orgVal == "D7"  || orgVal == "13")   return D7;
		else if (orgVal == "D8"  || orgVal == "15")   return D8;
		else if (orgVal == "RX"  || orgVal == "3")    return RX;
		else if (orgVal == "TX" || orgVal == "1")     return TX;
		else                                          return D0;

	#elif defined ESP8266_NODEMCU || defined WIFINFO

		// https://github.com/esp8266/Arduino/blob/master/variants/wifinfo/pins_arduino.h
		// https://github.com/esp8266/Arduino/blob/master/variants/nodemcu/pins_arduino.h
		// DEBUG_PRINTLN("- build-variant nodemcu and wifinfo -");
		if      (orgVal == "D0"  || orgVal == "16")   return D0;
		else if (orgVal == "D1"  || orgVal == "5")    return D1;
		else if (orgVal == "D2"  || orgVal == "4")    return D2;
		else if (orgVal == "D3"  || orgVal == "0")    return D3;
		else if (orgVal == "D4"  || orgVal == "2")    return D4;
		else if (orgVal == "D5"  || orgVal == "14")   return D5;
		else if (orgVal == "D6"  || orgVal == "12")   return D6;
		else if (orgVal == "D7"  || orgVal == "13")   return D7;
		else if (orgVal == "D8"  || orgVal == "15")   return D8;
		else if (orgVal == "D9"  || orgVal == "3")    return D9;
		else if (orgVal == "D10" || orgVal == "1")    return D10;
		else                                          return D0;

	#else

		// DEBUG_PRINTLN("- Generic ESP's -");
		
		// There are NO constants for the generic eps's!
		// But people makes mistakes when entering pin nr's in config
		// And if you originally developed your code for "Special ESP's"
		// this part makes makes it compatible when compiling for "Generic ESP's"
		
		if      (orgVal == "D0"  || orgVal == "16")   return 16;
		else if (orgVal == "D1"  || orgVal == "5")    return 5;
		else if (orgVal == "D2"  || orgVal == "4")    return 4;
		else if (orgVal == "D3"  || orgVal == "0")    return 0;
		else if (orgVal == "D4"  || orgVal == "2")    return 2;
		else if (orgVal == "D5"  || orgVal == "14")   return 14;
		else if (orgVal == "D6"  || orgVal == "12")   return 12;
		else if (orgVal == "D7"  || orgVal == "13")   return 13;
		else if (orgVal == "D8"  || orgVal == "15")   return 15;
		else if (orgVal == "D9"  || orgVal == "3")    return 3;
		else if (orgVal == "D10" || orgVal == "1")    return 1;
		else                                          return 16;

	#endif
}



/** 
	Set mode and reboot
*/
void IOTAppStory::espRestart(char mmode) {
	//while (isModeButtonPressed()) yield();    // wait till GPIOo released
	delay(500);
	
	boardMode = mmode;
	boardInfo boardInfo(bootTimes, boardMode);
	boardInfo.write();

	ESP.restart();
}



/** 
	Erase EEPROM from till
*/
void IOTAppStory::eraseEEPROM(int eepFrom, int eepTo) {
	#if DEBUG_LVL >= 2 || DEBUG_EEPROM_CONFIG == true
		DEBUG_PRINTF_P(SER_ERASE_FLASH, eepFrom, eepTo);
	#endif
	
	EEPROM.begin(EEPROM_SIZE);
	for (int t = eepFrom; t < eepTo; t++) EEPROM.write(t, 0);
	EEPROM.end();
}



/** 
	Erase EEPROM (F)ull or (P)artial
*/
void IOTAppStory::eraseEEPROM(const char ea) {

	// erase eeprom after config (delete extra field data etc.)
	if(ea == 'F'){
		
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(SER_ERASE_FULL);
		#endif
		
		// Wipe out WiFi credentials.
		WiFi.disconnect();
		delay(200);
		
		// erase full eeprom
		this->eraseEEPROM(0, EEPROM_SIZE);

	}else if(ea == 'P'){
		
		#if DEBUG_LVL == 1
			DEBUG_PRINTLN(SER_ERASE_PART);
		#endif
		
		#if DEBUG_LVL >= 2
			DEBUG_PRINTLN(SER_ERASE_PART_EXT);
		#endif
		
		// erase eeprom but leave the config settings
		this->eraseEEPROM(FIELD_EEP_START_ADDR, EEPROM_SIZE);
	}
}



/** 
	Write the config struct to EEPROM
*/
void IOTAppStory::writeConfig(configStruct &config) {
	#if DEBUG_EEPROM_CONFIG == true
		DEBUG_PRINTLN("DEBUG_EEPROM\t| running writeConfig(...)");
	#endif
	
	EEPROM.begin(EEPROM_SIZE);
	EEPROM.put(CFG_EEP_START_ADDR, config);
	EEPROM.end();
}



/** 
	Read the config struct from EEPROM
*/
void IOTAppStory::readConfig(configStruct &config) {
	#if DEBUG_EEPROM_CONFIG == true
		DEBUG_PRINTLN("DEBUG_EEPROM\t| running readConfig()");
	#endif
	
	EEPROM.begin(EEPROM_SIZE);
	const int magicBytesBegin = CFG_EEP_START_ADDR + sizeof(config) - 4;
	
	#if DEBUG_EEPROM_CONFIG == true
		DEBUG_PRINT("DEBUG_EEPROM\t| config start: ");
		DEBUG_PRINTLN(CFG_EEP_START_ADDR);
		DEBUG_PRINT("DEBUG_EEPROM\t| config end: ");
		DEBUG_PRINTLN(CFG_EEP_START_ADDR + sizeof(config));
		DEBUG_PRINT("DEBUG_EEPROM\t| Searching for config MAGICBYTES at: ");
		DEBUG_PRINTLN(magicBytesBegin);
	#endif
	if(EEPROM.read(magicBytesBegin) == MAGICBYTES[0] && EEPROM.read(magicBytesBegin + 1) == MAGICBYTES[1] && EEPROM.read(magicBytesBegin + 2) == MAGICBYTES[2]){
		EEPROM.get(CFG_EEP_START_ADDR, config);
		#if DEBUG_EEPROM_CONFIG == true
			DEBUG_PRINTLN("DEBUG_EEPROM\t| Found! Succesfully read config from EEPROM");
		#endif
	}else{
		EEPROM.put(CFG_EEP_START_ADDR, config);
		#if DEBUG_EEPROM_CONFIG == true
			DEBUG_PRINTLN("DEBUG_EEPROM\t| Failed! Writing config to EEPROM and return new config");
		#endif
	}
	EEPROM.end();
}



void IOTAppStory::loop() {
	
	// wifi connector
	#if WIFI_MULTI_FORCE_RECONN_ANY == true
	if(WiFi.status() == WL_NO_SSID_AVAIL){
		_connected = false;
		WiFi.disconnect(false);
		delay(10);
		
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(SER_CONN_LOST_RECONN);
		#endif
		
		WiFiConnector WiFiConn;
		WiFiConn.WiFiConnectToAP(".");

		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(F(""));
			DEBUG_PRINTLN(FPSTR(SER_DEV));
		#endif
	}
	#endif
	
	// Synchronize the internal clock useing SNTP every SNTP_INT_CLOCK_UPD_INTERVAL
	#if SNTP_INT_CLOCK_UPD == true
		if(_connected && millis() - _lastTimeSet > SNTP_INT_CLOCK_UPD_INTERVAL){
			this->setClock();
		}
	#endif
	
	// Call home and check for updates every _callHomeInterval
	if (_connected && _callHomeInterval > 0 && millis() - _lastCallHomeTime > _callHomeInterval) {
		this->callHome();
	}
	
	// handle button presses: short, long, xlong
	this->buttonLoop();
	
	#if DEBUG_FREE_HEAP == true
		DEBUG_PRINTLN(" end of IAS::loop");
		DEBUG_PRINTF(" Free heap: %u\n", ESP.getFreeHeap());
	#endif
}



ModeButtonState IOTAppStory::buttonLoop() {
	return getModeButtonState();
}



bool IOTAppStory::isModeButtonPressed() {
	return digitalRead(_modeButton) == LOW; // LOW means flash button IS pressed
}



ModeButtonState IOTAppStory::getModeButtonState() {

	while(true)
	{
		unsigned long buttonTime = millis() - _buttonEntry;

		switch(_appState) {
		case AppStateNoPress:
			if (isModeButtonPressed()) {
				_buttonEntry = millis();
				_appState = AppStateWaitPress;
				continue;
			}
			return ModeButtonNoPress;

		case AppStateWaitPress:
			if (buttonTime > MODE_BUTTON_SHORT_PRESS) {
				_appState = AppStateShortPress;
				if (_shortPressCallback)
					_shortPressCallback();
				continue;
			}
			if (!isModeButtonPressed()) {
				_appState = AppStateNoPress;
			}
			return ModeButtonNoPress;

		case AppStateShortPress:
			if (buttonTime > MODE_BUTTON_LONG_PRESS) {
				_appState = AppStateLongPress;
				if (_longPressCallback)
					_longPressCallback();
				continue;
			}
			if (!isModeButtonPressed()) {
				_appState = AppStateFirmwareUpdate;
				continue;
			}
			return ModeButtonShortPress;

		case AppStateLongPress:
			if (buttonTime > MODE_BUTTON_VERY_LONG_PRESS) {
				_appState = AppStateVeryLongPress;
				if (_veryLongPressCallback)
					_veryLongPressCallback();
				continue;
			}
#if CFG_INCLUDE == true
			if (!isModeButtonPressed()) {
				_appState = AppStateConfigMode;
				continue;
			}
#endif
			return ModeButtonLongPress;
	
		case AppStateVeryLongPress:
			if (!isModeButtonPressed()) {
				_appState = AppStateNoPress;
				if (_noPressCallback)
					_noPressCallback();
				continue;
			}
			return ModeButtonVeryLongPress;
		
		case AppStateFirmwareUpdate:
			_appState = AppStateNoPress;
			this->callHome();
			continue;
#if CFG_INCLUDE == true	
		case AppStateConfigMode:
			_appState = AppStateNoPress;
			#if DEBUG_LVL >= 1
				DEBUG_PRINTLN(SER_CONFIG_ENTER);
			#endif
			espRestart('C');
			continue;
#endif
		}
	}
	return ModeButtonNoPress; // will never reach here (used just to avoid compiler warnings)
}



/**
	callBacks
*/
void IOTAppStory::onFirstBoot(THandlerFunction value) {
	_firstBootCallback = value;
}

void IOTAppStory::onModeButtonNoPress(THandlerFunction value) {
	_noPressCallback = value;
}
void IOTAppStory::onModeButtonShortPress(THandlerFunction value) {
	_shortPressCallback = value;
}
void IOTAppStory::onModeButtonLongPress(THandlerFunction value) {
	_longPressCallback = value;
}
void IOTAppStory::onModeButtonVeryLongPress(THandlerFunction value) {
	_veryLongPressCallback = value;
}

void IOTAppStory::onFirmwareUpdateCheck(THandlerFunction value) {
	_firmwareUpdateCheckCallback = value;
}
void IOTAppStory::onFirmwareUpdateDownload(THandlerFunction value) {
	_firmwareUpdateDownloadCallback = value;
}
void IOTAppStory::onFirmwareUpdateProgress(THandlerFunctionArg value) {
	_firmwareUpdateProgressCallback = value;
}
void IOTAppStory::onFirmwareUpdateError(THandlerFunction value) {
	_firmwareUpdateErrorCallback = value;
}
void IOTAppStory::onFirmwareUpdateSuccess(THandlerFunction value) {
	_firmwareUpdateSuccessCallback = value;
}

void IOTAppStory::onConfigMode(THandlerFunction value) {
	_configModeCallback = value;
}



/** Handle root */
String IOTAppStory::servHdlRoot() {

	String retHtml;
	retHtml += FPSTR(HTTP_TEMP_START);

	if(_connected){

		retHtml.replace("{h}", FPSTR(HTTP_STA_JS));

	}else{

		retHtml.replace("{h}", FPSTR(HTTP_AP_CSS));
		retHtml += FPSTR(HTTP_WIFI_FORM);
		retHtml.replace("{r}", strWifiScan());
		retHtml += FPSTR(HTTP_AP_JS);
	}

	retHtml += FPSTR(HTTP_TEMP_END);
	return retHtml;
}



/** Handle device information */
String IOTAppStory::servHdlDevInfo(){
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SERV_DEV_INFO);
	#endif
	
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);

	String retHtml;
	retHtml += FPSTR(HTTP_DEV_INFO);
	retHtml.replace(F("{cid}"), String(ESP_GETCHIPID));
	retHtml.replace(F("{fid}"), String(ESP_GETFLASHCHIPID));

	#if defined  ESP8266 && HTTPS_8266_TYPE == FNGPRINT
		retHtml.replace(F("{f}"), config.sha1);
	#endif

	retHtml.replace(F("{fss}"), String(ESP.getFreeSketchSpace()));
	retHtml.replace(F("{ss}"), String(ESP.getSketchSize()));
		
	retHtml.replace(F("{fs}"), String(ESP.getFlashChipSize()));
	retHtml.replace(F("{ab}"), ARDUINO_BOARD);
	retHtml.replace(F("{mc}"), WiFi.macAddress());
	retHtml.replace(F("{xf}"), String(_nrXF));

	if(String(config.actCode) == "000000" || String(config.actCode) == ""){
		retHtml.replace(F("{ac}"), "0");	
	}else{
		retHtml.replace(F("{ac}"), "1");	
	}
	
	return retHtml;
}



/** Handle wifi scan */
String IOTAppStory::strWifiScan(){
	
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SERV_WIFI_SCAN_RES);
	#endif
	
    // WiFi.scanNetworks will return the number of networks found	
	String retHtml;
	int n = WiFi.scanComplete();
	if(n == -2){
		
		WiFi.scanNetworks(true);
		
	}else if(n){
		/**
			All credits for the "sort networks" & "RSSI SORT" code below goes to tzapu!
		*/
		
		// sort networks
		int indices[n];
		for (int i = 0; i < n; i++) {
			indices[i] = i;
		}

		// RSSI SORT
		for (int i = 0; i < n; i++) {
			for (int j = i + 1; j < n; j++) {
				if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
					std::swap(indices[i], indices[j]);
				}
			}
		}
		/**
			All credits for the "sort networks" & "RSSI SORT" code above goes to tzapu!
		*/
		
		for(int i = 0; i < n; i++){
			// return html results from the wifi scan
			retHtml += FPSTR(HTTP_WIFI_SCAN);
			retHtml.replace(F("{s}"), WiFi.SSID(indices[i]));
			retHtml.replace(F("{q}"), String(WiFi.RSSI(indices[i])));
			retHtml.replace(F("{e}"), String(WiFi.encryptionType(indices[i])));             
			delay(10);
		}
		
		WiFi.scanDelete();
		if(WiFi.scanComplete() == -2){
			WiFi.scanNetworks(true);
		}
	}
	return retHtml;
}



/** Handle wifi scan */
String IOTAppStory::strWifiCred(){
	
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SERV_WIFI_CRED);
	#endif
	
	WiFiConnector WiFiConn;
	
	String retHtml = F("[");
	
	for(int i = 1; i <= WIFI_MULTI_MAX; i++){
		if(i > 1){
			retHtml += F(",");
		}
		
		// create json string {\"s\":\"ssid1\"}
		retHtml += F("{\"s\":\"");
		
		#if WIFI_DHCP_ONLY == true
			retHtml += WiFiConn.getSSIDfromEEPROM(i);
		#else
			WiFiCredStruct config;
			WiFiConn.getAPfromEEPROM(config, i);
			
			retHtml += config.ssid;
			retHtml += F("\",\"si\":\"");
			retHtml += config.ip.toString();
			retHtml += F("\",\"ss\":\"");
			retHtml += config.subnet.toString();
			retHtml += F("\",\"sg\":\"");
			retHtml += config.gateway.toString();
			retHtml += F("\",\"sd\":\"");
			retHtml += config.dnsserv.toString();
		#endif
		
		retHtml += F("\"}");
	}
	
	retHtml += F("]");
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(retHtml);
	#endif
	return retHtml;
}



/** Handle save wifi credentials */
String IOTAppStory::servHdlWifiSave(const char* newSSID, const char* newPass, const int apNr) {

	WiFiConnector WiFiConn;
	if(apNr==0){
		WiFiConn.addAndShiftAPinEEPROM(newSSID, newPass);
	}else{
		WiFiConn.addAPtoEEPROM(newSSID, newPass, apNr);
	}
	
	return F("1");		// ok
}



/** Handle save wifi credentials */
String IOTAppStory::servHdlWifiSave(const char* newSSID, const char* newPass, String ip, String subnet, String gateway, String dnsserv) {
	
	WiFiConnector WiFiConn;
	if((ip+subnet+gateway+dnsserv) == ""){
		WiFiConn.addAPtoEEPROM(newSSID, newPass, 1);
	}else{
		WiFiConn.addAPtoEEPROM(newSSID, newPass, ip, subnet, gateway, dnsserv);
	}
	
	return F("1");		// ok
}



/** Handle app / firmware information */
String IOTAppStory::servHdlAppInfo(){
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SERV_APP_SETTINGS);
	#endif
	

	// EEPROM begin
	EEPROM.begin(EEPROM_SIZE);
	
	_nrXFlastAdd = 0;

	String retHtml = F("[");
	for (unsigned int i = 0; i < _nrXF; ++i) {

		// return html results from the wifi scan
		if(i > 0){
			retHtml += F(",");
		}

		// init fieldStruct
		addFieldStruct fieldStruct;
		
		// calculate EEPROM addresses
		const int eepStartAddress = FIELD_EEP_START_ADDR + (i * sizeof(addFieldStruct));
		int eepFieldStart;
		
		// get the fieldStruct from EEPROM
		EEPROM.get(eepStartAddress, fieldStruct);
		
		
		if(i == 0){
			eepFieldStart = FIELD_EEP_START_ADDR + (MAXNUMEXTRAFIELDS * sizeof(addFieldStruct)) + _nrXFlastAdd;
		}else{
			eepFieldStart = _nrXFlastAdd;
		}
		_nrXFlastAdd = eepFieldStart + fieldStruct.length + 1;
		
		// temp buffer
		char eepVal[fieldStruct.length + 1];

		// read field value from EEPROM and store it in eepVal buffer
		unsigned int ee = 0;
		for(unsigned int e=eepFieldStart; e < _nrXFlastAdd; e++){
			eepVal[ee] = EEPROM.read(e);
			ee++;
		}
		
		
		// add slashed to values where necessary to prevent the json repsons from being broken
		String value = eepVal;
		value.replace("\\", "\\\\");
		value.replace("\"", "\\\"");
		value.replace("\n", "\\n");
		value.replace("\r", "\\r");
		value.replace("\t", "\\t");
		value.replace("\b", "\\b");
		value.replace("\f", "\\f");

		// get PROGMEM json string and replace {*} with values
		retHtml += FPSTR(HTTP_APP_INFO);
		retHtml.replace(F("{l}"), String(fieldStruct.fieldLabel));
		retHtml.replace(F("{v}"), value);
		retHtml.replace(F("{n}"), String(i));
		retHtml.replace(F("{m}"), String(fieldStruct.length));
		retHtml.replace(F("{t}"), String(fieldStruct.type));
		delay(10);
	}
	retHtml += F("]");

	// EEPROM end
	EEPROM.end();
	delay(500);
	
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(retHtml);
	#endif
	
	return retHtml;
}



/** 
	Save new fingerprint
	Only used if ESP8266 && HTTPS_8266_TYPE == FNGPRINT
*/
#if defined  ESP8266 && HTTPS_8266_TYPE	== FNGPRINT
String IOTAppStory::servHdlFngPrintSave(String fngprint){
	
	// get config from EEPROM
	configStruct config;
	this->readConfig(config);

	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SAVE_FINGERPRINT);
		DEBUG_PRINT("Received fingerprint: ");
		DEBUG_PRINTLN(fngprint);
		DEBUG_PRINT("Current  fingerprint: ");
		DEBUG_PRINTLN(config.sha1);
	#endif
	
	fngprint.toCharArray(config.sha1, 60);

	// write config to EEPROM
	this->writeConfig(config);

	return F("1");
}
#endif



#if defined  ESP32
/** Get all root certificates */
String IOTAppStory::strCertScan(String path){
	
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SERV_CERT_SCAN_RES);
	#endif
	
	// open SPIFFS certificate directory
    File root = SPIFFS.open("/cert");
    if(!root || !root.isDirectory()){
		#if DEBUG_LVL >= 3
			DEBUG_PRINTLN(" Failed to open directory");
		#endif
		
        return "0";
    }
	
	// delete requested file
	if(path != ""){
		if(!SPIFFS.remove(path)){
			#if DEBUG_LVL >= 3
				DEBUG_PRINTLN(" Failed to delete file!");
			#endif
		}
	}
	
	// return all the files found in this directory and return them as a json string
    File file = root.openNextFile();
	String retHtml = "[";
	bool pastOne = false;
	
    while(file){
        if(!file.isDirectory()){
			if(pastOne == true){
				retHtml += F(",");
			}
			retHtml += "{";
			retHtml += "\"n\":\"" + String(file.name()) + "\"";
			retHtml += ",\"s\":" + String(file.size());
			retHtml += "}";
			pastOne = true;
        }
        file = root.openNextFile();
    }
	retHtml += "]";

	// return json string
	return retHtml;
}
#else
String IOTAppStory::strCertScan(String path){
	
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SERV_CERT_SCAN_RES);
	#endif
	
	//Initialize File System
	if(!ESP_SPIFFSBEGIN){
		#if DEBUG_LVL >= 3
			DEBUG_PRINT(F(" SPIFFS Mount Failed"));
		#endif
	}
	
	/* <-- always fails
    // check if SPIFFS certificate directory exists
    if(!SPIFFS.exists("/cert")){  // || !root.isDirectory()
		#if DEBUG_LVL >= 2
			DEBUG_PRINTLN(F(" Failed to open directory"));
		#endif
		
        //return "0";
    }*/
	
	// open SPIFFS certificate directory
	Dir dir = SPIFFS.openDir("/cert/");
	
	// delete requested file
	if(path != ""){
		if(!SPIFFS.remove(path)){
			#if DEBUG_LVL >= 3
				DEBUG_PRINTLN(F(" Failed to delete file!"));
			#endif
		}
	}
	
	// return all the files found in this directory and return them as a json string
	String retHtml = "[";
    while(dir.next()){
		
		if(dir.fileSize()) {
			File file = dir.openFile("r");
			
			if(retHtml != "["){
				retHtml += F(",");
			}
			retHtml += "{";
			retHtml += "\"n\":\"" + String(file.name()) + "\"";
			retHtml += ",\"s\":" + String(file.size());
			retHtml += "}";
		}
    }
	retHtml += "]";

	// return json string
	return retHtml;
}
#endif


/** Save App Settings */
String IOTAppStory::servHdlAppSave(AsyncWebServerRequest *request) {
	#if DEBUG_LVL >= 3
		DEBUG_PRINTLN(SER_SAVE_APP_SETTINGS);
	#endif
	
	if(_nrXF){

		// EEPROM begin
		EEPROM.begin(EEPROM_SIZE);	
		_nrXFlastAdd = 0;
		
		// init fieldStruct
		addFieldStruct fieldStruct;		

		for(unsigned int i = 0; i < _nrXF; i++){
			if(request->hasParam(String(i), true)){	

				// calculate EEPROM addresses
				const int eepStartAddress = FIELD_EEP_START_ADDR + (i * sizeof(fieldStruct));
				int eepFieldStart;
				
				// get the fieldStruct from EEPROM
				EEPROM.get(eepStartAddress, fieldStruct);
				
				
				
				if(i == 0){
					eepFieldStart = FIELD_EEP_START_ADDR + (MAXNUMEXTRAFIELDS * sizeof(fieldStruct)) + _nrXFlastAdd;
				}else{
					eepFieldStart = _nrXFlastAdd;
				}
				_nrXFlastAdd = eepFieldStart + fieldStruct.length + 1;
				
				char eepVal[fieldStruct.length + 1];
				
				// read field value from EEPROM and store it in eepVal buffer
				unsigned int ee = 0;
				for(unsigned int e=eepFieldStart; e < _nrXFlastAdd; e++){
					eepVal[ee] = EEPROM.read(e);
					ee++;
				}
				
				
				
				if(strcmp(eepVal, request->getParam(String(i), true)->value().c_str()) != 0){
					
					char saveEepVal[fieldStruct.length+1];
					
					// overwrite current value with the saved value
					request->getParam(String(i), true)->value().toCharArray(saveEepVal, fieldStruct.length+1);

					// write the field value to EEPROM
					unsigned int ee = 0;
					for(unsigned int e=eepFieldStart; e < _nrXFlastAdd; e++){
						EEPROM.write(e, saveEepVal[ee]);
						ee++;
					}

				#if DEBUG_LVL >= 3
					DEBUG_PRINT("\nOverwrite with new value: ");
					DEBUG_PRINTLN(saveEepVal);
					DEBUG_PRINT("EEPROM from: ");
					DEBUG_PRINT(eepFieldStart);
					DEBUG_PRINT(" to ");
					DEBUG_PRINTLN(_nrXFlastAdd);
				}else{
					DEBUG_PRINTLN("No need to overwrite current value");
				#endif
				}
			}
		}
		
		// EEPROM end
		EEPROM.end();
		delay(200);

		return F("1");
	}
	
	return F("0");
}



/** Save activation code */
String IOTAppStory::servHdlactcodeSave(String actcode) {
	#if DEBUG_LVL >= 3
		DEBUG_PRINT(SER_REC_ACT_CODE);
		DEBUG_PRINTLN(actcode);
	#endif
	
	if(actcode != ""){
		// get config from EEPROM
		configStruct config;
		this->readConfig(config);

		actcode.toCharArray(config.actCode, 7);

		// write config to EEPROM
		this->writeConfig(config);

		return F("1");
	}

	return F("0");
}
