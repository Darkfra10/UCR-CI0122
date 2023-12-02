// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"
#include <iostream>

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(const char* debugName, int initialValue)
{
    name = (char *)debugName;
    value = initialValue;
    //! Esta lista contiene todos los hilos que estan dormidos en este semaforo
    queue = new List<Thread*>;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    //! deshabilita las interrupciones, esto para poder tocar valores sin que ningun otro hilo se entrometa
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    //! si el semaforo esta en cero
    while (value == 0) { 			// semaphore not available
        //! Poner a dormir al hilo que quiso decrementar el semaforo
        queue->Append(currentThread);		// so go to sleep
        //! "dormir" para el hilo al estado bloqueado hasta que otro hilo lo desbloquee
        //! "desbloquearlo" es pasarlo de nuevo a la lista de procesos listos.
        currentThread->Sleep();
    } 
    //! cuando un semaforo es mayor a cero lo podemos decrementar
    value--; 					// semaphore available, 
						// consume its value
    //! Se deshabilitan las interrupciones para estar seguros de que no habra un cambio de contexto 
    //! mientras se ejecuta esta funcion
    interrupt->SetLevel(oldLevel);		// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

//! Lo que puede desbloquear a los hilos bloqueados es subir el semaforo
void
Semaphore::V()
{
    Thread *thread;
    //! Aca sea deshabilitan las interrupciones nuevamente
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    //! Remove es equivalente a Pop en la cola
    thread = queue->Remove();
    //! Si habia algo en la cola, toma el hilo bloqueado y lo pone en el estado "listo"
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	    scheduler->ReadyToRun(thread);
    value++;
    interrupt->SetLevel(oldLevel);
}

#ifdef USER_PROGRAM
//----------------------------------------------------------------------
// Semaphore::Destroy
// 	Destroy the semaphore, freeing the waiting threads
//	This is used to destroy a user semaphore
//----------------------------------------------------------------------

void
Semaphore::Destroy()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    while ( (thread = queue->Remove() ) != NULL )	// make thread ready
	scheduler->ReadyToRun(thread);

    interrupt->SetLevel(oldLevel);
}

#endif


// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(const char* debugName) {
    name = (char *)debugName;
    value = 1; // Porque es un lock
    //! Esta cola contiene los hilos que estan dormidos en este lock
    queue = new List<Thread*>;
    currentThreadHolder = NULL;
}


Lock::~Lock() {
    delete queue;
}

//! Wait
void Lock::Acquire() {
    //! deshabilita las interrupciones, esto para poder tocar valores sin que ningun otro hilo se entrometa
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) {
        queue->Append(currentThread);		// so go to sleep
        currentThread->Sleep();
    }
    value--; // consume its value
    //! Marcar al hilo actual como quien tiene el lock
    this->currentThreadHolder = currentThread;
    interrupt->SetLevel(oldLevel);		// re-enable interrupts
}

//! Signal
void Lock::Release() {
    //! Solo el hilo que hizo lock puede hacer release
    if(this->isHeldByCurrentThread()) {
        IntStatus oldLevel = interrupt->SetLevel(IntOff);
        Thread* Thread;
        Thread = queue->Remove(); //! Pop
        if (Thread != NULL) {
            //! Mover al hilo actual al estado listo, de manera que un cambio de contexto pueda capturarlo
            scheduler->ReadyToRun(Thread);
        }
        value++;
        interrupt->SetLevel(oldLevel);		// re-enable interrupts
    }
}


bool Lock::isHeldByCurrentThread() {
   
   if (this->currentThreadHolder == currentThread) {
        return true;
   }else {
        return false;
   }

}


Condition::Condition(const char* debugName) {
    this->name = (char*) debugName;
    queue = new List<Semaphore*>;
}


Condition::~Condition() {
    delete queue;
}


void Condition::Wait( Lock * conditionLock ) {
    if (conditionLock->isHeldByCurrentThread()) {
        Semaphore* sem = new Semaphore("name", 0);
        queue->Append(sem);
        
        conditionLock->Release();
        // P decrementa
        sem->P();

        conditionLock->Acquire();
        delete sem;
    }
    
}


void Condition::Signal( Lock * conditionLock ) {
    if (conditionLock->isHeldByCurrentThread()) {
        Semaphore* sem;
        if (!queue->IsEmpty()){
            sem = queue->Remove();
            sem->V();
        }
        
    } else {
        std::cout << "Error: signal" << std::endl;
    }
    
}


void Condition::Broadcast( Lock * conditionLock ) {
    while (!queue->IsEmpty()){
        Signal(conditionLock);
    }
    
}


// Mutex class
Mutex::Mutex( const char * debugName ) {

}

Mutex::~Mutex() {

}

void Mutex::Lock() {

}

void Mutex::Unlock() {

}


// Barrier class
Barrier::Barrier( const char * debugName, int count ) {
}

Barrier::~Barrier() {
}

void Barrier::Wait() {
}

