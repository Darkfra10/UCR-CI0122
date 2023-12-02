#include "nachosOpenFilesTable.h"


/**
 * @brief Constructor of the class NachosOpenFilesTable
*/
NachosOpenFilesTable::NachosOpenFilesTable() {

  // Vector of open files
  openFiles = new int[MAX_OPEN_FILES];
  memset(this->openFiles, -1, MAX_OPEN_FILES);
  openFiles[0] = 0;
  openFiles[1] = 1;
  openFiles[2] = 2;

  // BitMap of the size of the maximum number of files
  openFilesMap = new BitMap(MAX_OPEN_FILES);
  openFilesMap->Mark(0); // stdin
  openFilesMap->Mark(1); // stdout
  openFilesMap->Mark(2); // stderr

  this->usage = 1; // The main thread is using this table
} // Initialize

NachosOpenFilesTable::~NachosOpenFilesTable() {
  delete openFiles;
  delete openFilesMap;
}

/**
 * @brief Open a file and return the file handle
 * @param UnixHandle Unix handle of the file
 * @return The file handle
*/
int NachosOpenFilesTable::Open( int UnixHandle ) {
  // Find the first free position in the vector of open files
  int handle = this->openFilesMap->Find();

  if (handle == -1) { // No free position
    return -1;
  }

  // Register the file handle
  this->openFiles[handle] = UnixHandle;
  return handle;
}

int NachosOpenFilesTable::Close( int NachosHandle ) {
  if (!isOpened(NachosHandle)) {
    return -1;
  }

  int UnixHandle = this->openFiles[NachosHandle];
  // Clear the entry for the file handle in the bitmap
  openFilesMap->Clear(NachosHandle);

  // Unregister the file handle
  this->openFiles[NachosHandle] = -1;
  return UnixHandle;
}

bool NachosOpenFilesTable::isOpened( int NachosHandle ) {
  // Check if the file handle is in the bitmap
  return openFilesMap->Test(NachosHandle);	
}

int NachosOpenFilesTable::getUnixHandle( int NachosHandle ) {
  if (!isOpened(NachosHandle)) {
    return -1;
  }
  return openFiles[NachosHandle];
}

void NachosOpenFilesTable::addThread() {
  usage++;
}

void NachosOpenFilesTable::delThread() {
  usage--;
}

int NachosOpenFilesTable::getUsage() {
  return this->usage;
}

void NachosOpenFilesTable::Print(){
  std::cout << "NachosOpenFilesTable contents:" << std::endl;
  std::cout << "Usage: " << this->usage << std::endl;
  std::cout << "Open files:" << std::endl;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (isOpened(i)) {
      std::cout << "NachosHandle: " << i << " UnixHandle: " << getUnixHandle(i) << std::endl;
    }
  }
} 
