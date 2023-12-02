#ifndef NACHOS_OPEN_FILES_TABLE_H
#define NACHOS_OPEN_FILES_TABLE_H

#define MAX_OPEN_FILES 32
#include <iostream>

#include "bitmap.h"

class NachosOpenFilesTable {
  public:
    NachosOpenFilesTable();
    ~NachosOpenFilesTable();
    
    // Register the file handle
    int Open( int UnixHandle );

    // Unregister the file handle
    int Close( int NachosHandle );

    // Return true if the file handle is registered, false otherwise
    bool isOpened( int NachosHandle );

    // Return the UNIX file handle associated to the Nachos file handle
    int getUnixHandle( int NachosHandle );

    // Return the number of threads using this table
    int getUsage();

    // Return the number of threads using this table
    void addThread();

    // Return the number of threads using this table
    void delThread();

    void Print();
    
    BitMap * openFilesMap;
  private:
    int * openFiles; // Vector of open files
    int usage; // Number of threads using this table
};

#endif // NACHOSOPENFILESTABLE_H