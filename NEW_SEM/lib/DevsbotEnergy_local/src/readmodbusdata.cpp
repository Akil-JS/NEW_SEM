#include<readmodbusdata.h>
#include<sdcard.h>
// #include "DebugHelper.h"
#include "DebugConfig.h"
#include "DebugMacro.h"
// #include "DevsbotEnergyLocal.h"
#include "HardwareRTC.h" // <-- ADDED: Include the new RTC manager


// --- START OF MODIFICATION (BLOCK 1) ---
// Set to 1 to enable hardcoded data, 0 to use real Modbus.
#define HARDCODE_ENERGY_DATA 1
// --- END OF MODIFICATION (BLOCK 1) ---


ModbusMaster node;
byte keyValThree=0;
const char* values[]={"R","Y","B"};
extern meterParams mtrparam[12];
extern sdcard card;

/* ==========================================
      GATEWAY TYPE SELECTION (HARDCODED)
   ========================================== */
#define GW_TYPE_NTS     1  // ESP32 Wroom 32 (Standard + RE/DE Flow Control)
#define GW_TYPE_NTS401  2  // ESP32 Wroom 32 (Pins 32/33)
#define GW_TYPE_NTS701  3  // ESP32-S3 Wroom-1 (Pins 2/1)

//  vvvvvv  CHANGE THIS VALUE TO SWITCH GATEWAY  vvvvvv
#define GATEWAY_TYPE    GW_TYPE_NTS701
//  ^^^^^^  CHANGE THIS VALUE TO SWITCH GATEWAY  ^^^^^^


void readMeterData::serialInit()
{
  DEBUG("\n========================================");
  DEBUG("      Modbus Serial Initialization      ");
  DEBUG("========================================");

  DEBUG("[Config] Settings:");
  DEBUG(" - Baud Rate: " + String(baudRate));
  DEBUG(" - Parity/Stop: 0x" + String(parityStopbit, HEX) + " (Decimal: " + String(parityStopbit) + ")");

  // ---------------------------------------------------------
  // GATEWAY: NTS (Standard ESP32 Wroom 32 with RE/DE Control)
  // ---------------------------------------------------------
  #if GATEWAY_TYPE == GW_TYPE_NTS
    DEBUG("\n[Mode] NTS (Standard ESP32 - RE/DE Control)");
    DEBUG(" - Configuring RE/DE control pins...");
    
    // Flow Control Pins
    pinMode(MAX485_RE_NEG, OUTPUT);
    pinMode(MAX485_DE, OUTPUT);
    
    // Init in receive mode
    digitalWrite(MAX485_RE_NEG, LOW);
    digitalWrite(MAX485_DE, LOW);
    
    // Assign Callbacks for ModbusMaster
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    
    // Standard Serial2 Pins (usually GPIO 16/17)
    Serial2.begin(baudRate, parityStopbit);

  // ---------------------------------------------------------
  // GATEWAY: NTS401 (ESP32 Wroom 32 - Custom Pins 32/33)
  // ---------------------------------------------------------
  #elif GATEWAY_TYPE == GW_TYPE_NTS401
    DEBUG("\n[Mode] NTS401 (ESP32 Wroom 32 - Custom Pins)");
    DEBUG(" - RX Pin: 32");
    DEBUG(" - TX Pin: 33");
    
    // Initialize with Custom Pins
    Serial2.begin(baudRate, parityStopbit, 32, 33); // RX, TX

  // ---------------------------------------------------------
  // GATEWAY: NTS701 (ESP32-S3 Wroom-1 - Pins 2/1)
  // ---------------------------------------------------------
  #elif GATEWAY_TYPE == GW_TYPE_NTS701
    DEBUG("\n[Mode] NTS701 (ESP32-S3 Wroom-1)");
    // S3 Hardware Serial Definition
    #define RXD2 2 
    #define TXD2 1 
    
    DEBUG(" - RX Pin: " + String(RXD2));
    DEBUG(" - TX Pin: " + String(TXD2));

    // Initialize with S3 specific pins
    Serial2.begin(baudRate, parityStopbit, RXD2, TXD2);

  #else
    DEBUG("\n[ERROR] No valid GATEWAY_TYPE defined!");
  #endif

  DEBUG("Waiting for Serial2 to stabilize...");
  delay(200);

  if (Serial2) {
      DEBUG(" -> Serial2 initialized successfully.");
  } else {
      DEBUG(" -> [ERROR] Serial2 failed to initialize!");
  }
  DEBUG("========================================\n");
}
void postTransmission()   //Set up call back function9
{
  digitalWrite(MAX485_RE_NEG, LOW);
  digitalWrite(MAX485_DE, LOW);
}

void preTransmission()  //Set up call back function
{
  digitalWrite(MAX485_RE_NEG, HIGH);
  digitalWrite(MAX485_DE, HIGH);
}



void readMeterData:: conversion(void* ptr,uint8_t dataType,uint8_t program)
{
  float pfVal=0.0;
  if(program == 1)
  {
    DEBUG("multiply by 1000 program 1 ");

    if(dataType==1)
      *(uint16_t*)ptr*=1000;
    else if (dataType == 2) 
      *(float*)ptr *= 1000;
    else if (dataType == 3)
      *(uint32_t*)ptr *= 1000;
    else if (dataType == 4)
      *(uint64_t*)ptr *= 1000;
  }
  else if(program == 2)
  {
    

    if (dataType == 2)
    {
      DEBUG("tempfloat :"); DEBUG(*(float*)ptr);
      powerfactorcalculation(ptr);

    } 

  }
  else if (program == 3)
  {
    DEBUG("div by 10 program 3 ");
    if(dataType==1)
      *(uint16_t*)ptr/=10;
    else if (dataType == 2) 
      *(float*)ptr/= 10;
    else if (dataType == 3)
      *(uint32_t*)ptr/= 10;
    else if (dataType == 4)
      *(uint64_t*)ptr/= 10;

  }
  
}


void readMeterData::powerfactorcalculation(void *vdrptr) //power factor calculation for each phase as per IEEE
{
  DEBUG("pf cal function for schenider meter");

  float tempfloat=0.0;
  DEBUG("tempfloat :"); DEBUG(tempfloat);

  if (*(float*)vdrptr > 1)
  {
    tempfloat = 2 - *(float*)vdrptr;
  }
  else if (*(float*)vdrptr < -1)
  {
    tempfloat = -2 - *(float*)vdrptr;
    //PF is leading
  }

  DEBUG("temp float val: ");DEBUG(tempfloat);

  *(float*)vdrptr = tempfloat;
}


