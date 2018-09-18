//
//  main.cpp
//  UnitTests
//
//  Created by Nyhl Rawlings on 10/09/2018.
//  Copyright © 2018 Liquidsoft Studio. All rights reserved.
//

#include <iostream>
#include "../libnrio/FileStream.h"

using namespace nrcore;

int main(int argc, const char * argv[]) {
    FileStream fs(String("./test.dat"));
    
    fs.seek(0);
    char buf[11];
    buf[10] = 0;
    int ret = fs.write("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 100);
    fs.flush();
    fs.seek(55);
    
    fs.read(buf, 10);
    
    fs.seek(55);
    fs.write("AAAAA", 5);
    fs.flush();
    
    fs.seek(55);
    fs.read(buf, 10);
    
    off_t sz = fs.getfileSize();
    
    fs.close();
    
    return 0;
}
