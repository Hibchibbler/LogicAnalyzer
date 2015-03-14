/******************************************************************************

Interface to memory
Interface to Logic Capture peripheral
Interface to UART

Thread 1
	//Responsible for talking to the Android
	while (!done)
	cmd = readfrom( Android )
	msgsnd (cmd)
Thread 2
	//Responsible for handling messages, and managing state.
	//Each state may respond to commands(from Android) differently
	while(!done)
		cmd = msgrcv()
		switch (state)
			Idle:
				if (cmd) { idleCmdHandler(cmd); }// start? config?
				//Do nothing
				break;
			Collecting:
				if (cmd) { collectCmdHandler(cmd) }; //abort?
				If capture is not busy transition to Uploading
				break;
			Uploading:
				if (cmd) { uploadCmdHandler(cmd) }; //abort?
				Switch memory to internal
				while (!doneReading)
					read value from memory
					write values to android
				break;
*
*-------System.mss BSP settings--------
*BEGIN OS
 PARAMETER OS_NAME = xilkernel
 PARAMETER OS_VER = 6.1
 PARAMETER PROC_INSTANCE = microblaze_0
 PARAMETER config_bufmalloc = true
 PARAMETER config_debug_support = true
 PARAMETER config_msgq = true
 PARAMETER config_sema = true
 PARAMETER config_shm = true
 PARAMETER config_time = true
 PARAMETER mem_table = ((8,20))
 PARAMETER stdin = axi_uartlite_0
 PARAMETER stdout = axi_uartlite_0
 PARAMETER sysintc_spec = axi_intc_0
 PARAMETER systmr_dev = axi_timer_0
END

///Randoms
 * BRAM_MUXXED_init(XPAR_BRAM_MUXXED_0_MUX_BRAM_AXI_BASEADDR);
   BRAM_MUXXED_Reg_SelfTest((void *) XPAR_BRAM_MUXXED_0_MUX_BRAM_AXI_BASEADDR);
   BRAM_MUXXED_setControlMode(0x1);
setControlMode = 0x1 for Microblaze control, 0x0 for peripheral control
******************************************************************************/

/* Includes */
#include "xmk.h"
#include <string.h>
#include "os_config.h"
#include "sys/ksched.h"
#include "sys/init.h"
#include "config/config_param.h"
#include "stdio.h"
#include "xparameters.h"
#include "platform.h"
#include "platform_config.h"
#include <pthread.h>
#include <sys/types.h>
#include <xil_types.h>
#include <xil_io.h>
#include <XUartlite.h>
#include "xuartlite_i.h"
#include <XGpio.h>
#include "BRAM_Muxxed.h"

//#include <MyLogicCapture.h>

// Function codes for
// serial communications protocol
#define TEXT_FUNCTION  0x00
#define BINARY_DATA    0x01
#define ERROR_FUNCTION 0x02
#define DEBUG_FUNCTION 0x03
#define CMD_ACK        0x04

// Subfunction codes
#define TRACE_DATA     0x00 // Subfunction code for trace data from BRAM

/* Declarations */
#define LOGIC_CAPTURE_ID		XPAR_MYLOGICCAPTURE_0_DEVICE_ID
#define LOGIC_CAPTURE_BASEADDR	XPAR_MYLOGICCAPTURE_0_S00_AXI_BASEADDR

#define BRAM_BASEADDR			XPAR_BRAM_MUXXED_0_MUX_BRAM_AXI_BASEADDR

#define GPIO_LED_ID				XPAR_AXI_GPIO_1_DEVICE_ID
#define GPIO_LED_BASEADDR		XPAR_AXI_GPIO_1_BASEADDR
#define GPIO_LED_CHANNEL		1

#define DATA_SIZE   128
#define N_THREADS   2

typedef struct _LOGIC_CAPTURE_DEVICE{
	int id;
	int baseAddr;
}LOGIC_CAPTURE_DEVICE,*PLOGIC_CAPTURE_DEVICE;



void* MasterThread(void *);
void* StateThread(void *);
void* SerialThread(void *);

void toHexStr(u32 num, char * buf);

XStatus initializeHw();
XStatus initializeLogicCapture(PLOGIC_CAPTURE_DEVICE logCapDev, u32 baseAddr, u32 deviceId);

void uploadMemoryContents(u32 lowerAdx, u32 upperAdx);

u32 readStatus(PLOGIC_CAPTURE_DEVICE logCapDev);
void writeControl(PLOGIC_CAPTURE_DEVICE logCapDev, u32 ctl);
void writeConfig0(PLOGIC_CAPTURE_DEVICE logCapDev, u32 cfg0);
void writeConfig1(PLOGIC_CAPTURE_DEVICE logCapDev, u32 cfg1);