/** if wrong address is encountered move continue to nxt address value got jumped to nxt pin is corected */
// void readMeterData::readModbusJson(uint8_t slaveNum)
// {
//   DEBUG("readModbusJson begin\n");

//   readDataFlag=1;
//   DynamicJsonDocument docJson1(2048);
//   JsonObject meterData;
//   JsonObject phaseQty;
//   JsonArray metersArray;

//   uint8_t result;
//   // uint8_t noOfSuccessCnt=0;
//   uint8_t numRegToRead=0;
//   bool modpollError=0;
//   void *vp;
//   float floatValue;
//   byte slaveFailCount=0;
//   byte count;
//   bool timeOutFlag=0;
//   bool onceSetFlag;
//   uint8_t timeOutCnt=0;
//   const uint8_t noOfTimeout=2;
//   bool cnt=true;
//   uint16_t plcData=0;
//   uint32_t value_32;
//   uint64_t value_64;
//   //uint16_t data[2];
//   metersArray = docJson1.to<JsonArray>();
//   for (byte slave = 0; slave < slaveNum; slave++)
//   {
//     onceSetFlag=1;
//     count=0;
//     timeOutCnt=0;
//     Serial.printf("slave : %d\n",dBot.slaveIdArray[slave]);
//     //DEBUG("slave : %d\n",slave);
//     node.begin(dBot.slaveIdArray[slave],Serial2);
//     //node.begin(1,Serial2);

//     if(dBot.wifiStatus)
//       dBot.deviceContinue();
//     uint8_t i,j;
//     for(i=0,j=0;i<mtrparam[slave].size;i++) //sizevPins=6,size=6
//     {
      
//       DEBUG("reg Address: " );DEBUG(mtrparam[slave].regAddr[i]);
//       DEBUG(" vpins : ");DEBUG(mtrparam[slave].vPins[j]);
//       DEBUG(" count : ");DEBUG(count);
//       DEBUG("reg type : " );DEBUG(mtrparam[slave].regType[i]);
      

//       if(mtrparam[slave].dataType[i]==3)
//         numRegToRead=2;
//       else
//         numRegToRead=mtrparam[slave].dataType[i];


//       if(mtrparam[slave].regType[i]==1 )
//       {
//         DEBUG("coil Reg\n");
//         result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
//       if(mtrparam[slave].regType[i]==2 )
//       {
//         DEBUG("readDiscrete Reg\n");
//         result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
//       else if(mtrparam[slave].regType[i]==3)
//       {
//         DEBUG("holding Reg\n");
//         // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//         result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
//       else if(mtrparam[slave].regType[i]==4)
//       {
//         DEBUG("Input Reg\n");
//         // result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//         result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         // delay(200);
//       }
      
//       delay(200);



//       uint64_t checkStartTimer = millis();
//       uint64_t checkEndTimer = checkStartTimer + modbusTimeOut;
//       while (result!=node.ku8MBSuccess)
//       {
//         checkStartTimer=millis();
//         switch (result) 
//         {
//           case node.ku8MBIllegalFunction:
//             DEBUG("Illegal function.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal function" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBIllegalDataAddress:
//             DEBUG("Illegal data address.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data address" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBIllegalDataValue:
//             DEBUG("Illegal data value.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data value" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBSlaveDeviceFailure:
//             DEBUG("Slave device failure.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Slave device failure" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBInvalidSlaveID:
//             DEBUG("Invalid Slave ID.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid Slave ID" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBInvalidFunction:
//             DEBUG("Invalid function.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid function" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           case node.ku8MBResponseTimedOut:
//             DEBUG("Response timed out.");
//             // dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Response timed out " + String(mtrparam[slave].regAddr[i]));
//             break;
//           case node.ku8MBInvalidCRC:
//             DEBUG("Invalid CRC.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid CRC" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//           default:
//             DEBUG("Unknown error.");
//             dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Unknown error" + String(mtrparam[slave].regAddr[i]));
//             modpollError=1;
//             break;
//         }

//         if(modpollError)
//         {
//           DEBUG("error occured move to nxt data\n");
//           break;
//         }

//         if(mtrparam[slave].regType[i]==1)
//           result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         else if(mtrparam[slave].regType[i]==2)
//           result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//         else if(mtrparam[slave].regType[i]==3)
//         {
//           // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//           result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(300);
//         }
//         else if(mtrparam[slave].regType[i]==4)
//         {
//           //result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//           result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(300);
//         }

//         delay(200);


//         /** check data until a for a certain time  */
//         if(checkEndTimer<=checkStartTimer)
//         {
//           timeOutFlag=1;
//           timeOutCnt++;
//           Serial.printf("failcount: %d\n",slaveFailCount);

         
//           if(slaveNum == 1)
//             break;
//           else if(slaveFailCount==slaveNum)
//           {
//             dBot.sentEnergyMeterData(noData);
//             docJson1.clear();
//             readDataFlag=0;
//             return;
//           }
//           break;
//         }
//       }


//       /** modpoll error occur mainly it will occur bcz of giving a wrong address in application ,
//        * if reading a data from a slave fails go for nxt value ,like that go for all n value   */
//       if(modpollError)
//       {
//         DEBUG("make a modpollerror to 0\n");
//         modpollError=0;
//         if(i != mtrparam[slave].size - 1)
//         {

//           if(mtrparam[slave].noParam[j]==3)
//           {
//             DEBUG("count : "); DEBUG(count);
//             count++;

//             if(count < sizeof(values)/sizeof(values[0]))
//               continue;
//             else
//             {
//               j++;
//               count=0;
//               if(cnt==false)
//                 cnt=true;
//             }
//           }
//           else
//             j++;

//           continue;
//         }
//         else
//         {
//           count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
//           cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
//           DEBUG("this is last data form a json\n");
//           break;
//         }
//       }
      

//       if(timeOutFlag==1)
//       {
//         timeOutFlag=0;
//         if(timeOutCnt < noOfTimeout)
//         {
//           DEBUG("timeout occured \n");
//           if(mtrparam[slave].noParam[j]==3)
//           {
//             DEBUG("count : "); DEBUG(count);
//             count++;

//             if(count < sizeof(values)/sizeof(values[0]))
//             {
//               continue;
//             }
//             else
//             {
//               j++;
//               count=0;
//               if(cnt==false)
//                 cnt=true;
//             }
//           }
//           else
//             j++;

