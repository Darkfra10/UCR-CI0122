#include "swap.h"

Swap::Swap(){
  this->size = NumPhysPages * 2; // 2 * 32 = 64 pages
  this->swapFile = fileSystem->Open("Swap");
  this->swapMap = new BitMap(size);
}

Swap::~Swap() {
  fileSystem->Remove("Swap");
  delete swapMap;
}

/*
 * @brief Usin the algorithm FIFO, find a page in main memory to move to the swap
 * @return int , the virtual page number of the page to move to the swap
*/
int Swap::findPageToSwap() {
  // Use this->oldPage to keep track of the page to replace
  for (int i = oldPage; i < NumPhysPages; i++) {
    // Validate is valid (in main memory) and dirty (has been modified)
    if (machine->pageTable[i].valid && machine->pageTable[i].dirty) {
      this->oldPage = i;
      return machine->pageTable[i].virtualPage;
    }
  }
}

/*
 * @brief Find a free page in the swap
 * @return int , the physical page number of the free page in the swap
*/
int Swap::findFreePageInSwap() {
  return swapMap->Find();
}

/*
 * @brief Move a page from main memory to the swap
 * @param vpn , the virtual page number of the page to move to the swap
 * @param ppn , the physical page number of the page to move to the swap
*/
void Swap::movePageToSwap(int vpn, int ppn) {
  int freePageInSwap = this->findFreePageInSwap();
  if (-1 == freePageInSwap) {
    printf("No free page in swap\n");
    ASSERT(false);
  }

  //  Validate the page to move is valid (in main memory) and dirty (has been modified)
  if (this->space->pageTable[vpn].valid && this->space->pageTable[vpn].dirty) {
    // Write the page to the swap
    this->swapFile->WriteAt(&(machine->mainMemory[ppn * PageSize]), PageSize, freePageInSwap * PageSize);
    // Update the page table
    this->space->pageTable[vpn].valid = false;
    this->space->pageTable[vpn].physicalPage = freePageInSwap;
    this->space->pageTable[vpn].dirty = false;
    // Update the swap map
    this->swapMap->Mark(freePageInSwap);
  }
  
}