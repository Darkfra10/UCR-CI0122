#ifndef SWAP_H
#define SWAP_H

#include "machine.h"
#include "openfile.h"
#include "bitmap.h"
#include "addrspace.h"

#include <array>

class Swap {
  public:
    Swap(/* args */);
    ~Swap();

    // int findFreePage(); // 
    // int findPageInSwap(int vpn);
    // // int readFromDisk(int vpn);
    // void writeOnDisk(unsigned int vpn);


    // !FIND (MAIN MEMORY)PAGE TO MOVE TO THE SWAP
    int findPageToSwap(); // return virtual page number (in main memory)
    int findFreePageInSwap(); // return physical page number (in swap)
    void movePageToSwap(int vpn, int ppn); // move page from main memory to swap ONLY IF IT IS DIRTY
    void movePageToMainMemory(int vpn, int ppn); // move page from swap to main memory



    int size = 0; // Number of pages the swap can hold. Is double the number of physical pages
    int oldPage = 0; // The page that will be replaced in the swap
    OpenFile* swapFile = NULL;
    BitMap* swapMap = NULL;
    AddrSpace* space = NULL;

};

#endif // SWAP_H
