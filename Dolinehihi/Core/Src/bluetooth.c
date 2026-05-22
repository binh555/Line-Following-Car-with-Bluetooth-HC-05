#include "bluetooth.h"
#include "stm32f4xx_hal.h"
#include "motor.h"
#include <stdint.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

#define BT_RX_BUF_SIZE 64
#define BT_TX_BUF_SIZE 256

static volatile uint8_t btRxBuf[BT_RX_BUF_SIZE];
static volatile uint8_t btRxHead = 0;
static volatile uint8_t btRxTail = 0;
static uint8_t btRxIrqByte = 0;

static volatile uint8_t btTxBuf[BT_TX_BUF_SIZE];
static volatile uint8_t btTxHead = 0;
static volatile uint8_t btTxTail = 0;
static volatile uint8_t btTxBusy = 0;
static uint8_t btTxIrqByte = 0;

static void btTxPushByteNoIrq(uint8_t b) {
    uint8_t next = (btTxHead+1)%BT_TX_BUF_SIZE;
    if(next == btTxTail) return;
    btTxBuf[btTxHead] = b;
    btTxHead = next;
}

static void btTxKickFromMain(void) {
    uint8_t needStart = 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if(!btTxBusy && btTxTail!=btTxHead) {
        btTxBusy = 1;
        btTxIrqByte = btTxBuf[btTxTail];
        btTxTail = (btTxTail+1)%BT_TX_BUF_SIZE;
        needStart = 1;
    }
    if(primask==0) __enable_irq();
    if(needStart) HAL_UART_Transmit_IT(&huart1, &btTxIrqByte,1);
}

static uint8_t btRxPop(uint8_t *out){
    if(btRxTail==btRxHead) return 0;
    *out = btRxBuf[btRxTail];
    btRxTail = (btRxTail+1)%BT_RX_BUF_SIZE;
    return 1;
}

static void btPrint(const char* s){
    while(*s){ btTxPushByteNoIrq((uint8_t)(*s)); s++; }
    btTxKickFromMain();
}

void btPrintHelp(void){
    btPrint("CMD: H=HELP T=STATUS U=AUTO M=MANUAL X=STOP\r\n");
    btPrint("MANUAL: W=FWD S=BWD A=LEFT D=RIGHT\r\n");
}

void bluetoothStartRx(void){
    HAL_UART_Receive_IT(&huart1,&btRxIrqByte,1);
}

void bluetoothTask(void){
    uint8_t c;
    btTxKickFromMain();
    while(btRxPop(&c)){
        if(c>='a' && c<='z') c = c-'a'+'A';
        switch(c){
            case 'H': btPrintHelp(); break;
            case 'W': manualForward(); break;
            case 'S': manualBackward(); break;
            case 'A': manualLeft(); break;
            case 'D': manualRight(); break;
            default: break;
        }
    }
}