void sendSerialPacket(u8 function, u8 subfunction, u32 payloadSize, const u8 *payLoad);
void sendDebugText(const char *c);
void sendText(const char *c);
void sendErrorText(const char *c);
void sendCmdAck(const u8 *payload, u32 payloadSize);
void sendBinaryPayload(const u8 *payload, u32 payloadSize, u8 structure);

LOGIC_CAPTURE_DEVICE gLogCapDev;

volatile u8 masterThreadDone=0;
volatile u8 stateThreadDone=0;
volatile u8 serialThreadDone=0;

volatile u32 gStatusReg		=0;
volatile u32 gControlReg	=0;

const int MSG_Q_ID 		= 1;
volatile int msgId;

#define STATE_INIT			0
#define STATE_SHUTDOWN		1
#define STATE_IDLE			2
#define STATE_COLLECTING	3
#define STATE_UPLOADING		4

#define CMD_FUNC_START		1
#define CMD_FUNC_ABORT		2


#define CMD_FUNC(inByte)	(inByte &  0x7)

typedef struct _CMD_PARAMS{
	u8 function;
	union{
		struct {
			//Byte 0
			u8 byte0;

			//Byte 1
			u8 chanTrigger	: 4;
			u8 reserved1    : 4;

			//Byte 2
			u8	chClk		: 2;
			u8	chEn		: 4;
			u8 reserved2    : 2;

			//Byte 3
			u8 PreBuf		: 8;


		}START;
		struct {
			u32	reserved1;
		}ABORT;
		struct {
			u8	byte0;
			u8	byte1;
			u8	byte2;
			u8	byte3;
		}GENERIC;
		u32 AsU32;
	};
}CMD_PARAMS, *PCMD_PARAMS;

/* Data */
XGpio	gGpioLed;

volatile u8 gDataBuffer[DATA_SIZE];
volatile u8 gDataBufferIdx=0;

void* StateThread (void *arg);
void* SerialThread(void *arg);
void* MasterThread(void *arg);

/* Functions */
int main()
{
    init_platform();

    /* Initialize xilkernel */
    xilkernel_init();

    /* Create the master thread */
    xmk_add_static_thread(MasterThread, 0);

    /* Start the kernel */
    xilkernel_start();

    /* Never reached */
    cleanup_platform();

    return 0;
}

/* The master thread */
void* MasterThread(void *arg)
{
    pthread_t worker[N_THREADS];
    pthread_attr_t attr;

#if SCHED_TYPE == SCHED_PRIO
    struct sched_param spar;
#endif

    int i, ret, *result;
    int args[N_THREADS];



    pthread_attr_init (&attr);

#if SCHED_TYPE == SCHED_PRIO
    spar.sched_priority = PRIO_HIGHEST;
    pthread_attr_setschedparam(&attr, &spar);
#endif

    initializeHw();

    /* This is how we can join with a thread and reclaim its return value */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	ret = pthread_create (&worker[0], &attr, (void*)StateThread,&args[0]);
	if (ret != 0) {
		sendErrorText ("Xilkernel: Error launching StateThread %d.\r\n");
	}

	ret = pthread_create (&worker[1], &attr, (void*)SerialThread,&args[0]);
	if (ret != 0) {
		sendErrorText ("Xilkernel: Error launching SerialThread %d.\r\n");
	}


	msgId = msgget(MSG_Q_ID, IPC_CREAT);

    while (!masterThreadDone){

    	sleep(1000);
    }
	sendText("MasterThread shutting down\r\n");

	for (i = 0; i < N_THREADS; i++) {
		ret = pthread_join(worker[0], (void*)&result);
	}

    return (void*)0;
}

/* The worker threads */
void* StateThread(void *arg)
{
    u8 state = STATE_INIT;
	CMD_PARAMS cmdParams;
	int ret;

    while (!stateThreadDone){
    	//Handle Commands from Client
    	ret = msgrcv(msgId, &cmdParams, sizeof(cmdParams), 0, IPC_NOWAIT);
    	if (ret!=-1){
			switch (cmdParams.function)
			{
			case CMD_FUNC_START:
				if (state == STATE_IDLE){

					gControlReg = 0x0001;
					writeControl(&gLogCapDev, gControlReg);
					sleep(100);
					gControlReg = 0x0000;
					writeControl(&gLogCapDev, gControlReg);

					state = STATE_COLLECTING;
				}

				break;
			case CMD_FUNC_ABORT:
				if (state == STATE_COLLECTING || state == STATE_UPLOADING){

					gControlReg = 0x0010;
					writeControl(&gLogCapDev, gControlReg);
					sleep(100);
					gControlReg = 0x0000;
					writeControl(&gLogCapDev, gControlReg);
					state = STATE_INIT;
				}
				break;
			default:
				break;
			}
		}

    	//Handle State specific stuff
    	switch (state){
    	case STATE_INIT:
    		//Initialize stuff
    		gDataBufferIdx=0;
    		state = STATE_IDLE;
    		break;
    	case STATE_IDLE:
    		//From this state, we can transition into Collecting, or stay and do nothing
    		//it depends on which messages we receive.

    		//Update leds, or whatever
    		break;
    	case STATE_COLLECTING:
    		gStatusReg = readStatus(&gLogCapDev);
    		if (gStatusReg & 0x00000001){

    		}else{
    			state = STATE_UPLOADING;
    		}
    		break;
    	case STATE_UPLOADING:
    		sendText("Uploading\r\n");
    		uploadMemoryContents(0,262143);
    		state = STATE_INIT;
    		break;
    	default:
    		break;
    	}
    	sleep(100);
    }

    sendText("StateThread shutting down\r\n");
    return NULL;
}

