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

int createDataPackage(unsigned char *packet,int sequenceNumber,const unsigned char *data,int dataSize){
    int index = 0;
    packet[index++]= 2; // control field (2 - data)
    packet[index++] = sequenceNumber % 100;
    
    packet[index++] = dataSize / 256;
    packet[index++] = dataSize % 256;

    memcpy(&packet[index], data, dataSize);
    index += dataSize;

    return index;
}


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort,serialPort);
    if (strcmp(role, "tx") == 0) {
        linkLayer.role = LlTx;
    } else {
        linkLayer.role = LlRx;
    }
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    if(llopen(linkLayer)!= 1) return -1;

    switch (linkLayer.role)
    {
    case LlTx:
        FILE *file = fopen(filename,"rb");

        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);


        //start control packet
        unsigned char startPacket[1024]; // ??????????????????????????
        int startPacketSize = createControlPackage(startPacket, 1, filename, fileSize);

        if(llwrite(startPacket, startPacketSize)== -1) return -1;

        //data packet
        unsigned char dataPacket[1024];
        unsigned char fileBuffer[1024];
        int sequenceNumber = 0;
        int bytesRead;
        while ((bytesRead = fread(fileBuffer, 1, sizeof(fileBuffer), file)) > 0) {
                int dataPacketSize = createDataPackage(dataPacket, sequenceNumber++, fileBuffer, bytesRead);
                if (llwrite(dataPacket, dataPacketSize) == -1) return -1;  
        }

        //end control packet
        unsigned char endPacket[1024];
        int endPacketSize = createControlPackage(endPacket, 3, filename, fileSize);
        if (llwrite(endPacket, endPacketSize) ==-1) return -1;
                
         
        fclose(file);
        break;

    case LlRx:
        unsigned char packet[1024];
        int packetSize;

        //pacote start
        packetSize = llread(packet);
        if (packetSize < 0 || packet[0] != 1) return -1;

        long receivedFileSize = 0;
        char receivedFilename[256];
        int index = 1; 

        while (index < packetSize) {
            unsigned char T = packet[index++]; // Tipo
            unsigned char L = packet[index++]; // Comprimento

            if (T == 0) { 
                memcpy(&receivedFileSize, &packet[index], sizeof(long));
                index += sizeof(long);
            } 
            else if (T == 1) { 
                int nameLength = L;
                memcpy(receivedFilename, &packet[index], nameLength);
                receivedFilename[nameLength] = '\0'; 
                index += nameLength;
            }
        }

        FILE *file = fopen(receivedFilename, "wb");

        while (1) {
            packetSize = llread(packet);
            if (packetSize < 0) break;

            if (packet[0] == 3) { // Pacote END
                break;
            } 
            else if (packet[0] == 2) { // Pacote de dados
                 
                int dataSize = packet[2] * 256 + packet[3]; // L2 e L1
                fwrite(&packet[4], 1, dataSize, file);
                }
            }

            
        fclose(file);
        break;
    
    default:
        break;
    }

}
