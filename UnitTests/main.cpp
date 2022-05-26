//
//  main.cpp
//  UnitTests
//
//  Created by Nyhl Rawlings on 10/09/2018.
//  Copyright © 2018 Liquidsoft Studio. All rights reserved.
//

#include <iostream>
#include "../libnrio/IndexedDataStore.h"

#include <libnrcore/memory/ByteArray.h>

using namespace nrcore;

int main(int argc, const char * argv[]) {
    IndexedDataStore *data = new IndexedDataStore("./index_data_Store_test.dat");
    
    data->setBlockSize(32);
    
    Ref<IndexedDataStore::LOADED_FILE_DESCRIPTOR> file1 = data->getFile(Memory(ByteArray::fromHex("0102"), 2));
    Ref<IndexedDataStore::LOADED_FILE_DESCRIPTOR> file2 = data->getFile(Memory(ByteArray::fromHex("010203"), 3));
    
    if (!file1.getPtr())
        file1 = data->createFile(Memory(ByteArray::fromHex("0102"), 2));
    
    if (!file2.getPtr())
        file2 = data->createFile(Memory(ByteArray::fromHex("010203"), 3));
        
    data->writeToFile(file1, Memory(ByteArray::fromHex("000102030405060708090A0B0C0D0E0F"), 16), 0, 16);
    data->writeToFile(file1, Memory(ByteArray::fromHex("000102030405060708090A0B0C0D0E0F"), 16), 16, 16);
    data->writeToFile(file1, Memory(ByteArray::fromHex("000102030405060708090A0B0C0D0E0F"), 16), 32, 16);
    data->writeToFile(file1, Memory(ByteArray::fromHex("000102030405060708090A0B0C0D0E0F"), 16), 48, 16);
    data->writeToFile(file1, Memory(ByteArray::fromHex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"), 16), 40, 16);
    data->writeToFile(file1, Memory(ByteArray::fromHex("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE"), 16), 56, 16);
    
    Memory mem = data->readFromFile(file1, 0, 72);
    
    printf("%s\n", mem.toHex(false).operator char *());
    
    mem = data->readFromFile(file1, 16, 16);
    
    printf("%s\n", mem.toHex(false).operator char *());
    
    
    data->writeToFile(file2, Memory(ByteArray::fromHex("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), 16), 0, 16);
    
    mem = data->readFromFile(file2, 0, 16);
    
    printf("%s\n", mem.toHex(false).operator char *());
    
    delete data;
    
    return 0;
}
