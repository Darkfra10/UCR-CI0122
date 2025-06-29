// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;
    ASSERT(numPages <= (unsigned int) memoryMap->NumClear());
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);

    // first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
        pageTable[i].physicalPage = memoryMap->Find(); // the location is the position within the bitmap this allow us to a program doesn't use the same physical page
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;  // if the code segment was entirely on a separate page, we could set its pages to be read-only
    }

    // LOAD CODE AND DATA SEGMENT FROM THE EXE FILE INTO THE MAIN MEMORY
    // then, copy in the code and data segments into memory

    // Code segment
    // unsigned int cantPagUsadas_CodeSeg = divRoundUp (noffH.code.size, PageSize);
    unsigned int codeSegmentPages = divRoundUp(noffH.code.size, PageSize);
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        
        // Go through the pages assigned to the executable, get the physical address and copy the CS
        for (unsigned int offset = 0; offset < codeSegmentPages; ++offset) {

            //  Get the physical address of the page
            int page = pageTable[offset].physicalPage;

            // Read the code segment data from the exevutable and copy it to the main memory
            // The offset is the number of pages used by the code segment times the size of a page
            // executable->ReadAt (& (machine->mainMemory[page * PageSize]), PageSize, (PageSize * offset + noffH.code.inFileAddr));
            executable->ReadAt (& (machine->mainMemory[page * PageSize]), 
                            PageSize,
                            ( (PageSize * offset) + noffH.code.inFileAddr));
        }
    }
    // unsigned int cantPagUsadas_DataSeg = divRoundUp (noffH.initData.size, PageSize);
    unsigned int dataSegmentPages = divRoundUp(noffH.initData.size, PageSize);
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
            noffH.initData.virtualAddr, noffH.initData.size);
        
        // Go through the pages assigned to the executable, get the physical address and copy the DS
        //
        for (unsigned int offset = codeSegmentPages; offset < dataSegmentPages; ++offset) {
            int page = pageTable[offset].physicalPage;
            executable->ReadAt (& (machine->mainMemory[page * PageSize]),
                            PageSize, 
                            ( (PageSize * offset) + noffH.initData.inFileAddr));
        }
    }

}

/**
 * @brief To copy the shared space segment between the child and father, but 
 * create new stack
 * 
 * @param space  The address space of the father
 */
AddrSpace::AddrSpace(AddrSpace* space) {
    // copy the number of pages of the father to the child
    numPages = space->numPages;

    // create a new page table for the child
    pageTable = new TranslationEntry[numPages];

    // size shared is the all sectors minus the stack
    unsigned int sharedMemorySize = numPages - divRoundUp(UserStackSize, PageSize);
    unsigned int index = 0; // index to go through the pages.
     // share the text & data segments between father and child address spaces
    for (; index < sharedMemorySize; index++) {
        pageTable[index].virtualPage = index;	// for now, virtual page # = phys page #
        pageTable[index].physicalPage = space->pageTable[index].physicalPage; // the location is the position within the bitmap they share the same physical page
        pageTable[index].valid = true;
        pageTable[index].use = false;
        pageTable[index].dirty = false;
        pageTable[index].readOnly = false;
    }

    // Go through each page to allocate memory for the child's adress space
    for (; index < this->numPages; index++) {
        pageTable[index].virtualPage = index;	// for now, virtual page # = phys page #
        pageTable[index].physicalPage = memoryMap->Find(); // Find free space to assign to the physical page
        pageTable[index].valid = true;
        pageTable[index].use = false;
        pageTable[index].dirty = false;
        pageTable[index].readOnly = false;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
}
//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    // Clear the memory map
    for (unsigned int page = 0; page < this->numPages; page++) {
        memoryMap->Clear(this->pageTable[page].physicalPage);
    }
   delete pageTable;

}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
