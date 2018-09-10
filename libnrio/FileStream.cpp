//
//  FileStream.cpp
//  NrIO
//
//  Created by Nyhl Rawlings on 10/09/2018.
//  Copyright © 2018 Liquidsoft Studio. All rights reserved.
//

#include "FileStream.h"



namespace nrcore {
    
    FileStream::FileStream(int fd) : Stream(fd) {
        
    }
    
    FileStream::FileStream(String filename) : Stream(0) {
        FILE *pFile;
        pFile = fopen ((char*)filename, "w+");
        fd = fileno(pFile);
    }
    
    FileStream::~FileStream() {
        
    }
    
    void FileStream::seek(off_t position) {
        this->position = lseek(fd, position, SEEK_SET);
    }
    
    ssize_t FileStream::write(const char* buf, size_t sz) {
        ssize_t ret = Stream::write(buf, sz);
        this->position += ret;
        return ret;
    }
    
    ssize_t FileStream::read(char* buf, size_t sz) {
        ssize_t ret = Stream::read(buf, sz);
        this->position += ret;
        return ret;
    }
    
}
