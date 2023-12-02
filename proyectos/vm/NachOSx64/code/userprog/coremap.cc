#include "coremap.h"

Coremap::Coremap(int numPages) {
  this->numPages = numPages;
  this->coremap = new int[numPages];
  this->fifo_to_replace = 0;
  for(int i = 0; i < numPages; i++){
    this->coremap[i] = -1;
  }
}

Coremap::~Coremap() {
  delete[] this->coremap;
}

int Coremap::find_space(int vpn) {
  for(int i = 0; i < this->numPages; i++){
    if(this->coremap[i] == -1){
      this->coremap[i] = vpn;
      return i;
    }
  }
  return -1;
}

int Coremap::find_page_replace(int vpn, int* swaped) {
  int i = this->fifo_to_replace;
  *swaped = this->coremap[i];
  this->coremap[i] = vpn;
  this->fifo_to_replace = (this->fifo_to_replace + 1) % this->numPages;
  return i;
}
