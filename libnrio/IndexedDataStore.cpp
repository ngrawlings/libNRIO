//
//  IndexDataStore.cpp
//  NrIO
//
//  Created by Nyhl Rawlings on 23/5/22.
//  Copyright © 2022 Liquidsoft Studio. All rights reserved.
//

#include "IndexedDataStore.h"

#include <libnrcore/memory/ByteArray.h>

namespace nrcore {

    IndexedDataStore::IndexedDataStore(String path) : file(path), block_size(4096) {
        if (file.length()==0) {
            INDEX_DESCRIPTOR root_descriptor;
            INDEX_DESCRIPTOR system_descriptor;
            INDEX_DESCRIPTOR user_descriptor;
            FILE_DESCRIPTOR recycled_blocks_file;
            
            memset(&root_descriptor, 0, sizeof(INDEX_DESCRIPTOR));
            memset(&system_descriptor, 0, sizeof(INDEX_DESCRIPTOR));
            memset(&user_descriptor, 0, sizeof(INDEX_DESCRIPTOR));
            memset(&recycled_blocks_file, 0, sizeof(FILE_DESCRIPTOR));
            
            root_descriptor.magic_flag = MAGIC_FLAG_INDEX;
            system_descriptor.magic_flag = MAGIC_FLAG_INDEX;
            user_descriptor.magic_flag = MAGIC_FLAG_INDEX;
            
            root_descriptor.slot[0] = sizeof(INDEX_DESCRIPTOR);   // Offset of system descriptor
            root_descriptor.slot[1] = sizeof(INDEX_DESCRIPTOR)*2; // Offset of user descriptor
            root_descriptor.file = sizeof(INDEX_DESCRIPTOR)*3;
            
            file.write(0, (const char*)&root_descriptor, sizeof(INDEX_DESCRIPTOR));
            file.write(sizeof(INDEX_DESCRIPTOR), (const char*)&system_descriptor, sizeof(INDEX_DESCRIPTOR));
            file.write(sizeof(INDEX_DESCRIPTOR)*2, (const char*)&user_descriptor, sizeof(INDEX_DESCRIPTOR));
            file.write(sizeof(INDEX_DESCRIPTOR)*3, (const char*)&recycled_blocks_file, sizeof(FILE_DESCRIPTOR));
        }
    }

    IndexedDataStore::~IndexedDataStore() {
        
    }

    void IndexedDataStore::setBlockSize(unsigned int block_size) {
        this->block_size = block_size;
    }

    Ref<IndexedDataStore::LOADED_FILE_DESCRIPTOR> IndexedDataStore::createFile(Memory key) {
        Ref<IndexedDataStore::LOADED_INDEX_DESCRIPTOR> desc = getUserDecriptor();
        
        for (int i=0; i<key.length(); i++) {
            desc = getChildDescriptor(desc, key.getPtr()[i]);
            if (!desc.getPtr())
                throw "Failed to get file key";
        }
        
        if (desc.getPtr()->descriptor.file)
            throw "File already exists";
        
        FILE_DESCRIPTOR file_desc;
        memset(&file_desc, 0, sizeof(FILE_DESCRIPTOR));
        file_desc.magic_flag = MAGIC_FLAG_FILE;
        unsigned long long offset = file.length();
        file.write(offset, (const char*)&file_desc, sizeof(FILE_DESCRIPTOR));
        
        desc.getPtr()->descriptor.file = offset;
        updateIndexDescriptor(desc);
        
        return loadFileDescriptor(offset);
    }

    Ref<IndexedDataStore::LOADED_FILE_DESCRIPTOR> IndexedDataStore::getFile(Memory key) {
        Ref<IndexedDataStore::LOADED_INDEX_DESCRIPTOR> desc = getUserDecriptor();
        
        for (int i=0; i<key.length(); i++) {
            desc = getChildDescriptor(desc, key.getPtr()[i]);
            if (!desc.getPtr())
                throw "Failed to get file key";
        }
        
        if (desc.getPtr()->descriptor.file)
            return loadFileDescriptor(desc.getPtr()->descriptor.file);
        
        return Ref<LOADED_FILE_DESCRIPTOR>();
    }

