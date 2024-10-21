// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

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
#define REJ0 0x54 
#define REJ1 0x55
#define DISC 0x0B

typedef enum
{
   START,
   FLAG_RCV,
   A_RCV,
   C_RCV,
   BCC_OK,
   STOP,
} State;

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
    int byte;
    char bytes[5];

    switch (connectionParameters.role)
    {
    case LlTx:
        
        bytes[0] = FLAG; bytes[1] = A_T; bytes[2] = C_SET; bytes[3] = A_T ^ C_SET;  bytes[4] = FLAG;
        if (writeBytes(&bytes, 5) != 0)return -1;

        while (state != STOP){

            if (readByte(&byte) != 0)return -1;

            switch (byte)
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
        break;
        
    case LlRx:

        while (state != STOP){

            if (readByte(&byte) != 0)return -1;

            switch (byte)
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
                break;
            }
        }
        bytes[0] = FLAG; bytes[1] = A_R; bytes[2] = C_UA; bytes[3] = A_R ^ C_UA;  bytes[4] = FLAG;
        if (writeBytes(&bytes, 5) != 0)return -1;
        break;

    default:
        break;
    }

    // TODO

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
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
