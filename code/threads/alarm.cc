// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "alarm.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom) {
    timer = new Timer(doRandom, this);
    sleepingList = new List<SleepingThread*>();
}

//----------------------------------------------------------------------
// Alarm::WaitUntil
//	Put the calling thread to sleep for 'x' ticks.
//
//	The thread is placed on the sleeping list with its wake-up tick,
//	then blocked. The CallBack() timer interrupt will wake it up
//	when enough time has passed.
//
//	'x' -- number of ticks to sleep
//----------------------------------------------------------------------

void Alarm::WaitUntil(int x) {
    // Disable interrupts to safely modify the sleeping list
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);

    // Calculate the tick at which this thread should wake up
    int wakeUpTick = kernel->stats->totalTicks + x;

    // Create a record for this sleeping thread
    SleepingThread* entry = new SleepingThread();
    entry->thread = kernel->currentThread;
    entry->wakeUpTick = wakeUpTick;

    // Add to sleeping list
    sleepingList->Append(entry);

    // Put this thread to sleep (blocks until ReadyToRun is called on it)
    kernel->currentThread->Sleep(false);

    // Restore interrupt level (this runs after the thread wakes up)
    kernel->interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	On every tick, check if any sleeping threads need to be woken up.
//	If a thread's wakeUpTick <= current tick, move it to the ready queue.
//  When multiple threads wake up at the same tick, wake them in priority
//  order (highest priority first) so the scheduler picks the right one.
//----------------------------------------------------------------------

void Alarm::CallBack() {
    Interrupt* interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();

    // Check the sleeping list and wake up any threads whose time has come.
    // Interrupts are already disabled when CallBack() is called by the timer.
    if (!sleepingList->IsEmpty()) {
        int currentTick = kernel->stats->totalTicks;

        // Collect all threads that need to wake up
        List<SleepingThread*>* toWake = new List<SleepingThread*>();
        List<SleepingThread*>* remaining = new List<SleepingThread*>();

        while (!sleepingList->IsEmpty()) {
            SleepingThread* entry = sleepingList->RemoveFront();
            if (entry->wakeUpTick <= currentTick) {
                toWake->Append(entry);
            } else {
                remaining->Append(entry);
            }
        }

        // Put remaining threads back into the sleeping list
        while (!remaining->IsEmpty()) {
            sleepingList->Append(remaining->RemoveFront());
        }
        delete remaining;

        // Wake up threads in priority order (highest priority first)
        // so they are added to the ready queue in the correct order.
        // Since the priority scheduler's ready queue is a SortedList,
        // the order here is just good practice — the scheduler handles it.
        while (!toWake->IsEmpty()) {
            // Find and remove the highest priority thread from toWake
            SleepingThread* highest = NULL;
            List<SleepingThread*>* temp = new List<SleepingThread*>();

            while (!toWake->IsEmpty()) {
                SleepingThread* entry = toWake->RemoveFront();
                if (highest == NULL || entry->thread->getPriority() >
                                           highest->thread->getPriority()) {
                    if (highest != NULL) temp->Append(highest);
                    highest = entry;
                } else {
                    temp->Append(entry);
                }
            }

            // Put the rest back
            while (!temp->IsEmpty()) {
                toWake->Append(temp->RemoveFront());
            }
            delete temp;

            // Wake up the highest priority thread
            if (highest != NULL) {
                kernel->scheduler->ReadyToRun(highest->thread);
                delete highest;
            }
        }
        delete toWake;
    }

    // Standard time-slicing: yield if something is running
    if (status != IdleMode) {
        interrupt->YieldOnReturn();
    }
}