    bool IndexedDataStore::writeToFile(Ref<LOADED_FILE_DESCRIPTOR> file, Memory data, unsigned long long offset, unsigned long long length) {
        unsigned long long file_size = getFileSize(file);
        if (offset > file_size)
            return false;
        
        if (!file.getPtr()->descriptor.first_data_block) {
            // Create First data block must be done manually
            DATA_BLOCK_DESCRIPTOR desc;
            memset(&desc, 0, sizeof(DATA_BLOCK_DESCRIPTOR));
            desc.magic_flag = MAGIC_FLAG_DATA;
            desc.block_size = block_size;
            
            unsigned long long file_offset = this->file.length();
            this->file.write(file_offset, (const char*)&desc, sizeof(DATA_BLOCK_DESCRIPTOR));
            
            Memory mem(block_size);
            for (int i=0; i<block_size; i++) {
                mem.getPtr()[i] = 0;
            }
            this->file.write(file_offset+sizeof(DATA_BLOCK_DESCRIPTOR), mem.operator char *(), block_size);
            
            file.getPtr()->descriptor.first_data_block = file_offset;
            //updateFileDescriptor(file); // Seems to be overwritten past the point of its end
        }
        
        Ref<LOADED_DATA_BLOCK_DESCRIPTOR> desc = loadDataDescriptor(file.getPtr()->descriptor.first_data_block);
        unsigned long long block_offset = 0;
        int cursor = 0;
        unsigned long long written = 0;

        // Get block where offset resides
        while(desc.getPtr() && desc.getPtr()->descriptor.used_bytes == desc.getPtr()->descriptor.block_size && (desc.getPtr()->descriptor.used_bytes+block_offset) <= offset) {
            if (desc.getPtr()->descriptor.next_data_block) {
                desc = loadDataDescriptor(desc.getPtr()->descriptor.next_data_block);
                block_offset += desc.getPtr()->descriptor.block_size;
            } else {
                desc = createDataBlock(desc, block_size);
                block_offset += block_size;
            }
        }
            
        if (!desc.getPtr())
            throw "Reached EOF before offset was located";
            
        if (offset > block_offset && offset < block_offset+desc.getPtr()->descriptor.block_size)
            cursor = int(offset-block_offset);
        
        while (written < length) {
            unsigned long long len = desc.getPtr()->descriptor.block_size - cursor;
            
            if (length-written < len)
                len = length-written;
            
            for (unsigned long long i=0; i<len; i++) {
                desc.getPtr()->data.getPtr()[cursor++] = data.getPtr()[written++];
            }
            
            if (cursor > desc.getPtr()->descriptor.used_bytes) {
                desc.getPtr()->descriptor.used_bytes = cursor;
                updateDataBlockDescriptor(desc);
            }
            
            updateDataBlockData(desc);
            
            if (written<length) {
                block_offset += desc.getPtr()->descriptor.block_size;
                
                if (desc.getPtr()->descriptor.next_data_block) {
                    desc = loadDataDescriptor(desc.getPtr()->descriptor.next_data_block);
                } else {
                    desc = createDataBlock(desc, block_size);
                }
            }
            cursor = 0;
        }
        
        if (offset+length > file_size) {
            file.getPtr()->descriptor.file_size = offset+length;
            updateFileDescriptor(file);
        }
        
        return written == length;
    }

    unsigned long long IndexedDataStore::getFileSize(Ref<LOADED_FILE_DESCRIPTOR> file) {
        return file.getPtr()->descriptor.file_size;
    }

    Memory IndexedDataStore::readFromFile(Ref<LOADED_FILE_DESCRIPTOR> file, unsigned long long offset, unsigned long long length) {
        ByteArray ret;
        
        unsigned long long file_size = getFileSize(file);
        if (offset > file_size)
            return ret;
        
        Ref<LOADED_DATA_BLOCK_DESCRIPTOR> desc = loadDataDescriptor(file.getPtr()->descriptor.first_data_block);
        int cursor = 0;
        
        // Get block where offset resides
        while(desc.getPtr() && desc.getPtr()->descriptor.used_bytes == desc.getPtr()->descriptor.block_size && (desc.getPtr()->descriptor.used_bytes+cursor) <= offset) {
            if (desc.getPtr()->descriptor.next_data_block) {
                desc = loadDataDescriptor(desc.getPtr()->descriptor.next_data_block);
                cursor += desc.getPtr()->descriptor.used_bytes;
            } else {
                return ret;
            }
        }
        
        unsigned int _offset = (int)(offset-cursor);
        unsigned int len = desc.getPtr()->descriptor.used_bytes-_offset;
        
        while (ret.length()<length) {
            ByteArray block = desc.getPtr()->data;
            ret.append(block.subBytes(_offset, len));
            
            if (!desc.getPtr()->descriptor.next_data_block)
                break;
            
            desc = loadDataDescriptor(desc.getPtr()->descriptor.next_data_block);
            
            _offset = 0;
            len = desc.getPtr()->descriptor.used_bytes;
            if (ret.length()+len > length)
                len = (unsigned int)(length-ret.length());
        }
        
        return ret;
    }