//           continue; // reading a nxt address of current slave
//         }
//         else
//         {
//           count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
//           cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
//           slaveFailCount++;
//           timeOutCnt=0;
//           break; // moving to a nxt slave
//         }
//       }

//       timeOutCnt=0;

//       if (onceSetFlag)
//       {
//         onceSetFlag=0;
//         meterData=metersArray.createNestedObject();
//         if(dBot.storeDataToSd)
//         {
//           DateTime now = rtcManager.now();
//           // // Format the date as YYYY-MM-DD HH:MM:SS
//           char updatetime[20]={0}; //init
//           sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
//           meterData["updatetime"]=updatetime;
//         }

//         meterData["slave_id"]=dBot.slaveIdArray[slave];
//       }

//       /**  parsing a uint16_t value  */
//       if(mtrparam[slave].dataType[i]==1) 
//       {
//         plcData=node.getResponseBuffer(0); //read a 16 bit data (LSB)
//         vp=&plcData;
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(uint16_t*)vp);
//         // meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
//       }
      
//       /**  parsing a float value  */
//       else if(mtrparam[slave].dataType[i]==2)//float value
//       {
//         value_32=0;
//         if(mtrparam[slave].endian[i]==1)
//           value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//         else if(mtrparam[slave].endian[i]==2)
//           value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
        
//         memcpy(&floatValue, &value_32, sizeof(floatValue));
        
        
//         vp=&floatValue;
//         // if(mtrparam[slave].operation[i]!=0 && !(mtrparam[slave].operation[i]==2))
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(float*)vp);
//       }

//       /**  parsing a uint32_t value  */
//       else if(mtrparam[slave].dataType[i]==3)//uint32_t value
//       {
//         value_32=0;
//         if(mtrparam[slave].endian[i]==1)
//           value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//           //value_32=(node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//         else if(mtrparam[slave].endian[i]==2)
//           value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
//          // value_32= (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
        

//         // DEBUG("lower buff : ");DEBUG(node.getResponseBuffer(0));
//         // DEBUG("higher buff : ");DEBUG(node.getResponseBuffer(1));
//         vp=&value_32;
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(uint32_t*)vp);
//       }

//       /**  parsing a uint64_t value  */
//       else if(mtrparam[slave].dataType[i]==4)//uint64_t
//       {
//         value_64=0;
//         if(mtrparam[slave].endian[i]==1)
//         {
//           //for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
//           for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
//             value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (index*16));
//         }
//         if(mtrparam[slave].endian[i]==2)
//         {
//           // for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
//           for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
//             value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (48-16*index));
//         }
//         vp=&value_64;
//         if(mtrparam[slave].operation[i]!=0)
//           conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//         DEBUG(" vp value : ");DEBUG(*(uint64_t*)vp);

//         // meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);
//       }


//       /** if virtual pin takes a ryb value eg.v1{r,y,b} */
//       if(mtrparam[slave].noParam[j]==3)
//       {
//         if(cnt==true)
//         {
//           cnt=false;
//           phaseQty=meterData[mtrparam[slave].vPins[j]].to<JsonObject>();
//         }
//         //DEBUG("count : %d\n",count);
//         if(mtrparam[slave].dataType[i]==1)
//           phaseQty[values[count++]]=String(*(uint16_t*)vp);
//         else if(mtrparam[slave].dataType[i]==2)
//           phaseQty[values[count++]]=String(*(float*)vp,2);
//         else if(mtrparam[slave].dataType[i]==3)
//           phaseQty[values[count++]]=String(*(uint32_t*)vp);
//         else
//           phaseQty[values[count++]]=(*(uint64_t*)vp);

    
        
//         if(count < sizeof(values)/sizeof(values[0])) //fix the != to <
//         {
//           // DEBUG("count : %d\n",count);
//           DEBUG("continue\n");
//           continue;
//         }
//         else
//         {
//           DEBUG("count is 0 and cnt is true\n");
//           count=0;
//           cnt=true;
//         }
//       }
//       else
//       {
//         // DEBUG("vpins : ");DEBUG(mtrparam[slave].vPins[j]);
//         DEBUG();
//         if(mtrparam[slave].dataType[i]==1)
//           meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
//         else if(mtrparam[slave].dataType[i]==2)
//           meterData[mtrparam[slave].vPins[j]]=String(*(float*)vp,2);
//         else if(mtrparam[slave].dataType[i]==3)
//           meterData[mtrparam[slave].vPins[j]]=String(*(uint32_t*)vp);
//         else
//           meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);

//       }
//       j++;

     

//     }
//   }

//   String jsonString="";
//   // String jsonstringOneObj="";
//   serializeJson(docJson1,jsonString);
//   //DEBUG("On the whole memory usage: ");DEBUG(docJson1.memoryUsage());
//   DEBUG("json String : " + jsonString);
//   // JsonObject firstObject = docJson1[0];
//   // serializeJson(firstObject,jsonstringOneObj);
//   // DEBUG("length of one json string : ");DEBUG(jsonstringOneObj.length());//(lenof one object * numofslaves) + 2
//   // if(jsonString.length()>0 )
//   // {
//   //   if(dBot.SendDataToServer)
//   //   {
//   //     if(!dBot.storeDataToSd)
//   //       dBot.sentEnergyMeterData(jsonString);
//   //     else 
//   //     {
//   //       if (SD.cardType() != CARD_NONE && jsonString != "[]") {
//   //           DEBUG("Saving energy data to SD card queue...");
//   //           card.saveEnergyLog(jsonString); // Use the new, simple function
//   //       } else {
//   //           DEBUG("!!No SD card present in SD card slot!!");
//   //       }
//   //     }
//   //   }
//   //   else
//   //     DEBUG("!!sending data to server is blocked!!");

//   // }

//   // if(jsonString.length()>0 )
//   if(jsonString.length() > 0 && jsonString != "[]")
//   {
//     if(dBot.SendDataToServer)
//     {
//       if(!dBot.storeDataToSd)
//         dBot.sentEnergyMeterData(jsonString);
//       else 
//       {
//         // to the thread-safe card.saveEnergyLog() function.
//         DEBUG("Saving energy data to SD card queue...");
//         card.saveEnergyLog(jsonString);
//       }
//     }
//     else
//       DEBUG("!!sending data to server is blocked!!");

//   }

