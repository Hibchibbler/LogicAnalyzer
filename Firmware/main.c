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

#define SAW_TRIGGER_MASK        0x00000002 //bit 1 of status reg
#define SAW_PRETRIGGER_MASK     0x00100000 // bit 20 of status reg 11111111111111111111
#define TRIGGER_ADDR_MASK       0x000FFFFC // 19:2 of status reg

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

void uploadMemoryContents(u32 startAdx, u32 adxLength);

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

	union{
		struct {
			//Byte 0
			u8 byte0;

			//Byte 1: Edge Detect Config
			// 3 bit value to indicate channel
			// to look for edge on
			// 1 bit value to indicate rising or falling
			u8 channelNumber : 3;
			u8 riseNotFall   : 1;
			u8 reserved0     : 4;

			// Byte 2 & 3
			// Value triggers for all channels
			// For each channel
			// 10 - Low true trigger
			// 11 - High true trigger
			// 01 - Don't care
			u16 ch0  : 2;
			u16 ch1  : 2;
			u16 ch2  : 2;
			u16 ch3  : 2;
			u16 ch4  : 2;
			u16 ch5  : 2;
			u16 ch6  : 2;
			u16 ch7  : 2;

			// Number of samples pre-trigger
			// total number of samples to save
			// before the trigger
			u32 preTriggerSamples : 18;
			u32 reserved1         : 14;

		}START;
		struct {
			u32	reserved1;
			u32 reserved2;
		}ABORT;
		struct {
			u8	byte0;
			u8	byte1;
			u8	byte2;
			u8	byte3;
			u8  byte4;
			u8  byte5;
			u8  byte6;
			u8  byte7;
		}GENERIC;
		u32 AsU32Lo;
		u32 AsU32Hi;
	};
}CMD_PARAMS, *PCMD_PARAMS;

/* Data */
XGpio	gGpioLed;

volatile u8 gDataBuffer[DATA_SIZE];
volatile u8 gDataBufferIdx=0;

void* StateThread (void *arg);
void* SerialThread(void *arg);
void* MasterThread(void *arg);


u32 buildConfig0(CMD_PARAMS * cmdParams);


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
		sendErrorText ("-E- Xilkernel: Error launching StateThread %d.\r\n");
	}

	ret = pthread_create (&worker[1], &attr, (void*)SerialThread,&args[0]);
	if (ret != 0) {
		sendErrorText ("-E- Xilkernel: Error launching SerialThread %d.\r\n");
	}


	msgId = msgget(MSG_Q_ID, IPC_CREAT);

    while (!masterThreadDone){

    	sleep(1000);
    }
	sendText("-I- MasterThread shutting down\r\n");

	for (i = 0; i < N_THREADS; i++) {
		ret = pthread_join(worker[0], (void*)&result);
	}

    return (void*)0;
}

u32 buildConfig0(CMD_PARAMS * cmdParams) {
	CMD_PARAMS cParams = *cmdParams;
	u32 config0 = 0;
	// config0 = {ch7,ch6,ch5,ch4,ch3,ch2,ch1,ch0,12'd0,riseOrFall,channelNumber}
	// config1 = {14'd0, preTriggerSamplesCount}
	config0 |= cParams.START.channelNumber;
	config0 |= cParams.START.riseNotFall << 3;
	config0 |= cParams.START.ch0 << 16;
	config0 |= cParams.START.ch1 << 18;
	config0 |= cParams.START.ch2 << 20;
	config0 |= cParams.START.ch3 << 22;
	config0 |= cParams.START.ch4 << 24;
	config0 |= cParams.START.ch5 << 26;
	config0 |= cParams.START.ch6 << 28;
	config0 |= cParams.START.ch7 << 30;
	return config0;
}