    Ref<IndexedDataStore::LOADED_INDEX_DESCRIPTOR> IndexedDataStore::getChildDescriptor(Ref<LOADED_INDEX_DESCRIPTOR> descriptor, int index) {
        do {
            if (descriptor.getPtr()->descriptor.range_start <= index && descriptor.getPtr()->descriptor.range_start+16 > index) {
                unsigned long long offset = descriptor.getPtr()->descriptor.slot[index-descriptor.getPtr()->descriptor.range_start];
                if ((offset & 0x8000000000000000) == 0x8000000000000000) {
                    // return existing descriptor
                    return loadIndexDescriptor(offset & 0x7FFFFFFFFFFFFFFF);
                } else {
                    // Create new descriptor
                    INDEX_DESCRIPTOR ndesc;
                    memset(&ndesc, 0, sizeof(INDEX_DESCRIPTOR));
                    ndesc.magic_flag = MAGIC_FLAG_INDEX;
                    
                    unsigned long long offset = file.length();
                    file.write(offset, (const char*)&ndesc, sizeof(INDEX_DESCRIPTOR));
                    
                    descriptor.getPtr()->descriptor.slot[index-descriptor.getPtr()->descriptor.range_start] = offset | 0x8000000000000000;
                    updateIndexDescriptor(descriptor);
                    
                    return loadIndexDescriptor(offset & 0x7FFFFFFFFFFFFFFF);
                }
            }
        } while (descriptor.getPtr()->descriptor.next_index_descriptor);
        
        // child descriptor range does not exist, create
        INDEX_DESCRIPTOR sibling_descriptor;
        memset(&sibling_descriptor, 0, sizeof(INDEX_DESCRIPTOR));
        sibling_descriptor.magic_flag = MAGIC_FLAG_INDEX;
        sibling_descriptor.range_start = index-(16-(index%16));
        
        unsigned long long offset = file.length();
        file.write(offset, (const char*)&sibling_descriptor, sizeof(INDEX_DESCRIPTOR));
        
        descriptor.getPtr()->descriptor.next_index_descriptor = offset;
        updateIndexDescriptor(descriptor);
        
        descriptor = loadIndexDescriptor(offset);
        if (descriptor.getPtr()) {
            INDEX_DESCRIPTOR ndesc;
            memset(&ndesc, 0, sizeof(INDEX_DESCRIPTOR));
            ndesc.magic_flag = MAGIC_FLAG_INDEX;
            
            unsigned long long offset = file.length();
            file.write(offset, (const char*)&ndesc, sizeof(INDEX_DESCRIPTOR));
            
            descriptor.getPtr()->descriptor.slot[index-descriptor.getPtr()->descriptor.range_start] = offset | 0x8000000000000000;
            updateIndexDescriptor(descriptor);
            
            return loadIndexDescriptor(offset & 0x7FFFFFFFFFFFFFFF);
        }
        
        return Ref<LOADED_INDEX_DESCRIPTOR>();
    }

    Ref<IndexedDataStore::LOADED_INDEX_DESCRIPTOR> IndexedDataStore::loadIndexDescriptor(unsigned long long offset) {
        Memory mem = file.read(offset, sizeof(INDEX_DESCRIPTOR));
        
        LOADED_INDEX_DESCRIPTOR *desc = new LOADED_INDEX_DESCRIPTOR;
        memcpy(&desc->descriptor, mem.operator char *(), sizeof(INDEX_DESCRIPTOR));
        
        if (desc->descriptor.magic_flag != MAGIC_FLAG_INDEX)
            throw "Invalid index descriptor";
        
        desc->offset = offset;
        
        return Ref<LOADED_INDEX_DESCRIPTOR>(desc);
    }

    Ref<IndexedDataStore::LOADED_FILE_DESCRIPTOR> IndexedDataStore::loadFileDescriptor(unsigned long long offset) {
        Memory mem = file.read(offset, sizeof(INDEX_DESCRIPTOR));
        
        LOADED_FILE_DESCRIPTOR *desc = new LOADED_FILE_DESCRIPTOR;
        memcpy(&desc->descriptor, mem.operator char *(), sizeof(FILE_DESCRIPTOR));
        
        if (desc->descriptor.magic_flag != MAGIC_FLAG_FILE)
            throw "Invalid file descriptor";
        
        desc->offset = offset;
        
        return Ref<LOADED_FILE_DESCRIPTOR>(desc);
    }

