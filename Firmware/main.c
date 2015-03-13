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
#include "BRAM_Muxxed.h"

//#include <MyLogicCapture.h>

/* Declarations */
#define LOGIC_CAPTURE_ID		XPAR_MYLOGICCAPTURE_0_DEVICE_ID
#define LOGIC_CAPTURE_BASEADDR	XPAR_MYLOGICCAPTURE_0_S00_AXI_BASEADDR

#define BRAM_BASEADDR			XPAR_BRAM_0_BASEADDR

#define DATA_SIZE   128
#define N_THREADS   2

typedef struct _LOGIC_CAPTURE_DEVICE{
	int id;
	int baseAddr;
}LOGIC_CAPTURE_DEVICE,*PLOGIC_CAPTURE_DEVICE;



void* MasterThread(void *);
void* StateThread(void *);
void* SerialThread(void *);

void initializeHw();
void initializeLogicCapture(PLOGIC_CAPTURE_DEVICE logCapDev, u32 baseAddr, u32 deviceId);

u32 readStatus(PLOGIC_CAPTURE_DEVICE logCapDev);
void writeControl(PLOGIC_CAPTURE_DEVICE logCapDev, u32 ctl);
void writeConfig0(PLOGIC_CAPTURE_DEVICE logCapDev, u32 cfg0);
void writeConfig1(PLOGIC_CAPTURE_DEVICE logCapDev, u32 cfg1);

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
		xil_printf ("Xilkernel: ERROR (%d) launching StateThread %d.\r\n", ret, 0);
	}

	ret = pthread_create (&worker[1], &attr, (void*)SerialThread,&args[0]);
	if (ret != 0) {
		xil_printf ("Xilkernel: ERROR (%d) launching SerialThread %d.\r\n", ret, 0);
	}


	msgId = msgget(MSG_Q_ID, IPC_CREAT);

    while (!masterThreadDone){

    	sleep(1000);
    }
	xil_printf("MasterThread shutting down\r\n");

	for (i = 0; i < N_THREADS; i++) {
		ret = pthread_join(worker[0], (void*)&result);
	}

    return (void*)0;
}



/* The worker threads */
void* StateThread(void *arg)
{
    u8 psum;
    u8 state = STATE_INIT;

    psum = 0;
	CMD_PARAMS cmdParams;
	int ret;

    while (!stateThreadDone){
    	xil_printf("%d\r\n", psum++);


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
    			//LogCapDev is busy..
    			//It is still collecting..
    			xil_printf("Logic Capture Busy\r\n");
    			sleep(1000);
    		}else{

    			state = STATE_UPLOADING;
    		}
    		break;
    	case STATE_UPLOADING:
    		xil_printf("Uploading\r\n");

    		//Switch Memory for MicroBlaze to use
    		//While !done
    		//	val = Read(Memory)
    		//  WriteUart(val)
    		//Switch Memory for LogicCap to use

    		state = STATE_INIT;
    		break;
    	default:
    		break;
    	}
    	sleep(100);
    }

    xil_printf("StateThread shutting down\r\n");
    return NULL;
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
			xil_printf("Command Start Received\r\n");//tTrigger=%x\r\n\tClk Channel=%x\r\n\t");
			xil_printf("Trigger Pattern=%x\r\n", cmdParams.GENERIC.byte1);
			xil_printf("Clock Channel=%d\r\n", cmdParams.START.chClk);
			xil_printf("Enabled Channels:\r\n Ch0 %d\r\n  Ch1 %d\r\n Ch2 %d\r\n Ch3 %d\r\n", cmdParams.START.chEn&0x1,cmdParams.START.chEn&0x2,cmdParams.START.chEn&0x4, cmdParams.START.chEn&0x8);
			xil_printf("Pre Trigger Buffer=%d\r\n", cmdParams.START.PreBuf);
			cmdRxd=1;
			break;
		case CMD_FUNC_ABORT:
			xil_printf("Command Abort Received\r\n");
			cmdRxd=1;
			break;
		default:
			xil_printf("Command UNKNOWN Received\r\n");
			break;
		}

		if (cmdRxd==1)
			msgsnd(msgId, &cmdParams, sizeof(cmdParams), 0);
		sleep(500);
	}

	xil_printf("SerialThread shutting down\r\n");
	return NULL;
}

void initializeHw()
{
	BRAM_MUXXED_init(BRAM_BASEADDR);
	initializeLogicCapture(&gLogCapDev,LOGIC_CAPTURE_BASEADDR, LOGIC_CAPTURE_ID);
}

void initializeLogicCapture(PLOGIC_CAPTURE_DEVICE logCapDev, u32 baseAddr, u32 deviceId)
{
	gControlReg = 0;
	logCapDev->id = deviceId;
	logCapDev->baseAddr = baseAddr;
	writeControl(logCapDev, 0x00000000);
	writeConfig0(logCapDev, 0x00000000);
	writeConfig1(logCapDev, 0x00000000);
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
