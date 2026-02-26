// alarm.h
//	Data structures for a software alarm clock.
//
//	We make use of a hardware timer device, that generates
//	an interrupt every X time ticks (on real systems, X is
//	usually between 0.25 - 10 milliseconds).
//
//	From this, we provide the ability for a thread to be
//	woken up after a delay; we also provide time-slicing.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ALARM_H
#define ALARM_H

#include "copyright.h"
#include "utility.h"
#include "callback.h"
#include "timer.h"
#include "thread.h"
#include "list.h"

// A simple struct to pair a sleeping thread with its scheduled wake-up tick
struct SleepingThread {
    Thread* thread;  // The thread that is sleeping
    int wakeUpTick;  // The tick at which this thread should wake up
};

// The following class defines a software alarm clock.
class Alarm : public CallBackObj {
   public:
    Alarm(bool doRandomYield);  // Initialize the timer, and callback
                                // to "toCall" every time slice.
    ~Alarm() {
        delete timer;
        delete sleepingList;
    }

    void WaitUntil(int x);  // Suspend execution until time > now + x ticks

   private:
    Timer* timer;                         // the hardware timer device
    List<SleepingThread*>* sleepingList;  // list of sleeping threads

    void CallBack();  // called when the hardware timer generates an interrupt
};

#endif  // ALARM_H
