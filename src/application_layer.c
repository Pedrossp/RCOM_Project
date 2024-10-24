// Application layer protocol implementation

#include "application_layer.h"
#include <link_layer.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void longToBinary(long value, unsigned char *buffer) {
    for (int i = sizeof(long) - 1; i >= 0; i--) {
        buffer[i] = value & 0xFF;  
        value >>= 8;              
    }
}


int createControlPackage(unsigned char *packet,unsigned int c, const char* filename, long fileSize){
    int index = 0;
    packet[index++] = c; // control field (1- start , 3 -end)

    //file size

    packet[index++] = 0; // T
    packet[index++] = sizeof(long); // L 
    longToBinary(fileSize, &packet[index]);  
    index += sizeof(long);

    //file name
    
    packet[index++] = 1;  
    int nameLength = strlen(filename);
    packet[index++] = nameLength;  
    memcpy(&packet[index], filename, nameLength);
    index += nameLength;

    return index; // tamanho do packet
}


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort,serialPort);
    if(role == "tx"){
        linkLayer.role = LlTx;
    }
    else{
        linkLayer.role = LlRx;
    }
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    if(llopen(linkLayer)!= 1) return -1;

}