//   jsonString.reserve(0);
//   docJson1.clear();
//   DEBUG("readModbusJson End\n");
//   DEBUG();
//   readDataFlag=0;
// }



/** if wrong address is encountered move continue to nxt address value got jumped to nxt pin is corected */
// void readMeterData::readModbusJson(uint8_t slaveNum)
// {
//   DEBUG("readModbusJson begin\n");

//   // --- FIX: Declare common variables *once* outside the #if block ---
//   DynamicJsonDocument docJson1(2048);
//   JsonArray metersArray;
//   JsonObject meterData;
//   String jsonString = ""; // Initialize as empty
//   // --- END FIX ---


//   // --- START OF MODIFICATION (BLOCK 2) ---
//   // This block will run *instead* of the real Modbus code if the flag is enabled.
//   #if HARDCODE_ENERGY_DATA == 1
//     DEBUG("[TEST] Using hardcoded energy data. No real Modbus read.");
    
//     // 1. Create the array (doc is already created)
//     metersArray = docJson1.to<JsonArray>();

//     // 2. Create a test object for one "slave"
//     meterData = metersArray.createNestedObject();
//     meterData["slave_id"] = 1; // Test slave ID 1

//     // 3. Get a timestamp
//     if (rtcManager.isReady()) {
//         DateTime now = rtcManager.now();
//         char updatetime[20] = {0};
//         // Format as YYYY-MM-DD HH:MM:SS
//         sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", 
//                 now.year(), now.month(), now.day(),
//                 now.hour(), now.minute(), now.second());
//         meterData["updatetime"] = updatetime;
//     } else {
//         meterData["updatetime"] = "2025-10-30 16:10:00"; // Fallback timestamp
//     }

//     // 4. Add your sample hardcoded data (with the new V-pins)
//     meterData["V3"] = String(240.5 + (random(0, 200) / 100.0), 2);
//     meterData["V1"] = String(1.25 + (random(0, 50) / 100.0), 2);
//     meterData["V2"] = String(0.95 + (random(0, 5) / 100.0), 2);
//     meterData["V0"] = 12345.67 + (millis() / 10000.0);

//     // 5. Serialize
//     serializeJson(docJson1, jsonString);
//     DEBUG("json String : " + jsonString);

//     readDataFlag = 0; // Ensure flag is cleared
//     // We will let the common saving block at the end of the function handle saving
  
//   #else // This is the original code path (if HARDCODE_ENERGY_DATA is 0)
  
//     // --- FIX: Remove redeclarations ---
//     readDataFlag=1;
//     // DynamicJsonDocument docJson1(2048); // <-- REMOVED (Declared above)
//     // JsonObject meterData; // <-- REMOVED (Declared above)
//     JsonObject phaseQty;
//     // JsonArray metersArray; // <-- REMOVED (Declared above)

//     uint8_t result;
//     // uint8_t noOfSuccessCnt=0;
//     uint8_t numRegToRead=0;
//     bool modpollError=0;
//     void *vp;
//     float floatValue;
//     byte slaveFailCount=0;
//     byte count;
//     bool timeOutFlag=0;
//     bool onceSetFlag;
//     uint8_t timeOutCnt=0;
//     const uint8_t noOfTimeout=2;
//     bool cnt=true;
//     uint16_t plcData=0;
//     uint32_t value_32;
//     uint64_t value_64;
//     //uint16_t data[2];
//     metersArray = docJson1.to<JsonArray>();
//     for (byte slave = 0; slave < slaveNum; slave++)
//     {
//       onceSetFlag=1;
//       count=0;
//       timeOutCnt=0;
//       Serial.printf("slave : %d\n",dBot.slaveIdArray[slave]);
//       //DEBUG("slave : %d\n",slave);
//       node.begin(dBot.slaveIdArray[slave],Serial2);
//       //node.begin(1,Serial2);

//       if(dBot.wifiStatus)
//         dBot.deviceContinue();
//       uint8_t i,j;
//       for(i=0,j=0;i<mtrparam[slave].size;i++) //sizevPins=6,size=6
//       {
        
//         DEBUG("reg Address: " );DEBUG(mtrparam[slave].regAddr[i]);
//         DEBUG(" vpins : ");DEBUG(mtrparam[slave].vPins[j]);
//         DEBUG(" count : ");DEBUG(count);
//         DEBUG("reg type : " );DEBUG(mtrparam[slave].regType[i]);
        

//         if(mtrparam[slave].dataType[i]==3)
//           numRegToRead=2;
//         else
//           numRegToRead=mtrparam[slave].dataType[i];


//         if(mtrparam[slave].regType[i]==1 )
//         {
//           DEBUG("coil Reg\n");
//           result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(200);
//         }
//         if(mtrparam[slave].regType[i]==2 )
//         {
//           DEBUG("readDiscrete Reg\n");
//           result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(200);
//         }
//         else if(mtrparam[slave].regType[i]==3)
//         {
//           DEBUG("holding Reg\n");
//           // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//           result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(200);
//         }
//         else if(mtrparam[slave].regType[i]==4)
//         {
//           DEBUG("Input Reg\n");
//           // result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//           result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           // delay(200);
//         }
        
//         delay(200);



//         uint64_t checkStartTimer = millis();
//         uint64_t checkEndTimer = checkStartTimer + modbusTimeOut;
//         while (result!=node.ku8MBSuccess)
//         {
//           checkStartTimer=millis();
//           switch (result) 
//           {
//             case node.ku8MBIllegalFunction:
//               DEBUG("Illegal function.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal function" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//             case node.ku8MBIllegalDataAddress:
//               DEBUG("Illegal data address.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data address" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//             case node.ku8MBIllegalDataValue:
//               DEBUG("Illegal data value.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Illegal data value" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//             case node.ku8MBSlaveDeviceFailure:
//               DEBUG("Slave device failure.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Slave device failure" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//             case node.ku8MBInvalidSlaveID:
//               DEBUG("Invalid Slave ID.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid Slave ID" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//             case node.ku8MBInvalidFunction:
//               DEBUG("Invalid function.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid function" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//             case node.ku8MBResponseTimedOut:
//               DEBUG("Response timed out.");
//               // dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Response timed out " + String(mtrparam[slave].regAddr[i]));
//               break;
//             case node.ku8MBInvalidCRC:
//               DEBUG("Invalid CRC.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Invalid CRC" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//             default:
//               DEBUG("Unknown error.");
//               dBot.deviceLog("slaveId: " + String(dBot.slaveIdArray[slave]) + "Unknown error" + String(mtrparam[slave].regAddr[i]));
//               modpollError=1;
//               break;
//           }

