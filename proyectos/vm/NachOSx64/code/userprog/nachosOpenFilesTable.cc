#include "nachosOpenFilesTable.h"

NachosOpenFilesTable::NachosOpenFilesTable() {
  // MAX_OPEN_FILES
  openFiles = new int[MAX_OPEN_FILES];
  memset(this->openFiles, -1, MAX_OPEN_FILES);
  openFiles[0] = 0;
  openFiles[1] = 1;
  openFiles[2] = 2;
  openFilesMap = new BitMap(MAX_OPEN_FILES);
  openFilesMap->Mark(0);
  openFilesMap->Mark(1);
  openFilesMap->Mark(2);
  this->usage = 1;
} // Initialize

NachosOpenFilesTable::~NachosOpenFilesTable() {
  delete openFiles;
  delete openFilesMap;
} // De-allocate

int NachosOpenFilesTable::Open( int UnixHandle ) {
  int handle = this->openFilesMap->Find();
  
  if (handle == -1) {
    return -1;
  }

  this->openFiles[handle] = UnixHandle;
  return handle;
}                    // Register the file handle

int NachosOpenFilesTable::Close( int NachosHandle ) {
  if (isOpened(NachosHandle)) {
    int UnixHandle = this->openFiles[NachosHandle];
    openFilesMap->Clear(NachosHandle);
    this->openFiles[NachosHandle] = -1;
    return UnixHandle;
  } else {
    return -1;
  }    
}                    // Unregister the file handle

bool NachosOpenFilesTable::isOpened( int NachosHandle ) {
  return openFilesMap->Test(NachosHandle);	
}
int NachosOpenFilesTable::getUnixHandle( int NachosHandle ) {
  return openFiles[NachosHandle];
}
void NachosOpenFilesTable::addThread() {
  usage++;
}                    // If a user thread is using this table, add it
void NachosOpenFilesTable::delThread() {
  usage--;
}                    // If a user thread is using this table, delete it

int NachosOpenFilesTable::getUsage() {
  return this->usage;
}

void NachosOpenFilesTable::Print(){
  for(int i=0; i< MAX_OPEN_FILES; i++) {
    if (this->isOpened(i)) {
      printf("%d\n", openFiles[i]);
    }
  }
} 