//Dump contents of bram to serial from lowerAdx to
//upperAdx inclusive. Also sends the number of
//bytes to be sent over the serial in 4 bytes,
// most significant byte first.
// Contains the logic to send a serial packet
// instead of using the sendSerialPacketFunction
// since the binary data is coming in piecewise
void uploadMemoryContents(u32 lowerAdx, u32 upperAdx) {
	u8 k;
	u32 addr;
	u32 totalByteCount = upperAdx - lowerAdx + 1;
	u32 thisCount;
	XUartLite_SendByte(STDOUT_BASEADDRESS,BINARY_DATA);
	XUartLite_SendByte(STDOUT_BASEADDRESS,TRACE_DATA);
	for (k = 0; k  < 4; k++) {
		thisCount = (totalByteCount >> (3-k)*8) & 0x000000FF;
		XUartLite_SendByte(STDOUT_BASEADDRESS, (u8) thisCount);
	}

	BRAM_MUXXED_setControlMode(0x1);
	for (addr = lowerAdx; addr <= upperAdx; addr++) {
	    XUartLite_SendByte(STDOUT_BASEADDRESS, BRAM_MUXXED_read(addr));
	}
	BRAM_MUXXED_setControlMode(0x0);
}


//Packetize incoming commands from Client
//send them via Message Q, to the StateThread
void* SerialThread(void* arg)
{
	while (!serialThreadDone){
		CMD_PARAMS cmdParams;
		int cmdRxd = 0;

		//Commands from Client are always 4 bytes.
		//cmdParams is a cool structure with a union; it allows for different ways to get at the same data.
		cmdParams.GENERIC.byte0 = XUartLite_RecvByte(STDOUT_BASEADDRESS);
		cmdParams.GENERIC.byte1 = XUartLite_RecvByte(STDOUT_BASEADDRESS);
		cmdParams.GENERIC.byte2 = XUartLite_RecvByte(STDOUT_BASEADDRESS);
		cmdParams.GENERIC.byte3 = XUartLite_RecvByte(STDOUT_BASEADDRESS);

		cmdParams.function = CMD_FUNC(cmdParams.GENERIC.byte0);

		switch(cmdParams.function){
		case CMD_FUNC_START:
			sendDebugText("Command Start Received\r\n");//tTrigger=%x\r\n\tClk Channel=%x\r\n\t");
			//xil_printf("Trigger Pattern=%x\r\n", cmdParams.GENERIC.byte1);
			//xil_printf("Clock Channel=%d\r\n", cmdParams.START.chClk);
			//xil_printf("Enabled Channels:\r\n Ch0 %d\r\n  Ch1 %d\r\n Ch2 %d\r\n Ch3 %d\r\n", cmdParams.START.chEn&0x1,cmdParams.START.chEn&0x2,cmdParams.START.chEn&0x4, cmdParams.START.chEn&0x8);
			//xil_printf("Pre Trigger Buffer=%d\r\n", cmdParams.START.PreBuf);
			cmdRxd=1;
			break;
		case CMD_FUNC_ABORT:
			sendDebugText("Command Abort Received\r\n");
			cmdRxd=1;
			break;
		default:
			sendDebugText("Command UNKNOWN Received\r\n");
			break;
		}

		if (cmdRxd==1)
			msgsnd(msgId, &cmdParams, sizeof(cmdParams), 0);
		sleep(500);
	}

	sendText("SerialThread shutting down\r\n");
	return NULL;
}

XStatus initializeHw()
{
	XStatus status = XST_SUCCESS;
	status = BRAM_MUXXED_init(BRAM_BASEADDR);
	if (status != XST_SUCCESS){
		sendDebugText("Failed to init BRAM_MUXXED peripheral");
		goto EndInitializing;
	}
	status = XGpio_Initialize(&gGpioLed, GPIO_LED_ID);
	if (status != XST_SUCCESS){
		sendDebugText("XGpio_Initialize(GPIO_LED_ID) failed\r\n");
		goto EndInitializing;
	}
	//Output-16bits - Hardware defined;
	XGpio_SetDataDirection(&gGpioLed, GPIO_LED_CHANNEL, 0x0000);
	initializeLogicCapture(&gLogCapDev,LOGIC_CAPTURE_BASEADDR, LOGIC_CAPTURE_ID);
EndInitializing:
	return status;
}

