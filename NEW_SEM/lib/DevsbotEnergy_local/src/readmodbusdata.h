#ifndef READMODBUSDATA_H
#define READMODBUSDATA_H

#include<Arduino.h>
#include<ArduinoJson.h>
#include<ModbusMaster.h>
#include<DevsbotEnergyLocal.h>
#include"meterparams.h"
#include<RTClib.h>

//pin defnition of modbus
#define MAX485_DE      19//Driver output Enable pin DE Active HIGH
#define MAX485_RE_NEG  18 //Receiver output Enable pin RE Active LOW

void postTransmission();
void preTransmission();

class readMeterData
{
    private:
        int modbusTimeOut=2000;

    public:
        
        ModbusMaster node;
        bool readDataFlag=0;
        //RTC_DS3231 rtc;
        int strlenOfOneSlave=1;
        bool stringlenFlag=0;
        // void countParam();
        //void rtcInit();
        void readModbusJson(uint8_t slaveNum);
        void powerfactorcalculation(void *vdrptr);
        // float powerfactorcalculation(float floatValue); //power factor calculation for each phase as per IEEE

        void conversion(void* ptr,uint8_t dataType,uint8_t program);
        void serialInit();
    
        
        unsigned long baudRate=9600;//default baud rate 
        uint32_t parityStopbit=0x800001e;//default parity and stop bit
        String noData="[]";
        // uint16_t regAddr[50]={0};
        // uint8_t regType;//specific to meter or Gateway.
        // uint16_t noRegToRead[50]={0};
        // char* vPins[50];  // virtual pins you may change here.
        // byte noParam[50]={0};
        // uint8_t dataType[50]={0};
        // byte size;
        // byte sizevPins;
};

#endif