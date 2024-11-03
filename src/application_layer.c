// Application layer protocol implementation

#include "application_layer.h"
#include <link_layer.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void longToBinary(long value, unsigned char *buffer) {
    for (int i = sizeof(long) - 1; i >= 0; i--) {
        buffer[i] = value & 0xFF;  
        value >>= 8;              
    }
}

long binaryToLong(unsigned char *buffer) {
    long value = 0;
    for (int i = 0; i < sizeof(long); i++) {
        value <<= 8;            // Desloca o valor atual 8 bits para a esquerda
        value |= buffer[i];     // Adiciona o próximo byte ao valor

    }
    return value;
}

void showProgress(long current, long total) {
    int progressWidth = 50; // Largura da barra de progresso
    int pos = (int)((double)current / total * progressWidth);
    printf("[");
    for (int i = 0; i < progressWidth; ++i) {
        if (i < pos) printf("▌");
        else if (i == pos) printf("▏");
        else printf(" ");
    }
    printf("] %3.0f%%\r", (double)current / total * 100);
    fflush(stdout);
}

int createControlPackage(unsigned char *packet,unsigned int c, const char* filename, long fileSize) {
    int index = 0;
    packet[index++] = c; // control field (1- start , 3 -end)

    // File size
    packet[index++] = 0; // T
    packet[index++] = sizeof(long); // L 
    longToBinary(fileSize, &packet[index]);
    index += sizeof(long);

    // File name
    packet[index++] = 1;  
    int nameLength = strlen(filename);
    packet[index++] = nameLength;  
    memcpy(&packet[index], filename, nameLength);
    index += nameLength;

    return index; // tamanho do packet
}

int createDataPackage(unsigned char *packet,int sequenceNumber,const unsigned char *data,int dataSize) {
    int index = 0;
    packet[index++] = 2; // control field (2 - data)
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
    printf("\n");
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

    if(llopen(linkLayer)!= 1)  return;

    clock_t startTime = clock();

    switch (linkLayer.role)
    {
    case LlTx:{
        FILE *file = fopen(filename,"rb");

        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);


        //start control packet
        unsigned char startPacket[MAX_PAYLOAD_SIZE];
        int startPacketSize = createControlPackage(startPacket, 1, filename, fileSize);

        if(llwrite(startPacket, startPacketSize)== -1) return;

        //data packet
        unsigned char dataPacket[MAX_PAYLOAD_SIZE];
        unsigned char fileBuffer[MAX_PAYLOAD_SIZE];

        int sequenceNumber = 0;
        int bytesRead;
        long totalBytesSent = 0;

        while ((bytesRead = fread(fileBuffer, 1, sizeof(fileBuffer), file)) > 0) {
                int dataPacketSize = createDataPackage(dataPacket, sequenceNumber++, fileBuffer, bytesRead);
                if (llwrite(dataPacket, dataPacketSize) == -1) return;

                totalBytesSent += bytesRead;
                showProgress(totalBytesSent, fileSize); 
        }

        //end control packet
        unsigned char endPacket[MAX_PAYLOAD_SIZE];
        int endPacketSize = createControlPackage(endPacket, 3, filename, fileSize);
        if (llwrite(endPacket, endPacketSize) ==-1) return;

        clock_t endTime = clock();
        double timeSpent = (double)(endTime - startTime) / CLOCKS_PER_SEC;

        fclose(file);
        llclose(timeSpent);  
    
        break;}

    case LlRx:{
        unsigned char packet[MAX_PAYLOAD_SIZE*2];
        int packetSize;

        //pacote start
        packetSize = llread(packet);
        if (packetSize < 0 || packet[0] != 1) return;

        unsigned char receivedFileSize[100];
        long size;
        unsigned char receivedFilename[256];
        int index = 1; 

        while (index < packetSize) {
            unsigned char T = packet[index++]; // Tipo
            unsigned char L = packet[index++]; // Comprimento

            if (T == 0) { 
                memcpy(&receivedFileSize, &packet[index], sizeof(long));
                size = binaryToLong(receivedFileSize);
                index += sizeof(long);
            } 
            else if (T == 1) { 
                int nameLength = L;
                memcpy(receivedFilename, &packet[index], nameLength);
                receivedFilename[nameLength] = '\0'; 
                index += nameLength;
            }
        }

        FILE *file1 = fopen(filename, "wb");

        long totalBytesReceived = 0;

        while (1) {
            while (packetSize = llread(packet) < 0);
            if (packetSize < 0) break;

            if (packet[0] == 3) { // Pacote END
                break;
            } 
            else if (packet[0] == 2) { // Pacote de dados
                 
                int dataSize = packet[2] * 256 + packet[3]; // L2 e L1
                fwrite(&packet[4], 1, dataSize, file1);

                totalBytesReceived += dataSize;
                showProgress(totalBytesReceived, 10968);
            }
        }

        clock_t endTime = clock();
        double timeSpent = (double)(endTime - startTime) / CLOCKS_PER_SEC;

        fclose(file1);
        llclose(timeSpent);
        break;
    }
    default:
        break;
    }

}
