/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** @file Image file abstraction for hard drive images */

#ifndef METAIMGFILE_H
#define METAIMGFILE_H

#include <thirdparty/Apple/AppleDiskPartitions.h>
#include <utils/imgfile.h>
#include <vector>

class MetaChunk {
public:
    MetaChunk();
    virtual ~MetaChunk();
    virtual uint64_t read(void* buf, uint64_t offset, uint64_t length) const = 0;
    virtual uint64_t write(const void* buf, uint64_t offset, uint64_t length) = 0;
    void set_chunk(uint64_t src_offset, uint64_t begin, uint64_t size);
    void set_chunk_begin(uint64_t begin);
    void set_chunk_size(uint64_t size);
    void add_src_offset(uint64_t src_offset);
    uint64_t chunk_begin = 0;
    uint64_t chunk_end = 0;
    uint64_t src_offset = 0;
};

class FileMetaChunk : public MetaChunk {
public:
    FileMetaChunk(const std::string& img_path);
    virtual ~FileMetaChunk();
    virtual uint64_t read(void* buf, uint64_t offset, uint64_t length) const override;
    virtual uint64_t write(const void* buf, uint64_t offset, uint64_t length) override;
    ImgFile image;
    bool is_opened = false;
    uint64_t image_size = 0;
};

class RamMetaChunk : public MetaChunk {
public:
    RamMetaChunk(std::unique_ptr<uint8_t[]> bytes, uint64_t size);
    virtual ~RamMetaChunk();
    virtual uint64_t read(void* buf, uint64_t offset, uint64_t length) const override;
    virtual uint64_t write(const void* buf, uint64_t offset, uint64_t length) override;
    std::unique_ptr<uint8_t[]> bytes;
};

class MetaImgFile {
public:
    MetaImgFile();
    ~MetaImgFile();

    bool open(const std::string& img_path);
    void close();

    uint64_t size() const;

    uint64_t read(void* buf, uint64_t offset, uint64_t length) const;
    uint64_t write(const void* buf, uint64_t offset, uint64_t length);
private:
    std::vector<std::unique_ptr<MetaChunk>> chunks;
    void add_partitions_from_image(std::unique_ptr<FileMetaChunk> file);
    void add_partition_from_image(std::unique_ptr<FileMetaChunk> file, std::string partition_type, std::string volume_name);
    void reserve_partitions(int partitions_to_reserve);
    void add_partition(Partition &new_part); // host endian
    void append_chunk(std::unique_ptr<FileMetaChunk> file);

    void dump();
    void dump_partition(Partition &part, int index, int num_partitions);

    //void ensure_default_partitions();
    //bool default_partitions = false;

    uint64_t meta_image_size = 0;
    uint32_t block_size = 512;
    uint32_t used_partitions = 0;
    uint32_t max_partitions = 0;
    uint32_t min_unused_partitions = 8;
    uint32_t partitions_increment = 64;
};
#endif // METAIMGFILE_H
