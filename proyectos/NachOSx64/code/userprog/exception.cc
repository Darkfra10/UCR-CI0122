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
#include <fcntl.h> 
#include <unistd.h>
#include <iostream>
#include "../threads/synch.h"
#include <unistd.h>
#include <vector>

#include <cstring>
#include <stdio.h>

#include <arpa/inet.h>  // for inet_pton
#include <sys/types.h>  // for connect 
#include <sys/socket.h>

#include "addrspace.h"
#include "bitmap.h"
#include "copyright.h"
#include "machine.h"
#include "system.h"
#include "syscall.h"

#include <map>
std::map<int, bool> socketMap;
BitMap * fileMap = new BitMap(128);

/**
 * Almacena la información correspondiente del hilo. 
*/
struct infoThread {
  long  id;
  char* fileName;
  Semaphore * semTh;
  inline infoThread()
  : id(-1)
  , fileName(NULL)
  , semTh(NULL){}
};

std::vector <infoThread*> threadData (sizeof(infoThread) * 128);

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
}

/*
 *  System call interface: void Exit( int )
 */
void NachOS_Exit() {		// System call 1
  DEBUG('b',"Exit, initiated by user program.\n");
  nachosOpenFilesTable->delThread();
  infoThread * dataAux = threadData[currentThread->id];
  if (dataAux != nullptr) {
    Semaphore* sem = dataAux->semTh;
    if (sem != nullptr) {
      DEBUG( 'f', "Soy el id dentro del if del semaforo %d\n" , dataAux->id);
      sem->V();
    }
  }
  // threadData[(long) currentThread->id]->semTh->V();
  delete currentThread->space;
  currentThread->Yield();
  currentThread->Finish();
  machine->WriteRegister(2, 0);
  returnFromSystemCall();
}

/**
 * lee datos de la memoria de usuario y los coloca en un buffer el cual retorna
*/
char* readEntry() {
   int bufferAddr = machine->ReadRegister(4);
   char* writeBuffer = new char[100];
   int size = 101;
   memset(writeBuffer, 0, size);
   char buffer = 0;
   int posBuffer = 0;
   for (int charPos = 0; charPos < size; charPos++) {
      posBuffer = charPos;
      machine->ReadMem(bufferAddr + charPos, 1, (int*) &buffer);
      charPos = posBuffer;
      if (buffer != 0) {
         writeBuffer[charPos] = buffer;
      }
   }
   return writeBuffer;
}

void NachosAsistExec(void* data) {
  // printf("Empieza EXEC_ASSIS\n");
  infoThread* dataAux = threadData[(long) data];
  OpenFile* executable = fileSystem->Open(dataAux->fileName);
  if (executable == NULL) {
    printf("Cant open file\n");
    return; /*returnFromSystemCall();*/
  }
  AddrSpace* space = new AddrSpace(executable);    
  currentThread->space = space;
  delete executable;
  space->InitRegisters();
  space->RestoreState();
  machine->Run();
  // threadData[(long) data]->semTh->V();
  printf("Termina EXEC_ASSIS\n");
  ASSERT(false);
}


/*
 *  System call interface: SpaceId Exec( char * )
 */
void NachOS_Exec() {		// System call 2
  // printf("Empieza EXEC\n");
  DEBUG('c',"Exec, initiated by user program.\n");
  Thread* newThread = new Thread("New Thread");
  char* fileName = readEntry();
  infoThread * threads = new infoThread();
  long bitClear = fileMap->Find();
  newThread->id = bitClear;
  if (newThread->id == -1 ) {
    printf("No space for new file\n");
    machine->WriteRegister(2, -1);
  } else {
    DEBUG( 'f', "ID of thread %d\n", threads->id );
    threads->id = newThread->id;
    threads->fileName = fileName;
    threads->semTh = new Semaphore("Thread Sem", 0);
    threadData[bitClear] = threads;
    newThread->Fork(NachosAsistExec, (void*) bitClear);
    machine->WriteRegister(2, 0);
  }
  currentThread->Yield();
  // printf("Termina EXEC_ASSIS\n");
  // printf("Termina EXEC\n");
  returnFromSystemCall();
}

/*
 *  System call interface: int Join( SpaceId )
 */
void NachOS_Join() {		// System call 3
  DEBUG('d',"Join, initiated by user program.\n");
  // printf("JOIN EMPIEZA\n");
	long id = machine->ReadRegister(4);  // se lee id ID del proceso que hay que esperar a que termine
  DEBUG( 'f', "Id de join %d \n" , id);
  if (fileMap->Test(id)) {
    threadData[id]->semTh->P();
    fileMap->Clear(id);
    machine->WriteRegister(2, 0);
  } else {
    printf("id del proceso invalido\n");
    machine->WriteRegister(2, -1);  
  }
  // printf("JOIN TERMINA\n");
  DEBUG( 'f', "Saliendo a join\n" );
  returnFromSystemCall();   
}

