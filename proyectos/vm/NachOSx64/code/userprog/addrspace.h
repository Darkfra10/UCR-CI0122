// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "translate.h" // TranslationEntry

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
  public:
    AddrSpace(OpenFile *executable);	// Create an address space,
					// initializing it with the program
					// stored in the file "executable"

    
    // Constructor for a thread that will execute the same executable
    // they need to share the same address space, the stack is the only
    // thing that needs to be different
    AddrSpace(AddrSpace *space);	
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch 

    int tlbCounter;
    // Private methods to handle page faults
    /**
     * @brief Return the physical address of the page containing the virtual address
     * @param vaddr The virtual address
     * @return The physical address or -1 if the page is not in memory
    */
    int getPA(int vpn);
  private:
    TranslationEntry *pageTable;	// Assume linear page table translation
					// for now!

    TranslationEntry *tlb;
    unsigned int numPages;		// Number of pages in the virtual 
					// address space

};

#endif // ADDRSPACE_H
