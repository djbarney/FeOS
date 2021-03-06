#include "feos.h"
#include "feosfifo.h"
#include <stdio.h>

void FeOS_IRQPoll();

//-----------------------------------------------------------------------------
// User-mode FIFO handler support, to be moved to a separate file
//-----------------------------------------------------------------------------

static void* userdata_array[FIFO_PROG_CH_NUM];
static FifoDatamsgHandlerFunc datamsghnd_array[FIFO_PROG_CH_NUM];
static FifoValue32HandlerFunc value32hnd_array[FIFO_PROG_CH_NUM];
static FifoAddressHandlerFunc addresshnd_array[FIFO_PROG_CH_NUM];

enum
{
	QUEUE_DATAMSG, QUEUE_VALUE32, QUEUE_ADDRESS
};

typedef struct tagFifoMsgQueueEntry
{
	struct tagFifoMsgQueueEntry* next;
	u16 type, channel;
	//void* userdata;
	union
	{
		int datamsgSize;
		word_t value32;
		void* address;
	};
} FifoMsgQueueEntry;

// Ersatz memory allocator

#define FIFOMSGSTACK_SIZE (4*1024)

static byte_t fifoMsgStack[FIFOMSGSTACK_SIZE];
static int fifoMsgStackPos = FIFOMSGSTACK_SIZE;

static FifoMsgQueueEntry* FeOS_AllocFifoMsg()
{
	FifoMsgQueueEntry* ret = NULL;
	int cs = enterCriticalSection();
	int newpos = fifoMsgStackPos - sizeof(FifoMsgQueueEntry);
	if (newpos >= 0)
		ret = (FifoMsgQueueEntry*)(fifoMsgStack + (fifoMsgStackPos = newpos));
	leaveCriticalSection(cs);
	return ret;
}

// User-mode FIFO message queue data structure. This must be done in
// assembly because a STRD instruction is needed to set both fields
// at the same time.

asm("\
.text\n\
	.align 3\n\
	.global __clearFifoMsgQueue\n\
__clearFifoMsgQueue:\n\
	mov r0, #0\n\
	mov r1, #0\n\
	strd r0, [pc]\n\
	bx lr\n\
	.global __fifoMsgQueue\n\
__fifoMsgQueue:\n\
	.word 0\n\
	.global __fifoMsgQueueTail\n\
__fifoMsgQueueTail:\n\
	.word 0");

#define fifoMsgQueue __fifoMsgQueue
#define fifoMsgQueueTail __fifoMsgQueueTail

extern FifoMsgQueueEntry* __fifoMsgQueue;
extern FifoMsgQueueEntry* __fifoMsgQueueTail;

void __clearFifoMsgQueue();

static void FeOS_AddFifoMsgToQueue(FifoMsgQueueEntry* msg)
{
	int cs = enterCriticalSection();

	msg->next = NULL;

	if (fifoMsgQueueTail)
		fifoMsgQueueTail->next = msg;
	else
		fifoMsgQueue = msg;

	fifoMsgQueueTail = msg;

	leaveCriticalSection(cs);
}

int FeOS_swi_FifoGetDatamsg(int, word_t, void*);

void FeOS_RunFifoQueue()
{
	FifoMsgQueueEntry* ent;

	for(ent = fifoMsgQueue; ent; ent = ent->next)
	{
		int chn = ent->channel - FIFO_PROG_CH;
		void* userdata = userdata_array[chn];
		switch(ent->type)
		{
			case QUEUE_DATAMSG:
				if (datamsghnd_array[chn])
					datamsghnd_array[chn](ent->datamsgSize, userdata);
				else
					FeOS_swi_FifoGetDatamsg(ent->channel, 0, NULL); // delete the data msg
				break;
			case QUEUE_VALUE32:
				if (value32hnd_array[chn])
					value32hnd_array[chn](ent->value32, userdata);
				break;
			case QUEUE_ADDRESS:
				if (addresshnd_array[chn])
					addresshnd_array[chn](ent->address, userdata);
				break;
		}
	}

	__clearFifoMsgQueue();
	fifoMsgStackPos = FIFOMSGSTACK_SIZE;
}

static void StubFifoDatamsgHandler(int datamsgSize, void* userdata)
{
	FifoMsgQueueEntry* msg = FeOS_AllocFifoMsg();
	FeOS_AddFifoMsgToQueue(msg);
	msg->type = QUEUE_DATAMSG;
	msg->channel = (u16) (word_t) userdata;
	msg->datamsgSize = datamsgSize;
}

static void StubFifoValue32Handler(word_t value32, void* userdata)
{
	FifoMsgQueueEntry* msg = FeOS_AllocFifoMsg();
	FeOS_AddFifoMsgToQueue(msg);
	msg->type = QUEUE_VALUE32;
	msg->channel = (u16) (word_t) userdata;
	msg->value32 = value32;
}

