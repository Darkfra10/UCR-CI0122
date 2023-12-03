// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.


// #include "nachosOpenFilesTable.h"
#include <unistd.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <fcntl.h> 
#include <cstring>
#include <stdio.h>
#include <map>
#include <arpa/inet.h>  // for inet_pton
#include <sys/socket.h>
#include <sys/types.h>  // for connect 

#include "../threads/synch.h"
#include "addrspace.h"
#include "bitmap.h"
#include "copyright.h"
#include "machine.h"
#include "system.h"
#include "syscall.h"


#define BITMAP_SIZE 128

std::map<int, bool> socketMap;
BitMap * processMap = new BitMap(BITMAP_SIZE);

/**
 * Class that manages the the data of a open process
*/
struct currentProcess {
  currentProcess();
  long id;
  char* fileName;
  Semaphore * semTh;
};

currentProcess::currentProcess() {
  id = -1;
  fileName = NULL;
  semTh = NULL;
}

std::vector <currentProcess*> processTable (sizeof(currentProcess) * 128);

void returnFromSystemCall() {
  machine->WriteRegister( PrevPCReg, machine->ReadRegister( PCReg ) );        // PrevPC <- PC
  machine->WriteRegister( PCReg, machine->ReadRegister( NextPCReg ) );            // PC <- NextPC
  machine->WriteRegister( NextPCReg, machine->ReadRegister( NextPCReg ) + 4 );    // NextPC <- NextPC + 4
}  

/*
 *  System call interface: Halt()
 */
void NachOS_Halt() {		// System call 0
  DEBUG('a', "Shutdown, initiated by user program.\n");
  currentThread->Finish();
  interrupt->Halt();
  returnFromSystemCall();  // Update the PC registers
}

/*
 *  System call interface: void Exit( int )
 */
void NachOS_Exit() {		// System call 1
  DEBUG ('a', "Entering exit  syscall...\n");
  std::cout << "Exit syscall" << std::endl;

  // Get the process data associated with the current thread
  currentProcess * processData = processTable[currentThread->id];

  // openFilesTable, current thread is exiting (being deleted)
  nachosOpenFilesTable->delThread();
  if (processData != nullptr) {
    Semaphore* sem = processData->semTh;
    if (sem != nullptr) {
      // If it does exist, signal the process semaphore
      DEBUG('f', "Process %d is waiting...\n", processData->id);
      // Signal the process semaphore to wake up the parent process
      sem->V();
    }
  }
  
  delete currentThread->space;
  // Yield the CPU to allow other threads to execute
  currentThread->Yield();
  // exit the current thread
  currentThread->Finish();
  machine->WriteRegister(2, 0); // success
  returnFromSystemCall();
}

/**
 * lee datos de la memoria de usuario y los coloca en un buffer el cual retorna
*/
char* readExecEntry() {
  // Read the file address from reg 4
  int fileAddr = machine->ReadRegister(4);

  // Buffer to store the content of the file being writte
  char* writeBuffer = new char[100];
  int size = 101; // Add + 1 cause the last char is \0
  memset(writeBuffer, 0, size);


  char buffer = 0;
  int positionBuffer = 0;

  // Copy the content from memory to the buffer
  for (int position = 0; position < size; position++) {
    positionBuffer = position;
    // Read a char from memory and store it in the buffer
    machine->ReadMem(fileAddr + position, 1, (int*) &buffer);
    position = positionBuffer;
    if (buffer != 0) {
        writeBuffer[position] = buffer;
    }
  }

  return writeBuffer;
}


void NachosAssistExec(void* data) {
  // Get the process data assocaited with the specified position in the table
  currentProcess* processData = processTable[(long) data];

  OpenFile* executable = fileSystem->Open(processData->fileName);
  if (executable == NULL) {
    std::cout << "Unable to open file " << processData->fileName << std::endl;
    return; /*returnFromSystemCall();*/
  }

  // New constructor to copy the shared segments and create a new stack
  AddrSpace* space = new AddrSpace(executable);    
  currentThread->space = space;

  // Close the executable file
  delete executable;

  space->InitRegisters();
  space->RestoreState();
  machine->Run();
  
  std::cerr << "This message should never be printed" << std::endl;
  ASSERT(false);
}


/*
 *  System call interface: SpaceId Exec( char * )
 */