    Ref<IndexedDataStore::LOADED_DATA_BLOCK_DESCRIPTOR> IndexedDataStore::loadDataDescriptor(unsigned long long offset) {
        Memory mem = file.read(offset, sizeof(DATA_BLOCK_DESCRIPTOR));
        
        LOADED_DATA_BLOCK_DESCRIPTOR *desc = new LOADED_DATA_BLOCK_DESCRIPTOR;
        memcpy(&desc->descriptor, mem.operator char *(), sizeof(DATA_BLOCK_DESCRIPTOR));
        
        if (desc->descriptor.magic_flag != MAGIC_FLAG_DATA)
            throw "Invalid data descriptor";
        
        desc->offset = offset;
        desc->data = file.read(offset+sizeof(DATA_BLOCK_DESCRIPTOR), desc->descriptor.block_size);
        
        return Ref<LOADED_DATA_BLOCK_DESCRIPTOR>(desc);
    }

    void IndexedDataStore::updateIndexDescriptor(Ref<LOADED_INDEX_DESCRIPTOR> descriptor) {
        file.write(descriptor.getPtr()->offset, (const char*)&descriptor.getPtr()->descriptor, sizeof(INDEX_DESCRIPTOR));
    }

    Ref<IndexedDataStore::LOADED_DATA_BLOCK_DESCRIPTOR> IndexedDataStore::createDataBlock(Ref<LOADED_DATA_BLOCK_DESCRIPTOR> previous, unsigned int block_size) {
        DATA_BLOCK_DESCRIPTOR desc;
        memset(&desc, 0, sizeof(DATA_BLOCK_DESCRIPTOR));
        desc.magic_flag = MAGIC_FLAG_DATA;
        desc.block_size = block_size;
        
        unsigned long long file_offset = file.length();
        file.write(file_offset, (const char*)&desc, sizeof(DATA_BLOCK_DESCRIPTOR));
        
        Memory mem(block_size);
        for (int i=0; i<block_size; i++) {
            mem.getPtr()[i] = 0;
        }
        this->file.write(file_offset+sizeof(DATA_BLOCK_DESCRIPTOR), mem.operator char *(), block_size);
        
        previous.getPtr()->descriptor.next_data_block = file_offset;
        updateDataBlockDescriptor(previous);
        
        return loadDataDescriptor(file_offset);
    }
        
    void IndexedDataStore::updateFileDescriptor(Ref<LOADED_FILE_DESCRIPTOR> descriptor) {
        file.write(descriptor.getPtr()->offset, (const char*)&descriptor.getPtr()->descriptor, sizeof(FILE_DESCRIPTOR));
    }

    void IndexedDataStore::updateDataBlockDescriptor(Ref<LOADED_DATA_BLOCK_DESCRIPTOR> descriptor) {
        file.write(descriptor.getPtr()->offset, (const char*)&descriptor.getPtr()->descriptor, sizeof(DATA_BLOCK_DESCRIPTOR));
    }

    void IndexedDataStore::updateDataBlockData(Ref<LOADED_DATA_BLOCK_DESCRIPTOR> descriptor) {
        unsigned long long offset = descriptor.getPtr()->offset+sizeof(DATA_BLOCK_DESCRIPTOR);
        int len = descriptor.getPtr()->descriptor.block_size;
        
        file.write(offset, descriptor.getPtr()->data.operator char *(), len);
    }

    Ref<IndexedDataStore::LOADED_INDEX_DESCRIPTOR> IndexedDataStore::getRootDecriptor() {
        return loadIndexDescriptor(0);
    }

    Ref<IndexedDataStore::LOADED_INDEX_DESCRIPTOR> IndexedDataStore::getSystemDecriptor() {
        Ref<LOADED_INDEX_DESCRIPTOR> root = getRootDecriptor();
        return loadIndexDescriptor(root.getPtr()->descriptor.slot[0]);
    }

    Ref<IndexedDataStore::LOADED_INDEX_DESCRIPTOR> IndexedDataStore::getUserDecriptor() {
        Ref<LOADED_INDEX_DESCRIPTOR> root = getRootDecriptor();
        return loadIndexDescriptor(root.getPtr()->descriptor.slot[1]);
    }

}