//           if(modpollError)
//           {
//             DEBUG("error occured move to nxt data\n");
//             break;
//           }

//           if(mtrparam[slave].regType[i]==1)
//             result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           else if(mtrparam[slave].regType[i]==2)
//             result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//           else if(mtrparam[slave].regType[i]==3)
//           {
//             // result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//             result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//             // delay(300);
//           }
//           else if(mtrparam[slave].regType[i]==4)
//           {
//             //result = node.readInputRegisters(mtrparam[slave].regAddr[i],mtrparam[slave].noRegToRead[i]); // Read two registers starting at address 0x0000
//             result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead); // Read two registers starting at address 0x0000
//             // delay(300);
//           }

//           delay(200);


//           /** check data until a for a certain time  */
//           if(checkEndTimer<=checkStartTimer)
//           {
//             timeOutFlag=1;
//             timeOutCnt++;
//             Serial.printf("failcount: %d\n",slaveFailCount);

          
//             if(slaveNum == 1)
//               break;
//             else if(slaveFailCount==slaveNum)
//             {
//               dBot.sentEnergyMeterData(noData);
//               docJson1.clear();
//               readDataFlag=0;
//               return;
//             }
//             break;
//           }
//         }


//         /** modpoll error occur mainly it will occur bcz of giving a wrong address in application ,
//         * if reading a data from a slave fails go for nxt value ,like that go for all n value   */
//         if(modpollError)
//         {
//           DEBUG("make a modpollerror to 0\n");
//           modpollError=0;
//           if(i != mtrparam[slave].size - 1)
//           {

//             if(mtrparam[slave].noParam[j]==3)
//             {
//               DEBUG("count : "); DEBUG(count);
//               count++;

//               if(count < sizeof(values)/sizeof(values[0]))
//                 continue;
//               else
//               {
//                 j++;
//                 count=0;
//                 if(cnt==false)
//                   cnt=true;
//               }
//             }
//             else
//               j++;

//             continue;
//           }
//           else
//           {
//             count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
//             cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
//             DEBUG("this is last data form a json\n");
//             break;
//           }
//         }
        

//         if(timeOutFlag==1)
//         {
//           timeOutFlag=0;
//           if(timeOutCnt < noOfTimeout)
//           {
//             DEBUG("timeout occured \n");
//             if(mtrparam[slave].noParam[j]==3)
//             {
//               DEBUG("count : "); DEBUG(count);
//               count++;

//               if(count < sizeof(values)/sizeof(values[0]))
//               {
//                 continue;
//               }
//               else
//               {
//                 j++;
//                 count=0;
//                 if(cnt==false)
//                   cnt=true;
//               }
//             }
//             else
//               j++;

//             continue; // reading a nxt address of current slave
//           }
//           else
//           {
//             count=0; //if 3ph values are read from a slaves device ,in that instance a time out error occured and program move to nxt slave 
//             cnt=true;// in that instance while reading a 3ph qty  cnt=false ,count=1.
//             slaveFailCount++;
//             timeOutCnt=0;
//             break; // moving to a nxt slave
//           }
//         }

//         timeOutCnt=0;

//         if (onceSetFlag)
//         {
//           onceSetFlag=0;
//           meterData=metersArray.createNestedObject();
//           if(dBot.storeDataToSd)
//           {
//             DateTime now = rtcManager.now();
//             // // Format the date as YYYY-MM-DD HH:MM:SS
//             char updatetime[20]={0}; //init
//             sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
//             meterData["updatetime"]=updatetime;
//           }

//           meterData["slave_id"]=dBot.slaveIdArray[slave];
//         }

//         /** parsing a uint16_t value  */
//         if(mtrparam[slave].dataType[i]==1) 
//         {
//           plcData=node.getResponseBuffer(0); //read a 16 bit data (LSB)
//           vp=&plcData;
//           if(mtrparam[slave].operation[i]!=0)
//             conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//           DEBUG(" vp value : ");DEBUG(*(uint16_t*)vp);
//           // meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
//         }
        
//         /** parsing a float value  */
//         else if(mtrparam[slave].dataType[i]==2)//float value
//         {
//           value_32=0;
//           if(mtrparam[slave].endian[i]==1)
//             value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//           else if(mtrparam[slave].endian[i]==2)
//             value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
          
//           memcpy(&floatValue, &value_32, sizeof(floatValue));
          
          
//           vp=&floatValue;
//           // if(mtrparam[slave].operation[i]!=0 && !(mtrparam[slave].operation[i]==2))
//           if(mtrparam[slave].operation[i]!=0)
//             conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//           DEBUG(" vp value : ");DEBUG(*(float*)vp);
//         }

//         /** parsing a uint32_t value  */
//         else if(mtrparam[slave].dataType[i]==3)//uint32_t value
//         {
//           value_32=0;
//           if(mtrparam[slave].endian[i]==1)
//             value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//             //value_32=(node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);//selec & Elmeasure
//           else if(mtrparam[slave].endian[i]==2)
//             value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
//           // value_32= (node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);//Schneider Electric
          

//           // DEBUG("lower buff : ");DEBUG(node.getResponseBuffer(0));
//           // DEBUG("higher buff : ");DEBUG(node.getResponseBuffer(1));
//           vp=&value_32;
//           if(mtrparam[slave].operation[i]!=0)
//             conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//           DEBUG(" vp value : ");DEBUG(*(uint32_t*)vp);
//         }

//         /** parsing a uint64_t value  */
//         else if(mtrparam[slave].dataType[i]==4)//uint64_t
//         {
//           value_64=0;
//           if(mtrparam[slave].endian[i]==1)
//           {
//             //for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
//             for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
//               value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (index*16));
//           }
//           if(mtrparam[slave].endian[i]==2)
//           {
//             // for(byte index=0;mtrparam[slave].noRegToRead[i]==4 && index<mtrparam[slave].noRegToRead[i];index++)
//             for(byte index=0;numRegToRead==4 && index<numRegToRead;index++)
//               value_64=value_64 + ((uint64_t)node.getResponseBuffer(index) << (48-16*index));
//           }
//           vp=&value_64;
//           if(mtrparam[slave].operation[i]!=0)
//             conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
//           DEBUG(" vp value : ");DEBUG(*(uint64_t*)vp);

//           // meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);
//         }