void NachOS_Exec() {		// System call 2
  // printf("Empieza EXEC\n");
  DEBUG ('a', "Entering exec  syscall...\n");

  // Create a new thread to execute the user thread
  Thread* child = new Thread("ChildThread");

  char* fileName = readExecEntry();
  currentProcess * processData = new currentProcess();

  // The thread id is the position in the processMap
  long bitClear = processMap->Find();
  child->id = bitClear;

  if (child->id == -1 ) {
    std::cout << "No hay espacio para el proceso" << std::endl;
    machine->WriteRegister(2, -1); // Fail 
  } else {

    DEBUG('f', "Thread id: %d\n", child->id);
    // The new process id is the thread id
    processData->id = child->id;
    processData->fileName = fileName;
    processData->semTh = new Semaphore("Process Sem", 0);
    // Store the process data in the process table at the specified position 
    processTable[bitClear] = processData;
    // Fork the new thread to execute the executable file
    child->Fork(NachosAssistExec, (void*) bitClear);
    machine->WriteRegister(2, 0); // Success
  }

  currentThread->Yield();
  returnFromSystemCall();
}

/*
 *  System call interface: int Join( SpaceId )
 */
void NachOS_Join() {		// System call 3
  DEBUG ('a', "Entering join  syscall...\n");

  // Get the process id from the r4 register
	long id = machine->ReadRegister(4);


  DEBUG('a', "Process id: %d\n", id);
  // Check if the process id is valid
  if (processMap->Test(id)) {
    // Wait for the process semaphore to be signaled
    processTable[id]->semTh->P();
    // Clear the process id from the process map
    processMap->Clear(id);
    // Return the process id
    machine->WriteRegister(2, 0);
  } else {
    std::cout << "Process id is not valid" << std::endl;
    machine->WriteRegister(2, -1);  // Fail
  }
  returnFromSystemCall();   
}

/*
 *  System call interface: void Create( char * )
 */
int iteratorCreate = 0;
void NachOS_Create() {		// System call 4
  DEBUG ('a', "Entering create  syscall...\n");

  // Get the address of the filename from r4 register
  int fileAddr = machine->ReadRegister(4);

  char charFileName;
  char fileName[FILENAME_MAX + 1];
  memset(fileName, 0, FILENAME_MAX + 1);
  
  // Read the filename from the memory and store it in a string
  for(int i = 0; ; i++) {
    machine->ReadMem(fileAddr + iteratorCreate, 1, (int*)&charFileName);
    fileName[iteratorCreate] = charFileName;

    if (charFileName == '\0') {
      break;
    }
    iteratorCreate++;
  }

  fileName[iteratorCreate] = '\0'; // Concatenate the end of string char

  ASSERT(fileSystem->Create(fileName, 100));
  machine->WriteRegister(2, 0);
  returnFromSystemCall();
}


/*
 *  System call interface: OpenFileId Open( char * )
 */

int iterator = 0; // Declare outside, still don't know if it's the best way
void NachOS_Open() {		// System call 5
  DEBUG ('a', "Entering open  syscall...\n");

  // buffer: register 4
  // size: register 5
  // id: register 6

  // Get the address of the buffer from r4 register
  // Where the data will be written
  // r4 is used to pass the data write address from the calling code 
  // (user code) and a called function (NachOs_Open())
  // In MIPS: the regs 4 - 7 are used to pass arguments and data references in
  // function calls

  // Read the file address from reg 4
  int fileAddr = machine->ReadRegister(4); //Nombre

  char charFileName;
  char fileName[FILENAME_MAX + 1];
  memset(fileName, 0, FILENAME_MAX + 1);
  
  // Read the filename from the memory and store it in a string
  for( int i = 0; ; i++ ) {
    machine->ReadMem(fileAddr + iterator, 1, (int*)&charFileName);
    fileName[iterator] = charFileName;

    iterator++;

    if (charFileName == '\0') {
      break;
    }
  }
  fileName[iterator] = '\0';

  // Open the file
  int id = open(fileName, O_RDWR | O_CREAT);
  if (id == -1) {
    DEBUG('a', "Error: NachOS_Open() fileOpened");
    machine->WriteRegister(2, -1);
  } else {
    // Add the file to the nachos open files table
    int fileId = nachosOpenFilesTable->Open(id);
    if (fileId == -1) {
      std::cout << "Error: NachOS_Open() fileOpened" << std::endl;
      machine->WriteRegister(2, -1);
    } else {
      std::cout << "File opened with id: " << fileId << std::endl;
      machine->WriteRegister(2, fileId);
    }
  }
  iterator = 0;
  returnFromSystemCall();		// Update the PC registers
}