XStatus initializeLogicCapture(PLOGIC_CAPTURE_DEVICE logCapDev, u32 baseAddr, u32 deviceId)
{
	XStatus status = XST_SUCCESS;
	gControlReg = 0;
	logCapDev->id = deviceId;
	logCapDev->baseAddr = baseAddr;
	writeControl(logCapDev, 0x00000000);
	writeConfig0(logCapDev, 0x00000000);
	writeConfig1(logCapDev, 0x00000000);
	return status;
}

u32 readStatus(PLOGIC_CAPTURE_DEVICE logCapDev){
	//return MYLOGICCAPTURE_mReadReg(logCapDev->baseAddr+0, 0);
	return *((u32*)(logCapDev->baseAddr+0));
}
void writeControl(PLOGIC_CAPTURE_DEVICE logCapDev, u32 ctl){
	//MYLOGICCAPTURE_mWriteReg(logCapDev->baseAddr+4, 0, ctl);
	*((u32*)(logCapDev->baseAddr+4)) = ctl;
}

void writeConfig0(PLOGIC_CAPTURE_DEVICE logCapDev, u32 cfg0){
	//MYLOGICCAPTURE_mWriteReg(logCapDev->baseAddr+8, 0, cfg0);
	*((u32*)(logCapDev->baseAddr+8)) = cfg0;
}

void writeConfig1(PLOGIC_CAPTURE_DEVICE logCapDev, u32 cfg1){
	//MYLOGICCAPTURE_mWriteReg(logCapDev->baseAddr+12, 0, cfg1);
	*((u32*)(logCapDev->baseAddr+12)) = cfg1;
}

void sendBinaryPayload(const u8 *payload, u32 payloadSize, u8 structure) {
	sendSerialPacket(BINARY_DATA, structure, payloadSize, payload);
}

// Send a command acknowledgment to client software
void sendCmdAck(const u8 *payload, u32 payloadSize) {
	sendSerialPacket(CMD_ACK, 0x00, payloadSize, payload);
}

// write to serial with text function code
// 1 to indicate debug text
void sendDebugText(const char *c) {
	u32 byteSize = strlen(c);
	sendSerialPacket(DEBUG_FUNCTION,0x00,byteSize, (u8 *) c);
}

// write to serial with function code
// 0 to indicate normal text
void sendText(const char *c) {
	u32 byteSize = strlen(c);
	sendSerialPacket(TEXT_FUNCTION,0x00,byteSize, (u8 *) c);
}

// write to serial with function code
// 0 to indicate normal text
void sendErrorText(const char *c) {
	u32 byteSize = strlen(c);
	sendSerialPacket(ERROR_FUNCTION,0x00,byteSize, (u8 *) c);
}

// Sends all the components of a serial
// command to over serial.
// Function - 1 byte- 0 Text, 1 Debug Text, 2 Binary Data, 3 Command Ack
// Subfunction - 1 byte - used only to indicate binary data structure, ignored otherwise
// Payload size - 4 bytes - Number of bytes of payload, most sigificant byte first
// Payload - "Payload size" bytes of data
void sendSerialPacket(u8 function, u8 subfunction, u32 payloadSize, const u8 *payLoad) {
	u32 k;
	XUartLite_SendByte(STDOUT_BASEADDRESS, function);
	XUartLite_SendByte(STDOUT_BASEADDRESS, subfunction);
	// Deliver payload size
	for (k = 0; k  < 4; k++) {
		XUartLite_SendByte(STDOUT_BASEADDRESS, (u8) (payloadSize >> (3-k)*8) & 0x000000FF);
	}
	// Now transfer payloadSize bytes
	//Note: Assumes payLoad size is at least payloadSize bytes long
	for (k = 0; k < payloadSize; k++) {
		XUartLite_SendByte(STDOUT_BASEADDRESS, payLoad[ k ]);
	}
}

// Debug function to convert a
// u32 value to a hex string
// assumes "buf" is a size of 9
void toHexStr(u32 num, char * buf) {
	  int   cnt;
	  char  *ptr;
	  int   digit;
	  ptr = buf;
	  for (cnt = 7 ; cnt >= 0 ; cnt--) {
	    digit = (num >> (cnt * 4)) & 0xf;
	    if (digit <= 9)
	      *ptr++ = (char) ('0' + digit);
	    else
	      *ptr++ = (char) ('a' - 10 + digit);
	  }
}