/*
 *  System call interface: void Create( char * )
 */
int iteratorCreate = 0;
void NachOS_Create() {		// System call 4
  // printf("CREATE EMPIEZA\n");

  //Comienzo de la string
  int fileNameAddr = machine->ReadRegister(4); //Nombre
  if(fileNameAddr == 0) {
    DEBUG('a', "Error: address to filename char* invalid: void NachOS_Create()");
  }
  char charFileName;
  char fileName[FILENAME_MAX + 1];
  
  //ReadMemFromUser debe traducir las paginas virtuales para leer el string de la memoria 10:00
  for(int i = 0; ; i++) {
    if (!machine->ReadMem(fileNameAddr + iteratorCreate, 1, (int*)&charFileName)) {
      DEBUG('a', "Error: (!machine->ReadMem(fileNameAddr, 1, (int*)&fileName))");
    }
    fileName[iteratorCreate] = charFileName;

    if (charFileName == '\0') {
      break;
    }
    iteratorCreate++;
  }

  fileName[iteratorCreate] = '\0';
  // printf("\tFilename is: %s\n", fileName);

  DEBUG('a', "File has been created: NachOS_Create()");
  ASSERT(fileSystem->Create(fileName, 100));
  machine->WriteRegister(2, 0);
  // printf("CREATE TERMINA\n");
  returnFromSystemCall();
}


/*
 *  System call interface: OpenFileId Open( char * )
 */
int iteratorOpen = 0;
void NachOS_Open() {		// System call 5
  // Read the name from the user memory, see 5 below
  // Use NachosOpenFilesTable class to create a relationship
  // between user file and unix file
  // Verify for errors
  // printf("Open empieza\n");
  //Comienzo de la string
  int fileNameAddr = machine->ReadRegister(4); //Nombre
  if(fileNameAddr == 0) {
    DEBUG('a', "Error: address to filename char* invalid: void NachOS_Open()");
  }
  char charFileName;
  char fileName[FILENAME_MAX + 1];
  memset(fileName, 0, FILENAME_MAX + 1);
  
  //Para obtener cada caracter del nombre y formar una string
  for( int i = 0; ; i++ ) {
    if (!machine->ReadMem(fileNameAddr + iteratorOpen, 1, (int*)&charFileName)) {
      DEBUG('a', "Error: NachOS_Open()");
    }

    fileName[iteratorOpen] = charFileName;

    iteratorOpen++;

    if (charFileName == '\0') {
      break;
    }
  }
  fileName[iteratorOpen] = '\0';

  // printf("\tFilename is: %s\n", fileName);

  DEBUG('a', "File has been opened: NachOS_Open()");
  int id = open(fileName, O_RDWR | O_CREAT);
  if (id == -1) {
    DEBUG('a', "Error: NachOS_Open() fileOpened");
    machine->WriteRegister(2, -1);
  } else {
    int added = nachosOpenFilesTable->Open(id);
    if (added == -1) {
      DEBUG('a', "Cant add the file to nachos open files table: NachOS_Open()");
      // printf("OPEN: Cant add the file to table\n");
      machine->WriteRegister(2, -1);
    } else {
      // printf("OPEN: Successful\n");
      machine->WriteRegister(2, added); //Devuelve el nachosHandle
    }
  }
  // printf("Open termina\n");
  iteratorOpen = 0;
  returnFromSystemCall();		// Update the PC registers
}

/*
 *  System call interface: OpenFileId Write( char *, int, OpenFileId )
 */