/*
 *  System call interface: OpenFileId Write( char *, int, OpenFileId )
 */
void NachOS_Write() {		// System call 7
  DEBUG ('f', "Entering write call...\n");


  // Get the address of the filename from r4 register
  int fileAddr = machine->ReadRegister(4);
  // Get the size of the filename from r5 register
  int fileSizeBuff = machine->ReadRegister(5);

  OpenFileId fileId = machine->ReadRegister(6);


  char* writeBuffer = new char[fileSizeBuff + 1];
  memset(writeBuffer, 0, fileSizeBuff + 1);

  char buffer = 0;
  int position = 0;
  
  // Read the filename from the memory and store it in a string
  for (int i = 0; i < fileSizeBuff; i++) {
    position = i;
    machine->ReadMem(fileAddr + i, 1, (int*) &buffer);
    i = position;
    if (buffer != 0) {
      writeBuffer[i] = buffer;
    }
  }


  switch (fileId) {
    case ConsoleInput:
      machine->WriteRegister(2, -1);
      break;

    case ConsoleOutput:
      std::cout << writeBuffer << std::endl;
      machine->WriteRegister(2, fileSizeBuff);
      stats->numConsoleCharsWritten += fileSizeBuff;
      break;
    case ConsoleError:
        // Write the content of the buffer to the console error and return the
        // number of bytes written
        std::cerr << machine->ReadRegister(4) << std::endl;
        // Write the number of bytes written to r2
        machine->WriteRegister(2, fileSizeBuff);
        break;
    default:
      
      // check if the given file is opened
      if (nachosOpenFilesTable->isOpened(fileId)) {
        int unixHandle = nachosOpenFilesTable->getUnixHandle(fileId);
        
        // unix syscall to write the buffer content to the file and return
        // the number of bytes written
        write(unixHandle, writeBuffer, fileSizeBuff);
        machine->WriteRegister(2, fileSizeBuff);
        stats->numDiskWrites += fileSizeBuff;
      } else {
        machine->WriteRegister(2, -1); // Error
      }
      break;
  }
  delete [] writeBuffer;
  returnFromSystemCall();
}


/*
 *  System call interface: OpenFileId Read( char *, int, OpenFileId )
 */
void NachOS_Read() {		// System call 7
  DEBUG ('f', "Entering read  syscall...\n");

   // buffer: register 4
   // size: register 5
   // id: register 6

   // Get the address of the buffer from r4 register
   // Where the data will be written
   // r4 is used to pass the data write address from the calling code 
   // (user code) and a called function (NachOs_Open())
   // In MIPS: the regs 4 - 7 are used to pass arguments and data references in
   // function calls

  // Read the file address from reg 4
  int fileAddr = machine->ReadRegister(4);
  // Get the size of the filename from r5 register
  int fileSizeBuff = machine->ReadRegister( 5 );

  
  // Get the file descriptor from r6 register
  OpenFileId fileId = machine->ReadRegister(6);

  char* readBuffer = new char[fileSizeBuff + 1];
  memset(readBuffer, 0, fileSizeBuff + 1);
  int charsRead = 0;

  // Add sempahaore just one read per time
  semConsole->P();
  switch (fileId) {
    case ConsoleInput:
      while (charsRead < fileSizeBuff) {
        int charRead = getchar();
        if (charRead == EOF) {
          break;
        }
        readBuffer[charsRead] = reinterpret_cast<char&>(charRead);
        charsRead++;
      }
      for (int i = 0; i < fileSizeBuff || i < (int) strnlen(readBuffer, fileSizeBuff) || readBuffer[i] != 0; i++) {
        machine->WriteMem(fileAddr + i, 1, readBuffer[i]);
      }
      machine->WriteRegister(2, strlen(readBuffer));
      stats->numConsoleCharsRead += strlen(readBuffer);
      break;
    case ConsoleOutput:
      machine->WriteRegister(2, -1); // Error
      break;
    default:
      // Validate if the file is opened
      if (nachosOpenFilesTable->isOpened(fileId)) {
        // Get the unix handle from the nachos open files table
        int unixHandle = nachosOpenFilesTable->getUnixHandle(fileId);
        
        // Call the unix syscall to read the file and return the number of
        int bytes = read(unixHandle, readBuffer, fileSizeBuff);
        
        // Write the number of bytes read to the memory
        for (int charPos = 0; charPos < bytes; charPos++) {
          machine->WriteMem(fileAddr + charPos, 1, readBuffer[charPos]);
        }
        // Write the number of bytes read to r2
        machine->WriteRegister(2, bytes);
        stats->numDiskReads += fileSizeBuff; // Update the stats
      } else {
        machine->WriteRegister(2, -1);
      }
      break;
  }

  // Release the semaphore
  semConsole->V();
  returnFromSystemCall();	
}