//         /** if virtual pin takes a ryb value eg.v1{r,y,b} */
//         if(mtrparam[slave].noParam[j]==3)
//         {
//           if(cnt==true)
//           {
//             cnt=false;
//             phaseQty=meterData[mtrparam[slave].vPins[j]].to<JsonObject>();
//           }
//           //DEBUG("count : %d\n",count);
//           if(mtrparam[slave].dataType[i]==1)
//             phaseQty[values[count++]]=String(*(uint16_t*)vp);
//           else if(mtrparam[slave].dataType[i]==2)
//             phaseQty[values[count++]]=String(*(float*)vp,2);
//           else if(mtrparam[slave].dataType[i]==3)
//             phaseQty[values[count++]]=String(*(uint32_t*)vp);
//           else
//             phaseQty[values[count++]]=(*(uint64_t*)vp);

      
          
//           if(count < sizeof(values)/sizeof(values[0])) //fix the != to <
//           {
//             // DEBUG("count : %d\n",count);
//             DEBUG("continue\n");
//             continue;
//           }
//           else
//           {
//             DEBUG("count is 0 and cnt is true\n");
//             count=0;
//             cnt=true;
//           }
//         }
//         else
//         {
//           // DEBUG("vpins : ");DEBUG(mtrparam[slave].vPins[j]);
//           DEBUG();
//           if(mtrparam[slave].dataType[i]==1)
//             meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
//           else if(mtrparam[slave].dataType[i]==2)
//             meterData[mtrparam[slave].vPins[j]]=String(*(float*)vp,2);
//           else if(mtrparam[slave].dataType[i]==3)
//             meterData[mtrparam[slave].vPins[j]]=String(*(uint32_t*)vp);
//           else
//             meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);

//         }
//         j++;

      

//       }
//     }

//     // --- FIX: Moved serialization and flag reset out of the #else block ---
//     serializeJson(docJson1, jsonString);
//     DEBUG("json String : " + jsonString);
//     readDataFlag=0; // Reset flag here
  
//   #endif // HARDCODE_ENERGY_DATA == 1
//   // --- END OF MODIFICATION (BLOCK 2) ---


// // [REPLACE IT WITH THIS NEW LOGIC]
//   if(jsonString.length() > 0 && jsonString != "[]")
//   {
//       if(dBot.SendDataToServer)
//       {
//           // Check if SD storage is enabled AND card is working
//           if(dBot.storeDataToSd && card.cardMounted) 
//           {
//               // --- NEW LOGIC (Point 1) ---
//               // 1. Always store to SD card first.
//               DEBUG("Saving energy data to SD card...");
//               bool sdSaveSuccess = card.saveEnergyLog(jsonString);

//               // 2. If save succeeded and WiFi is on, trigger immediate processing.
//               if (sdSaveSuccess && dBot.wifiStatus) {
//                   DEBUG("Energy saved. Triggering immediate SD processing...");
//                   // This will trigger the LogManager to read from its read-file
//                   // and attempt to send the data to the server.
//                   card.processEnergyLogQueue(); // <-- ADD THIS
//               } else if (!sdSaveSuccess) {
//                   DEBUG("SD card save failed! Sending to server instead...");
//                   dBot.sentEnergyMeterData(jsonString);
//               } else {
//                   DEBUG("Energy saved to SD (WiFi is offline). Will be sent later.");
//               }
//               // --- END NEW LOGIC ---
//           }
//           else if (dBot.wifiStatus) // Not storing to SD, but WiFi is on
//           {
//               // storeDataToSd is false, send directly to server
//               DEBUG("SD storage disabled. Sending Energy data live...");
//               dBot.sentEnergyMeterData(jsonString);
//           }
//           else
//           {
//               DEBUG("SD storage disabled and WiFi offline. Energy data lost.");
//           }
//       }
//       else
//       {
//           DEBUG("!!sending data to server is blocked!!");
//       }
//   }

//   jsonString.reserve(0);
//   docJson1.clear();
//   DEBUG("readModbusJson End\n");
//   DEBUG();
//   // readDataFlag=0; // <-- Moved up
// }

/* In: lib/DevsbotEnergy_local/src/readmodbusdata.cpp */

