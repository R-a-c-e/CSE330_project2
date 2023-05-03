// Project 2, Group 14

// Include module and kernel
#include <linux/module.h>
#include <linux/kernel.h>
// Needed for working with kthreads
#include <linux/kthread.h>
// Needed for for_each_process function
#include <linux/sched.h>
#include <linux/sched/signal.h>
// Needed for semaphores
#include <linux/semaphore.h>

MODULE_LICENSE("GPL");

#define MAX_BUFFER_SIZE 1000
#define MAX_CONSUMERS 100

// Input parameters
static int uid = 1000;
static int buff_size = 10;
static int p = 1;
static int c = 0;
module_param(uid, int, 0);
module_param(buff_size, int, 0);
module_param(p, int, 0);
module_param(c, int, 0);

// Producer and consumer threads
static struct task_struct* producerThread;
static struct task_struct* consumerThreads[MAX_CONSUMERS];
static int producerThreadCount = 0;
static int consumerThreadCount = 0;
static int numItemsProduced = 0;
static int numItemsConsumed = 0;

// Set up buffer
static struct task_struct* processBuffer[MAX_BUFFER_SIZE];
static int pBufferIndex = 0;
static int cBufferIndex = 0;

// Semaphores
static struct semaphore empty;
static struct semaphore full;
static struct semaphore mutex;

// Time
static int totalTime_ms = 0;

// Producer Function
static int producerFunction(void *arg) {
	struct task_struct* process;
	// Print producer thread
	printk("[kProducer-%d] kthread Producer Created Successfully\n", ++producerThreadCount);
	// Iterate through each process
	for_each_process(process) {
		// Check if thread should stop
		// if (!kthread_should_stop()) { break; }
		// Check if uid matches
		if (process->cred->uid.val == uid) {
			// Aquire locks
			if (down_interruptible(&empty)) { return -ERESTARTSYS; }
			if (down_interruptible(&mutex)) { return -ERESTARTSYS; }
			// if (kthread_should_stop()) { break; }
			// Add to buffer
			processBuffer[pBufferIndex] = process;
			printk("[kProducer-%d] Produced Item#-%d at buffer index:%d for PID:%d\n", p, ++numItemsProduced, pBufferIndex, process->pid);
			// Increase producer buffer index
			pBufferIndex = (pBufferIndex + 1) % buff_size;
			// Release locks
			up(&mutex);
			up(&full);
		}
	}
	return 0;
}

// Consumer Function
static int consumerFunction(void *arg) {
	int time, seconds, minutes, hours;
	int consumerThreadNum = ++consumerThreadCount;
	// Print consumer thread
	printk("[kConsumer-%d] kthread Consumer Created Successfully\n", consumerThreadNum);
	// Infinite loop checking if we should stop
	while (!kthread_should_stop()) {
		// Aquire locks
		if (down_interruptible(&full)) { return -ERESTARTSYS; }
		if (kthread_should_stop()) { break; }
		if (down_interruptible(&mutex)) { return -ERESTARTSYS; }
		if (kthread_should_stop()) { break; }
		// Get time elapsed, rounding to the nearest millisecond and adding that to totalTime
		time = ((ktime_get_ns() - processBuffer[cBufferIndex]->start_time) / 100000 + 5) / 10;
		totalTime_ms += time;
		// Get time rounded to the nearest second
		time = (time / 100 + 5) / 10;
		// Convert time to hours, minutes, seconds
		hours = time / 3600;
		minutes = (time / 60) % 60;
		seconds = time % 60;
		// Print that item was consumed. No need to remove from buffer as producer will overwrite it
		printk("[kConsumer-%d] Consumed Item#-%d on buffer index:%d::PID:%d\tElapsed Time %02d:%02d:%02d\n", consumerThreadNum, ++numItemsConsumed, cBufferIndex, processBuffer[cBufferIndex]->pid, hours, minutes, seconds);
		// Increase consumer buffer index
		cBufferIndex = (cBufferIndex + 1) % buff_size;
		// Release locks
		up(&mutex);
		up(&empty);
	}
	return 0;
}

// Initialize module
int initialize(void) {
	int i;
	// Print messages
	printk("CSE330 Project-2 Kernel Module Inserted\n");
	printk("These are the values passed in originally - UID value: %d, Buffer size value is: %d, Number of producers is: %d, Number of consumers is %d\n", uid, buff_size, p, c);
	// Initialize semaphores
	sema_init(&empty, buff_size);
	sema_init(&full, 0);
	sema_init(&mutex, 1);
	// Initialize producer thread
	if (p >= 1) {
		producerThread = kthread_run(producerFunction, NULL, "producer");
	}
	// Initialize consumer threads
	for (i = 0; i < c ; i++) {
		consumerThreads[i] = kthread_run(consumerFunction, NULL, "consumer");
	}
	return 0;
}

// Exit module
void terminate(void) {
	int i, time, seconds, minutes, hours;
	// Stop producer thread
	if (p >= 1) {
		// kthread_stop(producerThread);
		printk("[kProducer-%d] Producer Thread stopped.\n", p);
	}
	// Stop consumer threads
	for (i = 0; i < c; i++) {
		up(&empty);
		up(&full);
		up(&mutex);
		kthread_stop(consumerThreads[i]);
		printk("[kConsumer-%d] Consumer Thread stopped.\n", i+1);
	}
	// Print total number of items produced and consumed
	printk("Total number of items produced: %d\n", numItemsProduced);
	printk("Total number of items consumed: %d\n", numItemsConsumed);
	// Convert totalTime from milliseconds to seconds
	time = (totalTime_ms / 100 + 5) / 10;
	// Conver time to hours, minutes, seconds
	hours = time / 3600;
	minutes = (time / 60) % 60;
	seconds = time % 60;
	// Print total time elapsed and that the module was removed
	printk("The total elapsed time of all processes for UID %d is %02d:%02d:%02d\n", uid, hours, minutes, seconds);
	printk("CSE330 Project-2 Kernel Module Removed\n");
}

// Module functions
module_init(initialize);
module_exit(terminate);