/*
 *  System call interface: void Close( OpenFileId )
 */
void NachOS_Close() {		// System call 8
  DEBUG ('f', "Entering close  syscall...\n");

  int fileAddr = machine->ReadRegister(4);

  // Validate if the file is opened
  // Check if valid 
   if(!(currentThread->openFiles->openFilesMap->Test(fileAddr))) {
      DEBUG('a', "Error: invalid.\n");
      // Write the error to r2
      machine->WriteRegister(2, -1);
   } else {
      // Get the unix handle of the file from the table
      int unixHandle = currentThread->openFiles->getUnixHandle(fileAddr);
      // Use unix syscall to close the file
      int closeHandle = close(unixHandle);
      
      // Check if the file was closed successfully 
      if (currentThread->openFiles->Close(fileAddr) == 1) {
         // succes
         DEBUG('a', "File closed successfully.\n");
      }
      machine->WriteRegister(2, closeHandle);
   }

  // printf("CLOSE termina\n");
  returnFromSystemCall();	
}

// Pass the user routine address as a parameter for this function
// This function is similar to "StartProcess" in "progtest.cc" file under "userprog"
// Requires a correct AddrSpace setup to work well

void NachosForkThread( void * p ) { // for 64 bits version
  AddrSpace *space;
  space = currentThread->space;
  space->InitRegisters();             // set the initial register values
  space->RestoreState();              // load page table register

  // Set the return address for this thread to the same as the main thread
  // This will lead this thread to call the exit system call and finish
  machine->WriteRegister( RetAddrReg, 4 );

  machine->WriteRegister( PCReg, (long) p );
  machine->WriteRegister( NextPCReg, (long) p + 4 );

  machine->Run();                     // jump to the user progam
  ASSERT(false);
}

/*
 *  System call interface: void Fork( void (*func)() )
 */
void NachOS_Fork() {		// System call 9
  DEBUG( 'u', "Entering Fork System call\n" );
  // We need to create a new kernel thread to execute the user thread
  Thread * child = new Thread( "Kernel child" );

  // We need to share the Open File Table structure with this new child
  child->openFiles = currentThread->openFiles;

  // Child and father will also share the same address space, except for the stack
  // Text, init data and uninit data are shared, a new stack area must be created
  // for the new child
  child->space = new AddrSpace( currentThread->space );

  // We (kernel)-Fork to a new method to execute the child code
  // Pass the user routine address, now in register 4, as a parameter
  // Note: in 64 bits register 4 need to be casted to (void *)
  child->Fork( NachosForkThread, reinterpret_cast<void*>(machine->ReadRegister( 4 )) );

  DEBUG( 'u', "Exiting Fork System call\n" );
  returnFromSystemCall();
}


/*
 *  System call interface: void Yield()
 */
void NachOS_Yield() {		// System call 10
  currentThread->Yield();
  returnFromSystemCall();
}


/*
 *  System call interface: Sem_t SemCreate( int )
 */
void NachOS_SemCreate() {		// System call 11
  returnFromSystemCall();
}


/*
 *  System call interface: int SemDestroy( Sem_t )
 */
void NachOS_SemDestroy() {		// System call 12
  returnFromSystemCall();
}


/*
 *  System call interface: int SemSignal( Sem_t )
 */
void NachOS_SemSignal() {		// System call 13
  returnFromSystemCall();
}


/*
 *  System call interface: int SemWait( Sem_t )
 */
void NachOS_SemWait() {		// System call 14
  returnFromSystemCall();
}