/* The worker threads */
void* StateThread(void *arg)
{
    u8 state = STATE_INIT;
	CMD_PARAMS cmdParams;
	int ret;
//	u8 wigglePatternIdx		= 0;

	u32 sawTrigger = 0;
	u32 sawPreTrigger = 0;

	u32 triggerAddress;
	int startAddress;

	char numBuff[9];

    while (!stateThreadDone){
    	//Handle Commands from Client
    	ret = msgrcv(msgId, &cmdParams, sizeof(cmdParams), 0, IPC_NOWAIT);
    	if (ret!=-1){
			switch (CMD_FUNC(cmdParams.GENERIC.byte0))
			{
			case CMD_FUNC_START:
				if (state == STATE_IDLE){
					gControlReg = 0x0001;
					writeConfig0(&gLogCapDev,buildConfig0(&cmdParams));
					writeConfig1(&gLogCapDev, cmdParams.START.preTriggerSamples);
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
                //Uncommment if you want GPIO to drive datain
                //
    			//XGpio_DiscreteWrite(&gGpioLed, GPIO_LED_CHANNEL, wigglePatternIdx);
				//wigglePatternIdx++;
				//if (wigglePatternIdx > 255)
				//	wigglePatternIdx=0;
    		}else{
    			state = STATE_UPLOADING;
    		}
    		if ((gStatusReg & SAW_PRETRIGGER_MASK) && !sawPreTrigger) {
    			sawPreTrigger = 1;
    			sendText("-I- Pretrigger samples achieved!\r\n");
    		}
    		if ((gStatusReg & SAW_TRIGGER_MASK) && !sawTrigger) {
    			sawTrigger = 1;
    			sendText("-I- Trigger Detected!\r\n");
    		}
    		break;
    	case STATE_UPLOADING:
    		gStatusReg = readStatus(&gLogCapDev);
    		triggerAddress     = ((gStatusReg & TRIGGER_ADDR_MASK) >> 2);
    		toHexStr(triggerAddress, numBuff);
    		sendText("Trigger Address: 0x");
    		sendText(numBuff);
    		sendText("\r\n");
    		sendText("-I- Uploading...\r\n");
    		if (!sawPreTrigger) {
    			// Not enough samples taken before
    			// trigger to fill up pre trigger contents
    			// so just dump whole buffer
    			sendText("-I- Pre-trigger sample buffer not full\r\n");
    			sendText("-I- Dumping samples from address 0\r\n");
    			uploadMemoryContents(0,262144);
    		} else {
    			// Need to figure out the proper
    			// address to roll through
    			startAddress = (int)triggerAddress - (int)cmdParams.START.preTriggerSamples;
    			if (startAddress < 0) {
    				startAddress = 262143 + startAddress;
    			}
    			uploadMemoryContents(startAddress, 262144);
    		}
    		sendText("Complete!\r\n");
    		state = STATE_INIT;
    		break;
    	default:
    		break;
    	}
    }

    sendText("-I- StateThread shutting down\r\n");
    return NULL;
}

void uploadMemoryContents(u32 startAdx, u32 adxLength) {
	u32 maxAdx = adxLength - 1;
	u8 k;
	u32 addr;
	BRAM_MUXXED_setControlMode(0x1);
	XUartLite_SendByte(STDOUT_BASEADDRESS,BINARY_DATA);
	XUartLite_SendByte(STDOUT_BASEADDRESS,TRACE_DATA);
	for (k = 0; k  < 4; k++) {
		XUartLite_SendByte(STDOUT_BASEADDRESS, (u8) (adxLength >> (3-k)*8) & 0x000000FF);
	}
	for (addr = startAdx; addr <= maxAdx; addr++) {
		XUartLite_SendByte(STDOUT_BASEADDRESS, BRAM_MUXXED_read(addr));
	}
	for (addr = 0; addr < startAdx; addr++) {
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
		cmdParams.GENERIC.byte0 = XUartLite_RecvByte(STDIN_BASEADDRESS);
		cmdParams.GENERIC.byte1 = XUartLite_RecvByte(STDIN_BASEADDRESS);
		cmdParams.GENERIC.byte2 = XUartLite_RecvByte(STDIN_BASEADDRESS);
		cmdParams.GENERIC.byte3 = XUartLite_RecvByte(STDIN_BASEADDRESS);
		cmdParams.GENERIC.byte4 = XUartLite_RecvByte(STDIN_BASEADDRESS);
		cmdParams.GENERIC.byte5 = XUartLite_RecvByte(STDIN_BASEADDRESS);
		cmdParams.GENERIC.byte6 = XUartLite_RecvByte(STDIN_BASEADDRESS);
		cmdParams.GENERIC.byte7 = XUartLite_RecvByte(STDIN_BASEADDRESS);

		//cmdParams.function = CMD_FUNC(cmdParams.GENERIC.byte0);

		switch(CMD_FUNC(cmdParams.GENERIC.byte0)){
		case CMD_FUNC_START:
			sendText("-I- Command Start Received\r\n");//tTrigger=%x\r\n\tClk Channel=%x\r\n\t");
			cmdRxd=1;
			break;
		case CMD_FUNC_ABORT:
			sendText("-I- Command Abort Received\r\n");
			cmdRxd=1;
			break;
		default:
			sendErrorText("-E- Command UNKNOWN Received\r\n");
			break;
		}

		if (cmdRxd==1)
			msgsnd(msgId, &cmdParams, sizeof(cmdParams), 0);
		sleep(500);
	}

	sendText("-I- SerialThread shutting down\r\n");
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
