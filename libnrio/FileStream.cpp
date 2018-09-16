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
    
    FileStream::FileStream(const FileStream& fs) : Stream(fs) {
    }
    
    FileStream::~FileStream() {
        
    }
    
    void FileStream::seek(off_t position) {
        lseek(fd, position, SEEK_SET);
    }
    
    off_t FileStream::position() {
        return lseek(fd, 0, SEEK_CUR);
    }
    
    size_t FileStream::write(const char* buf, size_t sz) {
        return Stream::write(buf, sz);
    }
    
    size_t FileStream::read(char* buf, size_t sz) {
        return Stream::read(buf, sz);
    }
    
    off_t FileStream::getfileSize() {
        off_t position = lseek(fd, 0, SEEK_CUR);
        off_t end = lseek(fd, 0, SEEK_END);
        lseek(fd, position, SEEK_SET);
        return end;
    }
    
}
