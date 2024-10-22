// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include "signal.h"
#include "unistd.h"
#include <stdio.h>
#include <stdbool.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define BUF_SIZE 256

#define FLAG 0x7E
#define A_T 0x03
#define A_R 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_RR0 0xAA
#define C_RR1 0xAB
#define C_REJ0 0x54 
#define C_REJ1 0x55
#define DISC 0x0B
#define C_0 0X00
#define C_1 0x80
#define ESC 0x7D
#define STUFFING 0x20


typedef enum
{
   START,
   FLAG_RCV,
   A_RCV,
   C_RCV,
   BCC_OK,
   STOP,
} State;

static int sequenceNumber = 0; //armazenar o número da sequencia atual

int alarmEnabled = FALSE;
int alarmCount = 0;
int nRetransmissions = 0;
int timeout = 0;

unsigned char checkResponse(){
    State state = START;
    char byte;
    char response = 0;

    while (state != STOP){

        if (readByte(&byte) != 0)return -1;

        switch (state){
            case START:
                if (byte == FLAG){
                    state = FLAG_RCV;
                }
                break;

            case FLAG_RCV:
                if (byte == A_T){
                    state = A_RCV;
                }
                else if (byte != FLAG){
                    state = START;
                }
                break;

            case A_RCV:
                if (byte == FLAG){
                    state = FLAG_RCV;
                }
                else if (byte == C_REJ0 || byte == C_REJ1 || byte == C_RR0 || byte == C_RR1 || byte == DISC){
                    state = C_RCV;
                    response = byte;
                }
                else{
                    state = START;
                }
                break;

            case C_RCV:
                if (byte == (A_T ^ C_SET)){
                    state = BCC_OK;
                }
                else if (byte == FLAG){
                    state = FLAG_RCV;
                }
                else{
                    state = START;
                }
                break;

            case BCC_OK:
                if (byte == FLAG){
                    state = STOP;
                }
                else {
                    state = START;
                }
                break;

            default:
                return -1;
        }
    }

    return response;
}

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
}
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    State state = START;
    char byte;
    char bytes[5] = {0};
    nRetransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    switch (connectionParameters.role)
    {
    case LlTx:
    
        (void)signal(SIGALRM, alarmHandler);

        while (state != STOP && alarmCount != nRetransmissions){

            bytes[0] = FLAG; bytes[1] = A_T; bytes[2] = C_SET; bytes[3] = A_T ^ C_SET;  bytes[4] = FLAG;  
            if (writeBytes(&bytes, 5) != 0)return -1;

            alarm(timeout);
            alarmEnabled = TRUE;

            while (state != STOP && alarmEnabled == TRUE){
                if (readByte(&byte) != 0)return -1;

                switch (state)
                {
                case START:
                    if (byte == FLAG){
                        state = FLAG_RCV;
                    }
                    break;

                case FLAG_RCV:
                    if (byte == A_R){
                        state = A_RCV;
                    }
                    else if (byte != FLAG){
                        state = START;
                    }
                    break;

                case A_RCV:
                    if (byte == FLAG){
                        state = FLAG_RCV;
                    }
                    else if (byte == C_UA){
                        state = C_RCV;
                    }
                    else{
                        state = START;
                    }
                    break;

                case C_RCV:
                    if (byte == (A_R ^ C_UA)){
                        state = BCC_OK;
                    }
                    else if (byte == FLAG){
                        state = FLAG_RCV;
                    }
                    else{
                        state = START;
                    }
                    break;

                case BCC_OK:
                    if (byte == FLAG){
                        state = STOP;
                    }
                    else {
                        state = START;
                    }
                    break;

                default:
                    return -1;
                }
            }
        }
        alarmCount=0;
        break;
        
    case LlRx:

        while (state != STOP){

            if (readByte(&byte) != 0)return -1;

            switch (state)
            {
            case START:
                if (byte == FLAG){
                    state = FLAG_RCV;
                }
                break;

            case FLAG_RCV:
                if (byte == A_T){
                    state = A_RCV;
                }
                else if (byte != FLAG){
                    state = START;
                }
                break;

            case A_RCV:
                if (byte == FLAG){
                    state = FLAG_RCV;
                }
                else if (byte == C_SET){
                    state = C_RCV;
                }
                else{
                    state = START;
                }
                break;

            case C_RCV:
                if (byte == (A_T ^ C_SET)){
                    state = BCC_OK;
                }
                else if (byte == FLAG){
                    state = FLAG_RCV;
                }
                else{
                    state = START;
                }
                break;

            case BCC_OK:
                if (byte == FLAG){
                    state = STOP;
                }
                else {
                    state = START;
                }
                break;

            default:
                return -1;
            }
        }
        bytes[0] = FLAG; bytes[1] = A_R; bytes[2] = C_UA; bytes[3] = A_R ^ C_UA;  bytes[4] = FLAG;
        if (writeBytes(&bytes, 5) != 0)return -1;
        break;

    default:
        return -1;
    }

    // TODO

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{      
    int size = bufSize + 6;
    unsigned char bcc2 = buf[0];

    if(buf[0] == FLAG || buf[0] == ESC){
        size++;
    }

    for(int i = 1 ; i < bufSize; i++){
        if(buf[i] == FLAG || buf[i] == ESC){
            size++;
        }
        bcc2 ^= buf[i];
    }

    unsigned char *iframe = malloc(size); // (unsigned char *)
    if (iframe == NULL) {
        perror("Erro ao alocar memória para o I-frame");
        return -1;
    }

    iframe[0] = FLAG;
    iframe[1] = A_T;
    iframe[2] = (sequenceNumber == 0) ? C_0 : C_1; 
    iframe[3] = iframe[1] ^ iframe[2];

    int iframeIndex = 4;

    for(int i = 0 ; i < bufSize; i++){
        if (buf[i] == FLAG) {
            iframe[iframeIndex++] = ESC;
            iframe[iframeIndex++] = FLAG ^ STUFFING;  
        } 
        else if (buf[i] == ESC) {
            iframe[iframeIndex++] = ESC;
            iframe[iframeIndex++] = ESC ^ STUFFING;  
        } 
        else {
            iframe[iframeIndex++] = buf[i];  
        }
    }

    iframe[iframeIndex++]= bcc2;
    iframe[iframeIndex++]= FLAG;

    bool accepted;
    bool rejected;
    
    while(alarmCount != nRetransmissions){

        accepted = false;
        alarmEnabled = TRUE;
        alarm(timeout);

        while(alarmEnabled == TRUE && !accepted){

            writeBytes(iframe,size);

            unsigned char response = checkResponse();

            if(response == C_RR0 || response == C_RR1){
                accepted = true;
                sequenceNumber = (sequenceNumber + 1) % 2;
            }
            else if(response == C_REJ0 || response == C_REJ1){
                printf("Rej recebido, a retransmitir o frame...\n");
            }

        }
        if(accepted) break;
        
    }

    alarmCount = 0;
    free(iframe);
    if(accepted){
        return size;
    }

    // TODO

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}