static void StubFifoAddressHandler(void* address, void* userdata)
{
	FifoMsgQueueEntry* msg = FeOS_AllocFifoMsg();
	FeOS_AddFifoMsgToQueue(msg);
	msg->type = QUEUE_ADDRESS;
	msg->channel = (u16) (word_t) userdata;
	msg->address = address;
}

void _SetDatamsgHandler(int channel, bool set)
{
	if (set)
		fifoSetDatamsgHandler(channel, StubFifoDatamsgHandler, (void*) channel);
	else
		fifoSetDatamsgHandler(channel, NULL, NULL);
}

void _SetValue32Handler(int channel, bool set)
{
	if (set)
		fifoSetValue32Handler(channel, StubFifoValue32Handler, (void*) channel);
	else
		fifoSetValue32Handler(channel, NULL, NULL);
}

void _SetAddressHandler(int channel, bool set)
{
	if (set)
		fifoSetAddressHandler(channel, StubFifoAddressHandler, (void*) channel);
	else
		fifoSetAddressHandler(channel, NULL, NULL);
}

void FeOS_FifoSetDatamsgHandler(int channel, FifoDatamsgHandlerFunc handler, void* userdata)
{
	int ch = channel - FIFO_PROG_CH;
	userdata_array[ch] = userdata;
	datamsghnd_array[ch] = handler;
	FeOS_swi_SetDatamsgHandler(channel, !!handler);
	FeOS_RunFifoQueue();
}

void FeOS_FifoSetValue32Handler(int channel, FifoValue32HandlerFunc handler, void* userdata)
{
	int ch = channel - FIFO_PROG_CH;
	userdata_array[ch] = userdata;
	value32hnd_array[ch] = handler;
	FeOS_swi_SetValue32Handler(channel, !!handler);
	FeOS_RunFifoQueue();
}

void FeOS_FifoSetAddressHandler(int channel, FifoAddressHandlerFunc handler, void* userdata)
{
	int ch = channel - FIFO_PROG_CH;
	userdata_array[ch] = userdata;
	addresshnd_array[ch] = handler;
	FeOS_swi_SetAddressHandler(channel, !!handler);
	FeOS_RunFifoQueue();
}

//-----------------------------------------------------------------------------
// User mode IRQ support
//     Based on libnds code
//-----------------------------------------------------------------------------

#define MAX_IRQS 16
#define GET_IRQ ((VoidFn)0xFFFFFFFF)

static struct IntTable irqTable[MAX_IRQS];

VoidFn FeOS_SetInterrupt(word_t mask, VoidFn fn)
{
	if (!mask) return NULL;

	int i;
	for (i = 0; i < MAX_IRQS; i ++)
		if (!irqTable[i].mask || irqTable[i].mask == mask) break;

	if (i == MAX_IRQS) return NULL;

	VoidFn oldFn = irqTable[i].handler;

	if (fn != GET_IRQ)
	{
		// For IRQ-safety reasons, the registering MUST be done in this order
		irqTable[i].handler = fn;
		irqTable[i].mask = mask;
	}

	return oldFn;
}

static void FeOS_ProcessIRQ(word_t flags)
{
	// Check for FIFO recv IRQ before everything else
	if (flags & IRQ_FIFO_NOT_EMPTY)
	{
		FeOS_RunFifoQueue();

		// We don't want user code responding to this IRQ
		flags &= ~IRQ_FIFO_NOT_EMPTY;
	}

	int i; word_t mask; VoidFn fn;
	for (i = 0; i < MAX_IRQS && (mask = irqTable[i].mask); i ++)
		if ((mask & flags) && (fn = irqTable[i].handler))
		{
			fn();
			break;
		}
}

word_t FeOS_CheckPendingIRQs()
{
	word_t totalflags = 0;
	for(;;) // IRQ/FIFO might fire while the handlers are executing
	{
		word_t flags = __ARMSWP(0, &INTR_WAIT_FLAGS);
		if (flags)
			FeOS_ProcessIRQ(flags);
		else
			break;
		totalflags |= flags;
	}
	return totalflags;
};

static void _FeOS_WaitForIRQ(word_t mask)
{
	FeOS_CheckPendingIRQs();
	for(;;)
	{
		FeOS_IRQPoll();
		if (FeOS_CheckPendingIRQs() & mask)
			break;
	}
}

static irqWaitFunc_t irqWaitFunc = NULL;

void FeOS_WaitForIRQ(word_t mask)
{
	(irqWaitFunc ? irqWaitFunc : &_FeOS_WaitForIRQ)(mask);
}

irqWaitFunc_t FeOS_SetIRQWaitFunc(irqWaitFunc_t newFunc)
{
	irqWaitFunc_t oldFunc = irqWaitFunc;
	if (newFunc != GET_IRQFUNC)
		irqWaitFunc = newFunc;
	return oldFunc;
}

word_t FeOS_NextIRQ()
{
	word_t flags = 0;
	for(;;)
	{
		flags = FeOS_CheckPendingIRQs();
		if (flags == 0)
			FeOS_IRQPoll(); // wait for *any* IRQs to happen
		else
			break;
	}
	return flags;
}
