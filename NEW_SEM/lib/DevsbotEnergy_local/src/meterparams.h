#ifndef METERPARAMS_H
#define METERPARAMS_H

class meterParams
{
    public:
    uint16_t regAddr[100]={0};
    uint8_t regType[100]={0};//specific to meter or Gateway.
    uint16_t noRegToRead[100]={0};
    char* vPins[100];  // virtual pins you may change here.
    byte noParam[100]={0};
    uint8_t dataType[100]={0};
    uint8_t endian[100]={0};
    uint8_t operation[100]={0};
    byte size;
    byte sizevPins;
};

#endif