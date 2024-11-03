// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include "signal.h"
#include "unistd.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

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
   DECODING,
   STOP,
} State;

struct Statistics
{
    int totalTimeOuts;
    int totalRetrasmissions;
    int totalFrames;
};


static int sequenceNumber = 0; //armazenar o número da sequencia atual

int discReceived = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int nRetransmissions = 0;
int timeout = 0;
LinkLayerRole role;

struct Statistics status = {0,0,0};


unsigned char checkResponse(){
    State state = START;
    unsigned char byte;
    unsigned char response = 0;
    
    while (state != STOP && alarmEnabled == 1){

        if (readByte(&byte) > 0) {
           
        switch (state) {
            case START:
                if (byte == FLAG) {
                    state = FLAG_RCV;
                }
                break;

            case FLAG_RCV:
                if (byte == A_T) {
                    state = A_RCV;
                } else if (byte != FLAG) {
                    state = START;
                }
                break;

            case A_RCV:
                if (byte == FLAG) {
                } else if (byte == C_REJ0 || byte == C_REJ1 || byte == C_RR0 || byte == C_RR1 || byte == DISC) {
                    state = C_RCV;
                    response = byte;
                } else {
                    state = START;
                }
                break;

            case C_RCV:
                if (byte == (A_T ^ response)) {
                    state = BCC_OK;
                } else if (byte == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;

            case BCC_OK:
                if (byte == FLAG) {
                    state = STOP;
                } else {
                    state = START;
                }
                break;

            default:
                return -1;
        }}
    }

    return response;
}

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    status.totalTimeOuts++;
}
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0) {
        printf("Erro ao abrir a porta serial.\n");
        return -1;
    }

    (void)signal(SIGALRM, alarmHandler);

    State state = START;
    unsigned char byte;
    unsigned char bytes[5] = {0};
    nRetransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    role = connectionParameters.role;


    switch (connectionParameters.role) {
    case LlTx:
        
        while (state != STOP && alarmCount != nRetransmissions) {

            // Preparar e enviar pacote SET
            bytes[0] = FLAG; bytes[1] = A_T; bytes[2] = C_SET; bytes[3] = bytes[1] ^ bytes[2]; bytes[4] = FLAG;
            status.totalFrames++;

            if (writeBytes(bytes, 5) < 0) {
                printf("Erro ao enviar pacote SET.\n");
                return -1;
            }

            alarm(timeout);
            alarmEnabled = 1;

            while (state != STOP && alarmEnabled == 1) {
                if (readByte(&byte) > 0) {

                switch (state) {
                case START:
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    }
                    break;

                case FLAG_RCV:
                    if (byte == A_R) {
                        state = A_RCV;
                    } else if (byte != FLAG) {
                        state = START;
                    }
                    break;

                case A_RCV:
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else if (byte == C_UA) {
                        state = C_RCV;
                    } else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    if (byte == (A_R ^ C_UA)) {
                        state = BCC_OK;
                    } else if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else {
                        state = START;
                    }
                    break;

                case BCC_OK:
                    if (byte == FLAG) {
                        state = STOP;
                    } else {
                        state = START;
                    }
                    break;

                default:
                    return -1;
                }
            }}
        }
        alarmCount = 0;
        if(state!= STOP) return -1;
        break;
        
    case LlRx:

        while (state != STOP) {
            if (readByte(&byte) > 0) {

            switch (state) {
            case START:
                if (byte == FLAG) {
                    state = FLAG_RCV;
                }
                break;

            case FLAG_RCV:
                if (byte == A_T) {
                    state = A_RCV;
                } else if (byte != FLAG) {
                    state = START;
                }
                break;

            case A_RCV:
                if (byte == FLAG) {
                    state = FLAG_RCV;
                } else if (byte == C_SET) {
                    state = C_RCV;
                } else {
                    state = START;
                }
                break;

            case C_RCV:
                if (byte == (A_T ^ C_SET)) {
                    state = BCC_OK;
                } else if (byte == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;

            case BCC_OK:
                if (byte == FLAG) {
                    state = STOP;
                } else {
                    state = START;
                }
                break;

            default:
                return -1;
            }}
        }
        
        // Preparar e enviar pacote UA
        bytes[0] = FLAG; bytes[1] = A_R; bytes[2] = C_UA; bytes[3] = bytes[1] ^ bytes[2]; bytes[4] = FLAG;
        status.totalFrames++;
        
        if (writeBytes(bytes, 5) < 0) {
            printf("Erro ao enviar pacote UA.\n");
            return -1;
        }
        break;

    default:
        return -1;
    }

    return 1;
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {      
    int size = bufSize + 6;
    unsigned char bcc2 = 0;

    // Calcular BCC2 e ajustar tamanho para stuffing
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            size++;
        }
        bcc2 ^= buf[i];
    }

    // Alocar memória para o I-frame
    unsigned char *iframe = malloc(size);
    if (iframe == NULL) {
        perror("Erro ao alocar memória para o I-frame");
        return -1;
    }

    // Construir o I-frame
    iframe[0] = FLAG;
    iframe[1] = A_T;
    iframe[2] = (sequenceNumber == 0) ? C_0 : C_1;
    iframe[3] = iframe[1] ^ iframe[2];

    int iframeIndex = 4;

    // Preenchimento com stuffing
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG) {
            iframe[iframeIndex++] = ESC;
            iframe[iframeIndex++] = FLAG ^ STUFFING;  
        } else if (buf[i] == ESC) {
            iframe[iframeIndex++] = ESC;
            iframe[iframeIndex++] = ESC ^ STUFFING;  
        } else {
            iframe[iframeIndex++] = buf[i];  
        }
    }

    iframe[iframeIndex++] = bcc2;
    iframe[iframeIndex++] = FLAG;

    bool accepted = false;
    
    while (alarmCount != nRetransmissions) {
        accepted = false;
        alarmEnabled = 1;
        alarm(timeout);
        while (alarmEnabled == 1 && !accepted) {

            // Enviar I-frame
            status.totalFrames++;

            if (writeBytes(iframe, size) != size) {
                printf("Erro ao enviar I-frame.\n");
                free(iframe);
                return -1;
            }

            unsigned char response = checkResponse();

            if (response == C_RR0 || response == C_RR1) {
                accepted = true;
                sequenceNumber = (sequenceNumber + 1) % 2;
            } else if (response == C_REJ0 || response == C_REJ1) {
            }
        }

        if (accepted) break;
    }

    alarmCount = 0;
    free(iframe);

    if (accepted) {
        return size;
    }

    printf("Falha ao enviar o I-frame após várias tentativas. Fechando conexão...\n");
    llclose(0);

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    State state = START;
    unsigned char byte, cbyte;
    int i = 0;
    unsigned char bcc2 = 0;
    unsigned char bytes[5];
    unsigned char discframe[5];

    while (state != STOP) {
        if (readByte(&byte) > 0) {

            switch (state) {
                case START:
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    }
                    break;

                case FLAG_RCV:
                    if (byte == A_T) {
                        state = A_RCV;
                    } else if (byte != FLAG) {
                        state = START;
                    }
                    break;

                case A_RCV:
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else if (byte == C_0 || byte == C_1) {
                        cbyte = byte;
                        state = C_RCV;
                    } else if (byte == DISC){
                        discReceived = TRUE;
                        llclose(1);
                        return 0;
                    } else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    if (byte == (A_T ^ cbyte)) {
                        state = DECODING;
                    } else if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else {
                        state = START;
                    }
                    break;

                case DECODING:
                    if (byte == ESC) {
                        while (true){
                            if (readByte(&byte) > 0) {
                        
                                byte = byte ^ STUFFING;
                                packet[i++] = byte;
                                break;
                            }
                        }
                    } else if (byte == FLAG) {
                        // Verificar BCC2
                        unsigned char bcc2_read = packet[i - 1];
                        packet[--i] = '\0';

                        for (int k = 0; k < i; k++){
                            bcc2 ^= packet[k];
                        }

                        if (bcc2 != bcc2_read) {
                            printf("Erro: BCC2 inválido! Esperado: 0x%02X, Recebido: 0x%02X\n", bcc2, bcc2_read);

                            // Enviar resposta REJ
                            bytes[0] = FLAG; bytes[1] = A_T; bytes[2] = (sequenceNumber == 0) ? C_REJ0 : C_REJ1; bytes[3] = bytes[1] ^ bytes[2]; bytes[4] = FLAG;
                            status.totalFrames++;

                            if (writeBytes(bytes, 5) < 0) {
                                printf("Erro ao enviar REJ.\n");
                                return -1;
                            }
                            return -1;
                        }

                        // BCC2 válido, finalizar leitura
                        state = STOP;

                        // Enviar resposta RR
                        bytes[0] = FLAG; bytes[1] = A_T; bytes[2] = (sequenceNumber == 0) ? C_RR0 : C_RR1; bytes[3] = bytes[1] ^ bytes[2]; bytes[4] = FLAG;
                        status.totalFrames++;

                        if (writeBytes(bytes, 5) < 0) {
                            printf("Erro ao enviar RR.\n");
                            return -1;
                        }
                        sequenceNumber = (sequenceNumber + 1) % 2;

                        return i;
                    }

                    else{ 
                        // Armazenar byte e calcular BCC2
                        packet[i++] = byte;
                    }

                    break;

                default:
                    return -1;
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    State state = START;
    unsigned char byte;
    unsigned char bytes[5] = {0};

    switch (role)
    {
    case LlTx:
        while (alarmCount != nRetransmissions && state != STOP) {
            // Preparar o quadro DISC
            bytes[0] = FLAG; bytes[1] = A_T; bytes[2] = DISC; bytes[3] = bytes[1] ^ bytes[2]; bytes[4] = FLAG;
            status.totalFrames++;

            if (writeBytes(bytes, 5) < 0) {
                printf("Erro ao enviar DISC.\n");
                return -1;
            }

            alarm(timeout);
            alarmEnabled = 1;

            while (alarmEnabled == 1 && state != STOP) {
                if (readByte(&byte) > 0) {

                    switch (state) {
                        case START:
                            if (byte == FLAG) {
                                state = FLAG_RCV;
                            }
                            break;
                        case FLAG_RCV:
                            if (byte == A_R) {
                                state = A_RCV;
                            } else if (byte != FLAG) {
                                state = START;
                            }
                            break;
                        case A_RCV:
                            if (byte == DISC) {
                                state = C_RCV;
                            } else if (byte == FLAG) {
                                state = FLAG_RCV;
                            } else {
                                state = START;
                            }
                            break;
                        case C_RCV:
                            if (byte == (A_R ^ DISC)) {
                                state = BCC_OK;
                            } else if (byte == FLAG) {
                                state = FLAG_RCV;
                            } else {
                                state = START;
                            }
                            break;
                        case BCC_OK:
                            if (byte == FLAG) {
                                state = STOP;
                            } else {
                                state = START;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        bytes[0] = FLAG; bytes[1] = A_R; bytes[2] = C_UA; bytes[3] = bytes[1] ^ bytes[2]; bytes[4] = FLAG;
        status.totalFrames++;

        if (writeBytes(bytes, 5) < 0) {
                printf("Erro ao enviar C_UA.\n");
                return -1;
            }
        break;

    case LlRx:
        if (discReceived == TRUE){

        while (state != STOP) {
                if (readByte(&byte) > 0) {

                    switch (state) {
                        case START:
                            if (byte == FLAG) {
                                state = FLAG_RCV;
                            }
                            break;
                        case FLAG_RCV:
                            if (byte == A_T) {
                                state = A_RCV;
                            } else if (byte != FLAG) {
                                state = START;
                            }
                            break;
                        case A_RCV:
                            if (byte == DISC) {
                                state = C_RCV;
                            } else if (byte == FLAG) {
                                state = FLAG_RCV;
                            } else {
                                state = START;
                            }
                            break;
                        case C_RCV:
                            if (byte == (A_T ^ DISC)) {
                                state = BCC_OK;
                            } else if (byte == FLAG) {
                                state = FLAG_RCV;
                            } else {
                                state = START;
                            }
                            break;
                        case BCC_OK:
                            if (byte == FLAG) {
                                state = STOP;
                            } else {
                                state = START;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
            
            while (state != STOP && alarmCount != nRetransmissions) {

            bytes[0] = FLAG; bytes[1] = A_R; bytes[2] = DISC; bytes[3] = bytes[1] ^ bytes[2]; bytes[4] = FLAG;
            status.totalFrames++;

            if (writeBytes(bytes, 5) < 0) {
                printf("Erro ao enviar DISC.\n");
                return -1;
            }

            alarm(timeout);
            alarmEnabled = 1;

            while (state != STOP && alarmEnabled == 1) {
                if (readByte(&byte) > 0) {   

                switch (state) {
                case START:
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    }
                    break;

                case FLAG_RCV:
                    if (byte == A_R) {
                        state = A_RCV;
                    } else if (byte != FLAG) {
                        state = START;
                    }
                    break;

                case A_RCV:
                    if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else if (byte == C_UA) {
                        state = C_RCV;
                    } else {
                        state = START;
                    }
                    break;

                case C_RCV:
                    if (byte == (A_R ^ C_UA)) {
                        state = BCC_OK;
                    } else if (byte == FLAG) {
                        state = FLAG_RCV;
                    } else {
                        state = START;
                    }
                    break;

                case BCC_OK:
                    if (byte == FLAG) {
                        state = STOP;
                    } else {
                        state = START;
                    }
                    break;

                default:
                    return -1;
                }
            }}
        }
        alarmCount = 0;

            break;

    default:
        return -1;
    }
    sleep(1);
    printf("\n\n-Estatísticas:\n");
    printf("  timeouts = %d\n  frames enviados = %d\n  tempo de execução = %d s\n\n", status.totalTimeOuts, status.totalFrames, showStatistics);
    printf("Fechando a porta serial...\n");

    int clstat = closeSerialPort();
    printf("Porta serial fechada com status: %d\n", clstat);
    
    return clstat;
}

