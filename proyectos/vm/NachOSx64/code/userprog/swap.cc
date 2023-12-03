#include "swap.h"
#include <iostream>

Swap::Swap(int sizeSwap) {
  this->oldPage = 0;
  this->size = sizeSwap;
  this->swapMap = new BitMap(sizeSwap);
  this->swapFile = nullptr;
  this->space = nullptr;
  this->machine = nullptr;
}

Swap::~Swap() {
  delete swapMap;
  this->size = 0;
  this->oldPage = 0;
  this->swapFile = nullptr;
  this->space = nullptr;
  this->machine = nullptr;
}

/*
 * @brief Usin the algorithm FIFO, find a page in main memory to move to the swap
 * @return int , the virtual page number of the page to move to the swap
*/
int Swap::findPageToSwap() {
  // Use this->oldPage to keep track of the page to replace
  for (int i = oldPage; i < this->size; i++) {
    // Validate is valid (in main memory) and dirty (has been modified)
    if (this->space->getPageTable()[i].valid) {
      this->oldPage = i;
      return this->space->getPageTable()[i].virtualPage;
    }
  }
  // ! BETA option, redo
  for (int i = 0; i < oldPage; i++) {
    // Validate is valid (in main memory) and dirty (has been modified)
    if (this->space->getPageTable()[i].valid) {
      this->oldPage = i;
      return this->space->getPageTable()[i].virtualPage;
    }
  }

  return -1;
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
    std::cout << "No free page in swap" << std::endl;
    ASSERT(false);
  }

  //  Validate the page to move is valid (in main memory) and dirty (has been modified)
  // Write the page to the swap

  this->swapFile->WriteAt(&(machine->mainMemory[ppn * PageSize]), PageSize, freePageInSwap * PageSize);
  // Update the page table
  this->space->getPageTable()[vpn].valid = false;
  this->space->getPageTable()[vpn].physicalPage = freePageInSwap;
  this->space->getPageTable()[vpn].dirty = true;
  // Update the swap map
  this->swapMap->Mark(freePageInSwap);
}

/*
 * @brief Move a page from the swap to main memory
 * @param vpn , the virtual page number to move to main memory
 * @param frame, the physical page number to move to main memory
*/
void Swap::movePageToMainMemory(int vpn, int frame) {
  // Page is in the swap and is dirty

  // Read the page from the swap
  this->swapFile->ReadAt(&(machine->mainMemory[frame * PageSize]), 
    PageSize, 
    this->space->getPageTable()[vpn].physicalPage * PageSize);
  
  // Say in the swap File this position is free
  this->swapMap->Clear(this->space->getPageTable()[vpn].physicalPage);
  // Update the page table
  this->space->getPageTable()[vpn].valid = true;
  this->space->getPageTable()[vpn].physicalPage = frame;
  this->space->getPageTable()[vpn].virtualPage = vpn;
  this->space->getPageTable()[vpn].dirty = true; // The page is dirty because it has been modified

}