/*
 *  System call interface: Lock_t LockCreate( int )
 */
void NachOS_LockCreate() {		// System call 15
  returnFromSystemCall();
}


/*
 *  System call interface: int LockDestroy( Lock_t )
 */
void NachOS_LockDestroy() {		// System call 16
  returnFromSystemCall();
}


/*
 *  System call interface: int LockAcquire( Lock_t )
 */
void NachOS_LockAcquire() {		// System call 17
  returnFromSystemCall();
}


/*
 *  System call interface: int LockRelease( Lock_t )
 */
void NachOS_LockRelease() {		// System call 18
  returnFromSystemCall();
}


/*
 *  System call interface: Cond_t LockCreate( int )
 */
void NachOS_CondCreate() {		// System call 19
  returnFromSystemCall();
}


/*
 *  System call interface: int CondDestroy( Cond_t )
 */
void NachOS_CondDestroy() {		// System call 20
  returnFromSystemCall();
}


/*
 *  System call interface: int CondSignal( Cond_t )
 */
void NachOS_CondSignal() {		// System call 21
  returnFromSystemCall();
}


/*
 *  System call interface: int CondWait( Cond_t )
 */
void NachOS_CondWait() {		// System call 22
  returnFromSystemCall();
}


/*
 *  System call interface: int CondBroadcast( Cond_t )
 */
void NachOS_CondBroadcast() {		// System call 23
  returnFromSystemCall();
}


/*
 *  System call interface: Socket_t Socket( int, int )
 */
void NachOS_Socket() {			// System call 30
  int isIpPv6 = machine->ReadRegister(4);
  int socketType = machine->ReadRegister(5);


  isIpPv6 = (isIpPv6 == AF_INET_NachOS) ? AF_INET : AF_INET6;
  socketType = (socketType == SOCK_STREAM_NachOS) ? SOCK_DGRAM : SOCK_STREAM;  

  int idSocket = socket(isIpPv6, socketType, 0);
  int fd = currentThread->openFiles->Open(idSocket);
  machine->WriteRegister(2, fd);

  if (isIpPv6 == AF_INET6) {
    socketMap.insert({fd, true});
  }

  returnFromSystemCall();
}


/*
 *  System call interface: Socket_t Connect( char *, int )
 */
void NachOS_Connect() {		// System call 31
  int idSocket = machine->ReadRegister(4);
  int ipAddress = machine->ReadRegister(5);
  int port = machine->ReadRegister(6);

  char* hostip = new char[40];
  for (int i = 0; i < 40; ++i) {
    machine->ReadMem(ipAddress+i, 1, (int*)&hostip[i]);
    if (hostip[i] == '\0') {
      break;
    }
  }

  int fd = currentThread->openFiles->getUnixHandle(idSocket);
  auto it = socketMap.find(fd);

  int st = 0;

  struct sockaddr_in  host4;
  struct sockaddr_in6  host6;
  struct sockaddr * ha;

  if (it == socketMap.end()) {
    memset( (char *) &host4, 0, sizeof( host4 ) );
    host4.sin_family = AF_INET;
    st = inet_pton( AF_INET, hostip, &host4.sin_addr );
    if ( -1 == st ) {
      throw( std::runtime_error( "Nachos_Connect(): ipv4 inet" ));
    }

    host4.sin_port = htons( port );
    st = connect( idSocket, (sockaddr *) &host4, sizeof( host4 ) );
    if ( -1 == st ) {
      throw( std::runtime_error(  "Nachos_Connect(): ipv4 connect" ));
    }
  } else {
    memset( &host6, 0, sizeof( host6 ) );
    host6.sin6_family = AF_INET6;
    st = inet_pton( AF_INET6, hostip, &host6.sin6_addr );
    if ( -1 == st ) {	// 0 means invalid address, -1 means address error
      throw std::runtime_error( "Nachos_Connect(): ipv6 inet" );
    }

    host6.sin6_port = htons( port );
    ha = (struct sockaddr *) &host6;
    st = connect( idSocket, ha, sizeof( host6 ) );
    if ( -1 == st ) {	// 0 means invalid address, -1 means address error
      throw std::runtime_error( "Nachos_Connect(): ipv6 connect" );
    }
  }
  returnFromSystemCall();
}


/*
 *  System call interface: int Bind( Socket_t, int )
 */