void readMeterData::readModbusJson(uint8_t slaveNum)
{
  DEBUG("\n=========================================================================================");
  DEBUG("|                           MODBUS READ CYCLE START                                     |");
  DEBUG("|=======================================================================================|");
  DEBUG("| Slave | Addr  | Type | VPin       | Raw Data (Hex)     | Final Value      |");
  DEBUG("|-------|-------|------|------------|--------------------|------------------|");

  // --- FIX: Declare common variables *once* ---
  DynamicJsonDocument docJson1(4096); // Increased size for safety
  JsonArray metersArray = docJson1.to<JsonArray>();
  JsonObject meterData;
  JsonObject phaseQty;
  String jsonString = "";
  // --- END FIX ---

  // Local variables for table printing
  char rawHexStr[20];
  char finalValStr[20];
  String vPinName;

  #if HARDCODE_ENERGY_DATA == 1
      DEBUG("| [TEST MODE ACTIVE] Using hardcoded data. skipping real Modbus reads.           |");
      DEBUG("==================================================================================");
      // ... (Keep your hardcoded test logic here if needed, or remove if not using anymore) ...
      metersArray = docJson1.to<JsonArray>();
      meterData = metersArray.createNestedObject();
      meterData["slave_id"] = 1;
      if (rtcManager.isReady()) {
          DateTime now = rtcManager.now();
          char updatetime[20] = {0};
          sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
          meterData["updatetime"] = updatetime;
      } else { meterData["updatetime"] = "2025-10-30 16:10:00"; }
      meterData["V3"] = String(240.5 + (random(0, 200) / 100.0), 2);
      meterData["V1"] = String(1.25 + (random(0, 50) / 100.0), 2);
      meterData["V2"] = String(0.95 + (random(0, 5) / 100.0), 2);
      meterData["V0"] = 12345.67 + (millis() / 10000.0);
       readDataFlag=0;

  #else // Real Modbus Logic
    readDataFlag=1;
    uint8_t result;
    uint8_t numRegToRead=0;
    bool modpollError=0;
    void *vp;
    float floatValue;
    byte slaveFailCount=0;
    byte count=0;
    bool timeOutFlag=0;
    bool onceSetFlag;
    uint8_t timeOutCnt=0;
    const uint8_t noOfTimeout=2;
    bool cnt=true;
    uint16_t plcData=0;
    uint32_t value_32=0;
    uint64_t value_64=0;

    for (byte slave = 0; slave < slaveNum; slave++)
    {
      onceSetFlag=1;
      count=0;
      timeOutCnt=0;
      // Serial.printf("slave : %d\n",dBot.slaveIdArray[slave]); // <-- Removed, covered by table
      node.begin(dBot.slaveIdArray[slave],Serial2);

      if(dBot.wifiStatus) dBot.deviceContinue();

      uint8_t i,j;
      for(i=0,j=0;i<mtrparam[slave].size;i++)
      {
        // DEBUG("reg Address: " );DEBUG(mtrparam[slave].regAddr[i]); // <-- Removed redundant debugs
        
        if(mtrparam[slave].dataType[i]==3) numRegToRead=2;
        else numRegToRead=mtrparam[slave].dataType[i];

        // --- READ MODBUS REGISTERS ---
        if(mtrparam[slave].regType[i]==1)      result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead);
        else if(mtrparam[slave].regType[i]==2) result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead);
        else if(mtrparam[slave].regType[i]==3) result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead);
        else if(mtrparam[slave].regType[i]==4) result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead);
        
        delay(200); // Wait for response

        uint64_t checkStartTimer = millis();
        uint64_t checkEndTimer = checkStartTimer + modbusTimeOut;

        while (result!=node.ku8MBSuccess)
        {
            checkStartTimer=millis();
            // ... (Keep existing error handling switch-case here, it's good for deep debugging) ...
             switch (result) {
                case node.ku8MBIllegalFunction: DEBUG("[Modbus] Illegal function"); modpollError=1; break;
                case node.ku8MBIllegalDataAddress: DEBUG("[Modbus] Illegal data address"); modpollError=1; break;
                case node.ku8MBIllegalDataValue: DEBUG("[Modbus] Illegal data value"); modpollError=1; break;
                case node.ku8MBSlaveDeviceFailure: DEBUG("[Modbus] Slave device failure"); modpollError=1; break;
                case node.ku8MBInvalidSlaveID: DEBUG("[Modbus] Invalid Slave ID"); modpollError=1; break;
                case node.ku8MBInvalidFunction: DEBUG("[Modbus] Invalid function"); modpollError=1; break;
                case node.ku8MBResponseTimedOut: /* DEBUG("[Modbus] Response timed out"); */ break;
                case node.ku8MBInvalidCRC: DEBUG("[Modbus] Invalid CRC"); modpollError=1; break;
                default: DEBUG("[Modbus] Unknown error"); modpollError=1; break;
            }

            if(modpollError) break;

            // Retry logic
            if(mtrparam[slave].regType[i]==1)      result = node.readCoils(mtrparam[slave].regAddr[i],numRegToRead);
            else if(mtrparam[slave].regType[i]==2) result = node.readDiscreteInputs(mtrparam[slave].regAddr[i],numRegToRead);
            else if(mtrparam[slave].regType[i]==3) result = node.readHoldingRegisters(mtrparam[slave].regAddr[i],numRegToRead);
            else if(mtrparam[slave].regType[i]==4) result = node.readInputRegisters(mtrparam[slave].regAddr[i],numRegToRead);
            delay(200);

            if(checkEndTimer<=checkStartTimer) {
                timeOutFlag=1;
                timeOutCnt++;
                if(slaveNum == 1) break;
                else if(slaveFailCount==slaveNum) {
                    dBot.sentEnergyMeterData(noData);
                    docJson1.clear();
                    readDataFlag=0;
                    return;
                }
                break;
            }
        }

        // --- HANDLE ERRORS & TIMEOUTS ---
        if(modpollError || timeOutFlag) {
             // ... (Keep existing error continuation logic) ...
             // If error, print a failed row in the table
             Serial.printf("| %-5d | %-5d | %-4d | %-10s | [READ FAILED]        | --               |\n", 
                dBot.slaveIdArray[slave], mtrparam[slave].regAddr[i], mtrparam[slave].dataType[i], mtrparam[slave].vPins[j]);

             if (modpollError) {
                 modpollError=0;
                 // ... (logic to skip VPin index j correctly) ...
                  if(i != mtrparam[slave].size - 1) {
                    if(mtrparam[slave].noParam[j]==3) { count++; if(count < 3) continue; else { j++; count=0; if(!cnt) cnt=true; } }
                    else j++;
                    continue;
                  } else { break; }
             }
             if (timeOutFlag) {
                 timeOutFlag=0;
                 if(timeOutCnt < noOfTimeout) {
                    if(mtrparam[slave].noParam[j]==3) { count++; if(count < 3) { continue; } else { j++; count=0; if(!cnt) cnt=true; } }
                    else j++;
                    continue; 
                 } else {
                     slaveFailCount++; timeOutCnt=0; break; // Next slave
                 }
             }
        }
        timeOutCnt=0;

        // --- INITIALIZE NEW SLAVE OBJECT ---
        if (onceSetFlag) {
          onceSetFlag=0;
          meterData=metersArray.createNestedObject();
          if(dBot.storeDataToSd && rtcManager.isReady()) {
            DateTime now = rtcManager.now();
            char updatetime[20]={0};
            sprintf(updatetime, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
            meterData["updatetime"]=updatetime;
          }
          meterData["slave_id"]=dBot.slaveIdArray[slave];
        }

        // --- PROCESS DATA & PREPARE TABLE ROW ---
        vPinName = String(mtrparam[slave].vPins[j]);

        // 1. UINT16
        if(mtrparam[slave].dataType[i]==1) {
          plcData=node.getResponseBuffer(0);
          sprintf(rawHexStr, "0x%04X", plcData); // Capture Raw HEX
          vp=&plcData;
          if(mtrparam[slave].operation[i]!=0) conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
          sprintf(finalValStr, "%u", *(uint16_t*)vp); // Capture Final Value
        }
        // 2. FLOAT
        else if(mtrparam[slave].dataType[i]==2) {
          if(mtrparam[slave].endian[i]==1) value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);
          else if(mtrparam[slave].endian[i]==2) value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);
          
          sprintf(rawHexStr, "0x%08X", value_32); // Capture Raw HEX
          memcpy(&floatValue, &value_32, sizeof(floatValue));
          vp=&floatValue;
          if(mtrparam[slave].operation[i]!=0) conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
          dtostrf(*(float*)vp, 1, 2, finalValStr); // Capture Final Float (2 decimal places)
        }
        // 3. UINT32
        else if(mtrparam[slave].dataType[i]==3) {
          if(mtrparam[slave].endian[i]==1) value_32=((uint32_t)node.getResponseBuffer(1) << 16) + node.getResponseBuffer(0);
          else if(mtrparam[slave].endian[i]==2) value_32= ((uint32_t)node.getResponseBuffer(0) << 16) + node.getResponseBuffer(1);
          
          sprintf(rawHexStr, "0x%08X", value_32); // Capture Raw HEX
          vp=&value_32;
          if(mtrparam[slave].operation[i]!=0) conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
          sprintf(finalValStr, "%u", *(uint32_t*)vp); // Capture Final Value
        }
        // 4. UINT64
        else if(mtrparam[slave].dataType[i]==4) {
           value_64=0;
           // ... (Keep your existing 64-bit endianness logic here if needed, simplified for brevity below) ...
           // For now, just capturing the first 32 bits for HEX to save space, or use a custom print if needed.
           // Assuming standard iteration for now:
           for(byte idx=0; idx<numRegToRead; idx++) {
               value_64 |= ((uint64_t)node.getResponseBuffer(idx) << (idx*16)); 
           }
           sprintf(rawHexStr, "0x%08X...", (uint32_t)value_64); // Simplified HEX for 64-bit
           vp=&value_64;
           if(mtrparam[slave].operation[i]!=0) conversion(vp,mtrparam[slave].dataType[i],mtrparam[slave].operation[i]);
           // sprintf(finalValStr, "%llu", *(uint64_t*)vp); // Might need special handling depending on compiler
           strcpy(finalValStr, "UINT64_VAL"); 
        }

        // --- ADD TO JSON & PRINT TABLE ROW ---
        if(mtrparam[slave].noParam[j]==3) {
           if(cnt==true) { cnt=false; phaseQty=meterData[mtrparam[slave].vPins[j]].to<JsonObject>(); }
           
           String currentPhase = values[count];
           String fullVPin = vPinName + "(" + currentPhase + ")";
           
           // Print Table Row for this phase
           Serial.printf("| %-5d | %-5d | %-4d | %-10s | %-18s | %-16s |\n", 
                dBot.slaveIdArray[slave], mtrparam[slave].regAddr[i], mtrparam[slave].dataType[i], 
                fullVPin.c_str(), rawHexStr, finalValStr);

           // Add to JSON
           if(mtrparam[slave].dataType[i]==1)      phaseQty[currentPhase]=String(*(uint16_t*)vp);
           else if(mtrparam[slave].dataType[i]==2) phaseQty[currentPhase]=String(*(float*)vp,2);
           else if(mtrparam[slave].dataType[i]==3) phaseQty[currentPhase]=String(*(uint32_t*)vp);
           else                                    phaseQty[currentPhase]=(*(uint64_t*)vp);
           
           count++;
           if(count < 3) continue; 
           else { count=0; cnt=true; }
        } else {
           // Print Table Row for single value
           Serial.printf("| %-5d | %-5d | %-4d | %-10s | %-18s | %-16s |\n", 
                dBot.slaveIdArray[slave], mtrparam[slave].regAddr[i], mtrparam[slave].dataType[i], 
                vPinName.c_str(), rawHexStr, finalValStr);

           // Add to JSON
           if(mtrparam[slave].dataType[i]==1)      meterData[mtrparam[slave].vPins[j]]=String(*(uint16_t*)vp);
           else if(mtrparam[slave].dataType[i]==2) meterData[mtrparam[slave].vPins[j]]=String(*(float*)vp,2);
           else if(mtrparam[slave].dataType[i]==3) meterData[mtrparam[slave].vPins[j]]=String(*(uint32_t*)vp);
           else                                    meterData[mtrparam[slave].vPins[j]]=(*(uint64_t*)vp);
        }
        j++;
      } // End Register Loop
    } // End Slave Loop
  #endif

  serializeJson(docJson1, jsonString);

  DEBUG("|=======================================================================================|");
  DEBUG("|                                FINAL JSON OUTPUT                                      |");
  DEBUG("|=======================================================================================|");
  DEBUG(jsonString);
  DEBUG("|=======================================================================================|\n");

