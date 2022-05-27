//
//  IndexDataStore.hpp
//  NrIO
//
//  Created by Nyhl Rawlings on 23/5/22.
//  Copyright © 2022 Liquidsoft Studio. All rights reserved.
//

#ifndef IndexDataStore_hpp
#define IndexDataStore_hpp

#include <stdio.h>
#include <libnrcore/memory/Ref.h>
#include <libnrcore/memory/Memory.h>
#include <libnrcore/memory/String.h>
#include "File.h"

#define MAGIC_FLAG_INDEX 0xAAAAAAAA
#define MAGIC_FLAG_FILE 0xBBBBBBBB
#define MAGIC_FLAG_DATA 0xCCCCCCCC

namespace nrcore {

    class IndexedDataStore {
    public:
        typedef struct {
            unsigned long magic_flag;
            unsigned char range_start;      // Start of first 16 entries
            unsigned long long next_index_descriptor;
            unsigned long long file;        // File which holds data block for recycling
            unsigned long long slot[16];    // Most significant bit indicates if in use or not
        } INDEX_DESCRIPTOR;
        
        typedef struct {
            unsigned long long offset;
            INDEX_DESCRIPTOR descriptor;
        } LOADED_INDEX_DESCRIPTOR;
        
        typedef struct {
            unsigned long magic_flag;
            unsigned long long first_data_block;
            unsigned long long last_data_block;
            unsigned long long file_size;
            unsigned int block_size;
        } FILE_DESCRIPTOR;
        
        typedef struct {
            unsigned long long offset;
            FILE_DESCRIPTOR descriptor;
        } LOADED_FILE_DESCRIPTOR;
        
        typedef struct {
            unsigned long magic_flag;
            unsigned long long next_data_block;
            unsigned int block_size;
            unsigned int used_bytes;
        } DATA_BLOCK_DESCRIPTOR;
        
        typedef struct {
            unsigned long long offset;
            DATA_BLOCK_DESCRIPTOR descriptor;
            Memory data;
        } LOADED_DATA_BLOCK_DESCRIPTOR;
        
    public:
        IndexedDataStore(String path);
        virtual ~IndexedDataStore();
        
        Ref<LOADED_FILE_DESCRIPTOR> createFile(Memory key, unsigned int block_size);
        Ref<LOADED_FILE_DESCRIPTOR> getFile(Memory key);
        
        Ref<LOADED_FILE_DESCRIPTOR> getOrCreateFile(Memory key, unsigned int block_size);
        
        bool writeToFile(Ref<LOADED_FILE_DESCRIPTOR> file, Memory data, unsigned long long offset, unsigned long long length);
        unsigned long long getFileSize(Ref<LOADED_FILE_DESCRIPTOR> file);
        Memory readFromFile(Ref<LOADED_FILE_DESCRIPTOR> file, unsigned long long offset, unsigned long long length);
        
        Memory readOrSet(Memory key, Memory default_value);
        int readOrSet(Memory key, int default_value);
        unsigned int readOrSet(Memory key, unsigned int default_value);
        long long readOrSet(Memory key, long long default_value);
        unsigned long long readOrSet(Memory key, unsigned long long default_value);
        
    private:
        File file;
        
        Ref<LOADED_INDEX_DESCRIPTOR> getChildDescriptor(Ref<LOADED_INDEX_DESCRIPTOR> descriptor, int index, bool create_index);
        Ref<LOADED_INDEX_DESCRIPTOR> loadIndexDescriptor(unsigned long long offset);
        void updateIndexDescriptor(Ref<LOADED_INDEX_DESCRIPTOR> descriptor);
        
        Ref<LOADED_FILE_DESCRIPTOR> loadFileDescriptor(unsigned long long offset);
        void updateFileDescriptor(Ref<LOADED_FILE_DESCRIPTOR> descriptor);
        
        Ref<LOADED_DATA_BLOCK_DESCRIPTOR> loadDataDescriptor(unsigned long long offset);
        
        Ref<LOADED_DATA_BLOCK_DESCRIPTOR> createDataBlock(Ref<LOADED_DATA_BLOCK_DESCRIPTOR> previous, unsigned int block_size);
        void updateDataBlockDescriptor(Ref<LOADED_DATA_BLOCK_DESCRIPTOR> descriptor);
        
        void updateDataBlockData(Ref<LOADED_DATA_BLOCK_DESCRIPTOR> descriptor);
        
        Ref<LOADED_INDEX_DESCRIPTOR> getRootDecriptor();
        Ref<LOADED_INDEX_DESCRIPTOR> getSystemDecriptor();
        Ref<LOADED_INDEX_DESCRIPTOR> getUserDecriptor();
    };

}

#endif /* IndexDataStore_hpp */
