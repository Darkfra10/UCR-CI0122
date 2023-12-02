#ifndef NACHOSOPENFILESTABLE_H
#define NACHOSOPENFILESTABLE_H

#define MAX_OPEN_FILES 32
#include <iostream>

#include "bitmap.h"

class NachosOpenFilesTable {
  public:
    NachosOpenFilesTable();
    ~NachosOpenFilesTable();
    
    int Open( int UnixHandle );
    int Close( int NachosHandle );
    bool isOpened( int NachosHandle );
    int getUnixHandle( int NachosHandle );
    int getUsage();
    void addThread();
    void delThread();

    void Print();
    
    BitMap * openFilesMap;
  private:
    int * openFiles;
    int usage;
};

#endif // NACHOSOPENFILESTABLE_H