// [REPLACE IT WITH THIS NEW LOGIC]
  if(jsonString.length() > 0 && jsonString != "[]")
  {
      if(dBot.SendDataToServer)
      {
          // Check if SD storage is enabled AND card is working
          if(dBot.storeDataToSd && card.cardMounted) 
          {
              // --- NEW LOGIC (Point 1) ---
              // 1. Always store to SD card first.
              DEBUG("Saving energy data to SD card...");
              bool sdSaveSuccess = card.saveEnergyLog(jsonString);

              // 2. If save succeeded and WiFi is on, trigger immediate processing.
              if (sdSaveSuccess && dBot.wifiStatus) {
                  DEBUG("Energy saved. Triggering immediate SD processing...");
                  // This will trigger the LogManager to read from its read-file
                  // and attempt to send the data to the server.
                  card.processEnergyLogQueue(); // <-- ADD THIS
              } else if (!sdSaveSuccess) {
                  DEBUG("SD card save failed! Sending to server instead...");
                  dBot.sentEnergyMeterData(jsonString);
              } else {
                  DEBUG("Energy saved to SD (WiFi is offline). Will be sent later.");
              }
              // --- END NEW LOGIC ---
          }
          else if (dBot.wifiStatus) // Not storing to SD, but WiFi is on
          {
              // storeDataToSd is false, send directly to server
              DEBUG("SD storage disabled. Sending Energy data live...");
              dBot.sentEnergyMeterData(jsonString);
          }
          else
          {
              DEBUG("SD storage disabled and WiFi offline. Energy data lost.");
          }
      }
      else
      {
          DEBUG("!!sending data to server is blocked!!");
      }
  }

  jsonString.reserve(0);
  docJson1.clear();
  readDataFlag=0;
  
}

// void readMeterData::rtcInit()
// {
//   DEBUG("rtc init\n");
//   if (!rtc.begin()) 
//   {
//     DEBUG("Couldn't find RTC");
//     while (1);
//   }

//   // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//   rtc.adjust(DateTime(__DATE__, __TIME__));
//   // Check if the RTC lost power and if so, set the time
//   if (rtc.lostPower()) 
//   {
//     DEBUG("RTC lost power, let's set the time!");
//     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//   }
// }

readMeterData energy;