void NachOS_Bind() {		// System call 32
}


/*
 *  System call interface: int Listen( Socket_t, int )
 */
void NachOS_Listen() {		// System call 33
}


/*
 *  System call interface: int Accept( Socket_t )
 */
void NachOS_Accept() {		// System call 34
}


/*
 *  System call interface: int Shutdown( Socket_t, int )
 */
void NachOS_Shutdown() {	// System call 25
}


//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    switch ( which ) {

      case SyscallException:
        switch ( type ) {
            case SC_Halt:		// System call # 0
              NachOS_Halt();
              break;
            case SC_Exit:		// System call # 1
              NachOS_Exit();
              break;
            case SC_Exec:		// System call # 2
              NachOS_Exec();
              break;
            case SC_Join:		// System call # 3
              NachOS_Join();
              break;

            case SC_Create:		// System call # 4
              NachOS_Create();
              break;
            case SC_Open:		// System call # 5
              NachOS_Open();
              break;
            case SC_Read:		// System call # 6
              NachOS_Read();
              break;
            case SC_Write:		// System call # 7
              NachOS_Write();
              break;
            case SC_Close:		// System call # 8
              NachOS_Close();
              break;

            case SC_Fork:		// System call # 9
              NachOS_Fork();
              break;
            case SC_Yield:		// System call # 10
              NachOS_Yield();
              break;

            case SC_SemCreate:         // System call # 11
              NachOS_SemCreate();
              break;
            case SC_SemDestroy:        // System call # 12
              NachOS_SemDestroy();
              break;
            case SC_SemSignal:         // System call # 13
              NachOS_SemSignal();
              break;
            case SC_SemWait:           // System call # 14
              NachOS_SemWait();
              break;

            case SC_LckCreate:         // System call # 15
              NachOS_LockCreate();
              break;
            case SC_LckDestroy:        // System call # 16
              NachOS_LockDestroy();
              break;
            case SC_LckAcquire:         // System call # 17
              NachOS_LockAcquire();
              break;
            case SC_LckRelease:           // System call # 18
              NachOS_LockRelease();
              break;

            case SC_CondCreate:         // System call # 19
              NachOS_CondCreate();
              break;
            case SC_CondDestroy:        // System call # 20
              NachOS_CondDestroy();
              break;
            case SC_CondSignal:         // System call # 21
              NachOS_CondSignal();
              break;
            case SC_CondWait:           // System call # 22
              NachOS_CondWait();
              break;
            case SC_CondBroadcast:           // System call # 23
              NachOS_CondBroadcast();
              break;

            case SC_Socket:	// System call # 30
              NachOS_Socket();
              break;
            case SC_Connect:	// System call # 31
              NachOS_Connect();
              break;
            case SC_Bind:	// System call # 32
              NachOS_Bind();
              break;
            case SC_Listen:	// System call # 33
              NachOS_Listen();
              break;
            case SC_Accept:	// System call # 32
              NachOS_Accept();
              break;
            case SC_Shutdown:	// System call # 33
              NachOS_Shutdown();
              break;

            default:
              printf("Unexpected syscall exception %d\n", type );
              ASSERT( false );
              break;
      }
      break;

      case PageFaultException: {
        AddrSpace *space = currentThread->space; // Exist 
        ++stats->numPageFaults;

        int virtualAdress = machine->ReadRegister( BadVAddrReg );
        int fail = space->pageFaultHandler(virtualAdress / PageSize);
        if (fail == -1) {
          std::cout << "PageFaultException: No hay espacio en memoria" << std::endl;
          ASSERT(false);
        }
        break;
      }

      case ReadOnlyException:
        printf( "Read Only exception (%d)\n", which );
        ASSERT( false );
        break;

      case BusErrorException:
        printf( "Bus error exception (%d)\n", which );
        ASSERT( false );
        break;

      case AddressErrorException:
        printf( "Address error exception (%d)\n", which );
        ASSERT( false );
        break;

      case OverflowException:
        printf( "Overflow exception (%d)\n", which );
        ASSERT( false );
        break;

      case IllegalInstrException:
        printf( "Ilegal instruction exception (%d)\n", which );
        ASSERT( false );
        break;

      default:
        printf( "Unexpected exception %d\n", which );
        ASSERT( false );
        break;
  }
}