void NachOS_Write() {		// System call 7
  DEBUG( 'f', "Entrando a write\n" );
  int bufferAddr = machine->ReadRegister(4);
  int size = machine->ReadRegister(5);
  OpenFileId descriptorFile = machine->ReadRegister(6);
  char* writeBuffer = new char[size + 1];
  memset(writeBuffer, 0, size + 1);
  char buffer = 0;
  int pos = 0;
  // Mediante la direccion del registro 4 lee y alamacena en writebuffer.
  for (int i = 0; i < size; i++) {
    pos = i;
    machine->ReadMem(bufferAddr + i, 1, (int*) &buffer);
    i = pos;
    if (buffer != 0) {
      writeBuffer[i] = buffer;
    }
  }
  switch (descriptorFile) {
    case ConsoleInput:
      machine->WriteRegister(2, -1);
      break;
    case ConsoleOutput:
      printf("%s\n", writeBuffer);
      machine->WriteRegister(2, size);
      stats->numConsoleCharsWritten += size;
      break;
    default:
      // Verifica si el archivo está abierto
      if (nachosOpenFilesTable->isOpened(descriptorFile)) {
        int unixHandle = nachosOpenFilesTable->getUnixHandle(descriptorFile);
        // Escribe apartir del unixHandle y lo almacena en writeBuffer
        machine->WriteRegister(2, size);
        printf("%d\n", size);
        write(unixHandle, writeBuffer, size);
        stats->numDiskWrites += size;
      } else {
        machine->WriteRegister(2, -1); // return -1 in r2
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
  // printf("READ empieza\n");
  int bufferAddr = machine->ReadRegister(4);
  int size = machine->ReadRegister( 5 );	// Read size to write
  // buffer = Read data from address given by user;
  OpenFileId descriptor = machine->ReadRegister( 6 ); // Read file descriptor

  char* readBuffer = new char[size + 1];
  memset(readBuffer, 0, size + 1);
  int charReaded = 0;

  semConsole->P();
  switch (descriptor) {
    case ConsoleInput:
      while (charReaded < size) {
        int value = getchar();
        if (value == EOF) {
          break;
        }
        readBuffer[charReaded] = (char) value;
        charReaded++;
      }
      for (int i = 0; i < size || i < (int) strnlen(readBuffer, size) || readBuffer[i] != 0; i++) {
        machine->WriteMem(bufferAddr + i, 1, readBuffer[i]);
      }
      machine->WriteRegister(2, strlen(readBuffer));
      stats->numConsoleCharsRead += strlen(readBuffer);
      break;
    case ConsoleOutput:
      machine->WriteRegister(2, -1);
      break;
    default:
      // Verifica si el archivo está abierto
      if (nachosOpenFilesTable->isOpened(descriptor)) { // currentThread->openFilesTable->isOpened(descriptor)
        int unixHandle = nachosOpenFilesTable->getUnixHandle(descriptor);
        // Lee apartir del unixHandle y lo almacena en readBuffer
        int bytes = read(unixHandle, readBuffer, size);
        // Escribe el contenido de readBuffer a la memoria.
        for (int charPos = 0; charPos < bytes; charPos++) {
          machine->WriteMem(bufferAddr + charPos, 1, readBuffer[charPos]);
        }
        machine->WriteRegister(2, bytes);
        stats->numDiskReads += size;
      } else {
        machine->WriteRegister(2, -1);
      }
      break;
  }

  semConsole->V();
  // printf("READ termina\n");
  returnFromSystemCall();	
}


/*
 *  System call interface: void Close( OpenFileId )
 */
void NachOS_Close() {		// System call 8
  // printf("CLOSE empieza\n");
  int file_id = machine->ReadRegister(4);
  if (file_id < 0 || file_id  >= (int) nachosOpenFilesTable->getUsage()) {
    DEBUG('a', "Error: Null pointer exception\n");
  } else {
    int nachosHandle = nachosOpenFilesTable->Close(file_id);
    if (nachosHandle == -1) {
      machine->WriteRegister(2, -1);
    } else {
      int unixHandle = close(nachosHandle);
      DEBUG('a', "Closing file %d\n", file_id);
      if (unixHandle != 0) {
        machine->WriteRegister(2, -1);
      } else {
        machine->WriteRegister(2, 1);
      }
    }
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
  Thread * newT = new Thread( "child to execute Fork code" );

  // We need to share the Open File Table structure with this new child
  newT->openFilesTable = currentThread->openFilesTable;

  // Child and father will also share the same address space, except for the stack
  // Text, init data and uninit data are shared, a new stack area must be created
  // for the new child
  /* ************* newT->stack = new HostMemoryAddress(); *************/
  // We suggest the use of a new constructor in AddrSpace class,
  // This new constructor will copy the shared segments (space variable) from currentThread, passed
  // as a parameter, and create a new stack for the new child
  newT->space = new AddrSpace( currentThread->space );

  // We (kernel)-Fork to a new method to execute the child code
  // Pass the user routine address, now in register 4, as a parameter
  // Note: in 64 bits register 4 need to be casted to (void *)
  newT->Fork( NachosForkThread, (void *) machine->ReadRegister( 4 ) );

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
  int fd = currentThread->openFilesTable->Open(idSocket);
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

  int fd = currentThread->openFilesTable->getUnixHandle(idSocket);
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
