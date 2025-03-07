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

#include <utils/metaimgfile.h>
#include <memaccess.h>
#include <loguru.hpp>

#include <fstream>
#include <limits>

// ================================================================================================
std::string get_pascal_string(void * pstr)
{
    return std::string((char*)&(((uint8_t*)pstr)[1]), ((uint8_t*)pstr)[0]);
}

std::string get_part_string(void * pstr, int pstr_size)
{
    return std::string((char*)pstr, std::min(pstr_size, (int)std::strlen((char*)pstr)));
}

void set_pascal_string(void * pstr, int pstr_size, std::string str)
{
    int len = std::min(pstr_size - 1, (int)str.length());
    ((char*)pstr)[0] = len;
    std::strncpy((char*)pstr + 1, str.c_str(), pstr_size - 1);
}

void set_part_string(void * pstr, int pstr_size, std::string str)
{
    std::strncpy((char*)pstr, str.c_str(), pstr_size);
}

void Partition_endian_swap(Partition &part)
{
    part.pmSig         = BYTESWAP_16(part.pmSig);
    part.pmSigPad      = BYTESWAP_16(part.pmSigPad);
    part.pmMapBlkCnt   = BYTESWAP_32(part.pmMapBlkCnt);
    part.pmPyPartStart = BYTESWAP_32(part.pmPyPartStart);
    part.pmPartBlkCnt  = BYTESWAP_32(part.pmPartBlkCnt);
    part.pmLgDataStart = BYTESWAP_32(part.pmLgDataStart);
    part.pmDataCnt     = BYTESWAP_32(part.pmDataCnt);
    part.pmPartStatus  = BYTESWAP_32(part.pmPartStatus);
    part.pmLgBootStart = BYTESWAP_32(part.pmLgBootStart);
    part.pmBootSize    = BYTESWAP_32(part.pmBootSize);
    part.pmBootAddr    = BYTESWAP_32(part.pmBootAddr);
    part.pmBootAddr2   = BYTESWAP_32(part.pmBootAddr2);
    part.pmBootEntry   = BYTESWAP_32(part.pmBootEntry);
    part.pmBootEntry2  = BYTESWAP_32(part.pmBootEntry2);
    part.pmBootCksum   = BYTESWAP_32(part.pmBootCksum);
}

void Partition_BE_to_host(Partition &part)
{
    #ifdef __LITTLE_ENDIAN__
        Partition_endian_swap(part);
    #endif
}

void Partition_host_to_BE(Partition &part)
{
    #ifdef __LITTLE_ENDIAN__
        Partition_endian_swap(part);
    #endif
}

void Block0_endian_swap(Block0 &block0)
{
    int sb_driver_count = 0;
    int sb_block_size = 0;
    int max_dd_maps = 0;

    for (int pass = 0; pass < 2; pass++) {
        if (block0.sbSig == sbSIGWord) {
            sb_driver_count = block0.sbDrvrCount;
            sb_block_size = block0.sbBlkSize;
            max_dd_maps = (sb_block_size - offsetof(Block0, ddBlock)) / sizeof(DDMap);
            if (sb_driver_count > max_dd_maps) {
                LOG_F(ERROR, "MetaImgFile: sbDrvrCount (%d) is too big (%d).", sb_driver_count, max_dd_maps);
                block0.sbDrvrCount = max_dd_maps;
            }
        }

        if (pass == 0) {
            block0.sbSig       = BYTESWAP_16(block0.sbSig);
            block0.sbBlkSize   = BYTESWAP_16(block0.sbBlkSize);
            block0.sbBlkCount  = BYTESWAP_32(block0.sbBlkCount);
            block0.sbDevType   = BYTESWAP_16(block0.sbDevType);
            block0.sbDevId     = BYTESWAP_16(block0.sbDevId);
            block0.sbData      = BYTESWAP_32(block0.sbData);
            block0.sbDrvrCount = BYTESWAP_16(block0.sbDrvrCount);
        }
    }

    DDMap *ddmap = (DDMap *)&block0.ddBlock;

    for (int i = 0; i < sb_driver_count; i++) {
        ddmap->ddBlock = BYTESWAP_32(ddmap->ddBlock);
        ddmap->ddSize  = BYTESWAP_16(ddmap->ddSize);
        ddmap->ddType  = BYTESWAP_16(ddmap->ddType);
        ddmap++;
    }
}

void Block0_BE_to_host(Block0 &block0)
{
    #ifdef __LITTLE_ENDIAN__
        Block0_endian_swap(block0);
    #endif
}

void Block0_host_to_BE(Block0 &block0)
{
    #ifdef __LITTLE_ENDIAN__
        Block0_endian_swap(block0);
    #endif
}

bool not_zero(void *start, size_t len)
{
    uint8_t *p = (uint8_t *)start;
    while (len > 0) {
        if (*p)
            return true;
        len--;
        p++;
    }
    return false;
}

// ================================================================================================

MetaChunk::MetaChunk()
{
    LOG_F(5, "MetaChunk::MetaChunk");
}

MetaChunk::~MetaChunk() {
    LOG_F(5, "MetaChunk::~MetaChunk: begin:%lld end:%lld", this->chunk_begin, this->chunk_end);
}

void MetaChunk::set_chunk_begin(uint64_t begin)
{
    LOG_F(5, "MetaChunk::set_chunk_begin: begin:%lld", begin);
    this->chunk_end = begin + this->chunk_end - this->chunk_begin;
    this->chunk_begin = begin;
}

void MetaChunk::set_chunk_size(uint64_t size)
{
    LOG_F(5, "MetaChunk::set_chunk_size: size:%lld", size);
    this->chunk_end = this->chunk_begin + size;
}

void MetaChunk::add_src_offset(uint64_t src_offset)
{
    LOG_F(5, "MetaChunk::add_src_offset: offset:%lld", src_offset);
    this->src_offset += src_offset;
    this->chunk_end -= src_offset;
}

// ================================================================================================

FileMetaChunk::FileMetaChunk(const std::string& img_path)
{
    if (!this->image.open(img_path)) {
        ABORT_F("FileMetaChunk: could not open image file \"%s\"", img_path.c_str());
    }
    this->image_size = this->image.size();
    this->is_opened = true;
    this->set_chunk_size(image_size);
}

FileMetaChunk::~FileMetaChunk()
{
    if (this->is_opened) {
        this->image.close();
        is_opened = false;
    }
}

uint64_t FileMetaChunk::read(void* buf, uint64_t offset, uint64_t length) const
{
    return this->image.read(buf, offset, length);
}

uint64_t FileMetaChunk::write(const void* buf, uint64_t offset, uint64_t length)
{
    return this->image.write(buf, offset, length);
}

// ================================================================================================

RamMetaChunk::RamMetaChunk(std::unique_ptr<uint8_t[]> bytes, uint64_t size)
{
    this->bytes = std::move(bytes);
    this->set_chunk_size(size);
}

RamMetaChunk::~RamMetaChunk()
{
}

uint64_t RamMetaChunk::read(void* buf, uint64_t offset, uint64_t length) const
{
    std::memcpy(buf, &bytes[offset], length);
    return length;
}

uint64_t RamMetaChunk::write(const void* buf, uint64_t offset, uint64_t length)
{
    std::memcpy(&bytes[offset], buf, length);
    return length;
}

// ================================================================================================

MetaImgFile::MetaImgFile()
{
}

MetaImgFile::~MetaImgFile() = default;

bool MetaImgFile::open(const std::string &img_path)
{
    VLOG_SCOPE_F(loguru::Verbosity_INFO, "MetaImgFile: Open \"%s\".", img_path.c_str());
    auto file = std::unique_ptr<FileMetaChunk>(new FileMetaChunk(img_path));
    if (!file->is_opened)
        return false;

    auto data = std::unique_ptr<uint8_t[]>(new uint8_t[0x600]);
    uint64_t amount = file->read(data.get(), 0, 0x600);
    if (amount < 0x600) {
        LOG_F(ERROR, "MetaImgFile: Image is too small.");
        return false;
    }

    uint32_t dc_image_size;
    uint64_t offset = 0;
    std::string partition_type;

    int pass;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            // test for DiskCopy 4.2 image
            offset = 0x54;
            dc_image_size = READ_DWORD_BE_A(&data[0x040]);
            if (dc_image_size > file->image_size)
                continue;
        }

        uint16_t sig1 = READ_WORD_BE_A(&data[0x000 + offset]);
        uint16_t sig2 = READ_WORD_BE_A(&data[0x400 + offset]);

        if (sig1 == sbSIGWord) {
            break;
        }
        else if (sig2 == kMFSSigWord) {
            partition_type = "Apple_HFS";
            break;
        }
        else if (sig2 == kHFSSigWord) {
            partition_type = "Apple_HFS";
            break;
        }
        else if (sig2 == kHFSPlusSigWord) {
            partition_type = "Apple_HFS";
            break;
        }
        else if (sig2 == kHFSXSigWord) {
            partition_type = "Apple_HFSX";
            break;
        }
    } // for pass

    if (pass == 1) {
        file->add_src_offset(offset);
        file->set_chunk_size(dc_image_size);
    }

    if (pass < 2) {
        if (partition_type.length()) {
            this->add_partition_from_image(std::move(file), partition_type,
                get_pascal_string(&data[0x400 + offsetof(HFSMasterDirectoryBlock, drVN) + offset]));
        } else {
            if (this->chunks.size() == 0) {
                this->append_chunk(std::move(file));
            } else {
                this->add_partitions_from_image(std::move(file));
            }
        }
    }

    if (pass == 2) {
        // do we want to handle MBR or GPT or ProDOS or FAT or NTFS or BeOS?
        if (this->chunks.size() == 0) {
            LOG_F(WARNING, "MetaImgFile: Attaching unknown disk image type \"%s\".", img_path.c_str());
            this->append_chunk(std::move(file));
        } else {
            LOG_F(ERROR, "MetaImgFile: Ignored attempt to append unknown disk image type \"%s\".", img_path.c_str());
            return false;
        }
    }

    this->dump();
    return true;
}

void MetaImgFile::close()
{
    this->chunks.clear();
    this->meta_image_size = 0;
}

uint64_t MetaImgFile::size() const
{
    return this->meta_image_size;
}

uint64_t MetaImgFile::read(void* buf, uint64_t offset, uint64_t length) const
{
    uint64_t end = offset + length;
    uint64_t pos = offset;
    for (auto it = this->chunks.begin(); it != this->chunks.end(); it++) {
        MetaChunk *chunk = it->get();
        if (end <= chunk->chunk_begin)
            break; // the chunks are sorted so we can end early
        if (pos < chunk->chunk_end) {
            uint64_t chunk_begin = std::max(chunk->chunk_begin, pos);
            uint64_t chunk_end = std::min(chunk->chunk_end, end);
            uint64_t chunk_len = chunk_end - chunk_begin;
            if (pos < chunk_begin) {
                LOG_F(ERROR, "MetaImgFile: read [%lld..%lld) outside of chunks.", pos, chunk_begin);
                memset((char*)buf + pos - offset, 0, chunk_begin - pos);
            }
            uint64_t len = chunk->read((char*)buf + pos - offset,
                chunk_begin - chunk->chunk_begin + chunk->src_offset, chunk_len);
            pos += len;
            if (len < chunk_len)
                return pos - offset;
        }
    }
    if (pos < end) {
        LOG_F(ERROR, "MetaImgFile: read [%lld..%lld) outside of chunks.", pos, end);
        memset((char*)buf + pos - offset, 0, end - pos);
    }
    return pos - offset;
}

uint64_t MetaImgFile::write(const void* buf, uint64_t offset, uint64_t length)
{
    uint64_t end = offset + length;
    uint64_t pos = offset;
    for (auto it = this->chunks.begin(); it != this->chunks.end(); it++) {
        MetaChunk *chunk = it->get();
        if (end <= chunk->chunk_begin)
            break; // the chunks are sorted so we can end early
        if (pos < chunk->chunk_end) {
            uint64_t chunk_begin = std::max(chunk->chunk_begin, pos);
            uint64_t chunk_end = std::min(chunk->chunk_end, end);
            uint64_t chunk_len = chunk_end - chunk_begin;
            if (pos < chunk_begin) {
                LOG_F(ERROR, "MetaImgFile: write [%lld..%lld) outside of chunks.", pos, chunk_begin);
            }
            uint64_t len = chunk->write((char*)buf + pos - offset,
                chunk_begin - chunk->chunk_begin + chunk->src_offset, chunk_len);
            pos += len;
            if (len < chunk_len) {
                LOG_F(ERROR, "MetaImgFile: write fail offset:%lld length:%lld result:%lld", offset, chunk_len, len);
                return pos - offset;
            }
        }
    }
    if (pos < end) {
        LOG_F(ERROR, "MetaImgFile: write [%lld..%lld) outside of chunks.", pos, end);
    }
    return pos - offset;
}

void MetaImgFile::add_partitions_from_image(std::unique_ptr<FileMetaChunk> file)
{
    Block0 block0;
    file->read(&block0, 0x000 + file->src_offset, sizeof(block0));
    Block0_BE_to_host(block0);

    Partition part;

    uint64_t offset = block0.sbBlkSize;
    int part_num = 1;
    int num_partitions = 0;

    uint64_t disk_begin = -1;
    uint64_t disk_end = 0;
    std::vector<Partition> partitions;

    {
        VLOG_SCOPE_F(loguru::Verbosity_5, "Gathering partitions to add");
        for (;;) {
            file->read(&part, offset + file->src_offset, sizeof(part));
            Partition_BE_to_host(part);
            if (part.pmSig != pMapSIG) {
                LOG_F(ERROR, "MetaImgFile: Wrong partition signature.");
                break;
            }
            if (part_num == 1)
                num_partitions = part.pmMapBlkCnt;

            std::string partition_name = get_part_string(&part.pmPartName, sizeof(part.pmPartName));
            std::string partition_type = get_part_string(&part.pmParType, sizeof(part.pmParType));

            if (1
                && partition_type != "Apple_partition_map"
                && partition_type != "Apple_Driver43"
                && partition_type != "Apple_Driver_ATA"
                && partition_type != "Apple_FWDriver"
                && partition_type != "Apple_Driver_IOKit"
                && partition_type != "Apple_Patches"
                && partition_type != "Apple_Void"
            ) {
                uint64_t part_begin = (uint64_t)part.pmPyPartStart * block0.sbBlkSize;
                uint64_t part_end = part_begin + (uint64_t)part.pmPartBlkCnt * block0.sbBlkSize;
                if (part_begin > file->chunk_end || part_end > file->chunk_end) {
                    LOG_F(ERROR, "MetaImgFile: Partition %d has incorrect start or size.", part_num);
                }
                else {
                    if (part_begin < disk_begin)
                        disk_begin = part_begin;
                    if (part_end > disk_end)
                        disk_end = part_end;
                }
                //this->dump_partition(part, part_num, num_partitions);
                partitions.push_back(part);
            }

            offset += block0.sbBlkSize;
            part_num++;
            if (part_num > num_partitions)
                break;
        }
    }

    if (partitions.size())
    {
        VLOG_SCOPE_F(loguru::Verbosity_5, "Adding partitions");

        file->add_src_offset(disk_begin);
        file->set_chunk_size(disk_end - disk_begin);
        this->reserve_partitions((int)partitions.size()); // may increase this->meta_image_size
        file->set_chunk_begin(this->meta_image_size);

        for (auto it = partitions.begin(); it != partitions.end(); it++) {
            uint64_t block_count = (uint64_t)it->pmPartBlkCnt * block0.sbBlkSize / this->block_size;
            if (block_count >= std::numeric_limits<uint32_t>::max()) {
                LOG_F(ERROR, "MetaImgFile: Partition is too big (%lld blocks).", block_count);
                continue;
            }

            uint64_t block_start = (file->chunk_begin + (uint64_t)it->pmPyPartStart * block0.sbBlkSize - disk_begin)
                / this->block_size;
            if (block_count + block_start >= std::numeric_limits<uint32_t>::max()) {
                LOG_F(ERROR, "MetaImgFile: Partition is too big to append (adding %lld blocks; %lld blocks total).",
                    block_count, block_count + block_start);
                continue;
            }

            it->pmPyPartStart = uint32_t(block_start);
            it->pmPartBlkCnt = uint32_t(block_count);
            it->pmDataCnt = uint32_t(block_count);

            add_partition(*it);
        }

        this->append_chunk(std::move(file));
    }
    else {
        LOG_F(ERROR, "MetaImgFile: No partitions to add.");
    }
}

void MetaImgFile::add_partition_from_image(
    std::unique_ptr<FileMetaChunk> file, std::string partition_type, std::string volume_name
)
{
    this->reserve_partitions(1);

    file->set_chunk_begin(this->meta_image_size);

    uint64_t block_count = (file->chunk_end - file->chunk_begin + this->block_size - 1) / this->block_size;
    if (block_count >= std::numeric_limits<uint32_t>::max()) {
        LOG_F(ERROR, "MetaImgFile: Partition is too big (%lld blocks).", block_count);
        return;
    }

    uint64_t block_start = file->chunk_begin / this->block_size;
    if (block_count + block_start >= std::numeric_limits<uint32_t>::max()) {
        LOG_F(ERROR, "MetaImgFile: Partition is too big to append (adding %lld blocks; %lld blocks total).",
            block_count, block_count + block_start);
        return;
    }

    Partition part = {
        .pmSig = pMapSIG,
        .pmPyPartStart = uint32_t(block_start),
        .pmPartBlkCnt = uint32_t(block_count),
        .pmDataCnt = uint32_t(block_count),
        .pmPartStatus = kPartitionAUXIsValid|kPartitionAUXIsAllocated|
            kPartitionAUXIsReadable|kPartitionIsWriteable|kPartitionIsMountedAtStartup,
    };
    set_part_string(&part.pmPartName, sizeof(part.pmPartName), volume_name);
    set_part_string(&part.pmParType,  sizeof(part.pmParType),  partition_type);

    add_partition(part);

    append_chunk(std::move(file));
}

void MetaImgFile::append_chunk(std::unique_ptr<FileMetaChunk> file)
{
    this->meta_image_size += (file->chunk_end - file->chunk_begin + this->block_size - 1) / this->block_size * this->block_size;

    if (this->chunks.size()) {
        auto ram = dynamic_cast<RamMetaChunk *>(this->chunks[0].get());
        Block0* block0 = (Block0*)&ram->bytes[0];
        Block0_BE_to_host(*block0);
        {
            uint64_t new_block_count = this->meta_image_size / this->block_size;
            block0->sbBlkCount = (uint32_t)new_block_count;
            if (new_block_count != block0->sbBlkCount) {
                LOG_F(ERROR, "MetaImgFile: Block count exceeds 32-bit maximum. Need to increase block size.");
                block0->sbBlkCount = -1;
            }
        }
        Block0_host_to_BE(*block0);
    }

    this->chunks.push_back(std::move(file));
    std::sort(chunks.begin(), chunks.end(),
        [](std::unique_ptr<MetaChunk>& a, std::unique_ptr<MetaChunk> &b)
        {
            return a->chunk_begin < b->chunk_begin;
        }
    );
}

void MetaImgFile::reserve_partitions(int partitions_to_reserve)
{
    if (this->max_partitions == 0) {
        std::ifstream partitions;
        partitions.open("apm_all_drivers.bin", std::ios::in | std::ios::binary);
        if (partitions.fail()) {
            ABORT_F("MetaImgFile: Missing file \"apm_all_drivers.bin\"");
            return;
        }
        partitions.seekg(0, std::ios::end);
        size_t partitions_size = partitions.tellg();
        auto data = std::unique_ptr<uint8_t[]>(new uint8_t[partitions_size]);
        partitions.seekg(0, std::ios::beg);
        partitions.read((char*)data.get(), partitions_size);
        partitions.close();

        Partition part = *(Partition*)&data[this->block_size];
        Partition_BE_to_host(part);

        this->meta_image_size = partitions_size;
        this->used_partitions = part.pmMapBlkCnt - 1; // 8 (subtract the Apple_HFS partition included in apm_all_drivers.bin)
        this->max_partitions = part.pmPartBlkCnt; // 63

        auto partition_map = std::unique_ptr<RamMetaChunk>(new RamMetaChunk(std::move(data), partitions_size));

        if (this->chunks.size()) {
            std::unique_ptr<MetaChunk> meta = std::move(this->chunks.back());
            this->chunks.pop_back();
            this->chunks.insert(chunks.begin(), std::move(partition_map));
            FileMetaChunk *file = dynamic_cast<FileMetaChunk *>(meta.release());
            this->add_partitions_from_image(std::unique_ptr<FileMetaChunk>{file});
        } else {
            this->chunks.insert(chunks.begin(), std::move(partition_map));
        }
    }

    uint32_t new_num_partitions = this->used_partitions + this->min_unused_partitions + partitions_to_reserve;
    if (new_num_partitions <= this->max_partitions)
        return;

    // add 1 to include Block0 for the first partitions_increment number of blocks
    // then subtract 1 to remove Block0 from the number of partitions.
    uint32_t new_max_partitions = (1 + new_num_partitions + partitions_increment - 1)
        / partitions_increment * partitions_increment - 1;

    uint32_t block_increment = new_max_partitions - max_partitions;
    uint64_t bytes_increment = uint64_t(block_increment) * this->block_size;

    auto ram = dynamic_cast<RamMetaChunk *>(this->chunks[0].get());
    uint64_t ram_size = ram->chunk_end - ram->chunk_begin;
    // add 1 to include Block0
    uint64_t partitions_start = (uint64_t)(this->max_partitions + 1) * this->block_size;
    uint64_t partitions_size = ram_size - partitions_start;
    // add 1 to include Block0
    uint64_t partition_map_size = (uint64_t)(this->used_partitions + 1) * this->block_size;

    ram_size += bytes_increment;
    auto data2 = std::unique_ptr<uint8_t[]>(new uint8_t[ram_size]);
    memcpy(&data2[0], &ram->bytes[0], partition_map_size);
    memset(&data2[partition_map_size], 0, bytes_increment);
    memcpy(&data2[partitions_start + bytes_increment], &ram->bytes[partitions_start], partitions_size);

    Block0* block0 = (Block0*)&data2[0];
    Block0_BE_to_host(*block0);
    {
        block0->sbBlkCount += block_increment;
        DDMap *ddmap = (DDMap *)&block0->ddBlock;
        for (int i = 0; i < block0->sbDrvrCount; i++) {
            ddmap->ddBlock += block_increment;
            ddmap++;
        }
    }
    Block0_host_to_BE(*block0);

    Partition* part = (Partition*)&data2[0];
    int part_num = 1;
    int num_partitions = 0;
    for (;;) {
        part++;
        Partition_BE_to_host(*part);
        if (part_num == 1) {
            num_partitions = part->pmMapBlkCnt;
            part->pmPartBlkCnt += block_increment;
            part->pmDataCnt += block_increment;
        }
        else
        {
            part->pmPyPartStart += block_increment;
        }
        Partition_host_to_BE(*part);
        part_num++;
        if (part_num > num_partitions)
            break;
    }
    ram->bytes = std::move(data2);

    for (auto it = this->chunks.begin(); it != this->chunks.end(); it++) {
        MetaChunk *chunk = it->get();
        if (chunk->chunk_begin != 0)
            chunk->chunk_begin += bytes_increment;
        chunk->chunk_end += bytes_increment;
    }

    this->meta_image_size += bytes_increment;
}

void MetaImgFile::add_partition(Partition &new_part)
{
    if (this->used_partitions >= this->max_partitions) {
        LOG_F(ERROR, "MetaImgFile: No room for new partition. Use reserve_partitions() first.");
        return;
    }

    auto ram = dynamic_cast<RamMetaChunk *>(this->chunks[0].get());
    // add 1 to skip Block0
    Partition &dst_part = *(Partition *)(&ram->bytes[((uint64_t)this->used_partitions + 1) * this->block_size]);
    dst_part = new_part;
    //this->dump_partition(dst_part, used_partitions + 1, -1);
    Partition_host_to_BE(dst_part);

    this->used_partitions++;

    for (int i = 1; i <= this->used_partitions; i++) {
        Partition* part = (Partition*)&ram->bytes[(uint64_t)this->block_size * i];
        Partition_BE_to_host(*part);
        part->pmMapBlkCnt = this->used_partitions;
        Partition_host_to_BE(*part);
    }
}

void MetaImgFile::dump()
{
    if (this->chunks.size() == 0) {
        LOG_F(ERROR, "MetaImgFile: Image has no chunks.");
        return;
    }

    Block0 block0;
    Partition part;

    std::unique_ptr<uint8_t[]> data;
    uint64_t data_len = 64 * 512;
    
    int num_partitions;

    for (;;) {
        data = std::unique_ptr<uint8_t[]>(new uint8_t[data_len]);
        this->read(&data[0], 0, data_len);
        block0 = *(Block0*)&data[0];
        Block0_BE_to_host(block0);
        if (block0.sbSig != sbSIGWord) {
            LOG_F(ERROR, "MetaImgFile: Image is not Apple Partition Map formatted.");
            return;
        }
        part = *(Partition*)&data[(uint64_t)this->block_size];
        Partition_BE_to_host(part);
        num_partitions = part.pmMapBlkCnt;

        if (data_len >= (uint64_t)this->block_size * num_partitions)
            break;
        data_len <<= 1;
    }

    if (block0.sbSig == sbSIGWord) {
        printf("APM Block 0 contents\n");

        printf("000: sbSig      : 'ER' = sbSIGWord\n");
        printf("002: sbBlkSize  : %d\n", block0.sbBlkSize);
        printf("004: sbBlkCount : %d = %lld MB = %lld MiB\n", block0.sbBlkCount,
            (uint64_t)block0.sbBlkCount * block0.sbBlkSize / (1000 * 1000),
            (uint64_t)block0.sbBlkCount * block0.sbBlkSize / (1024 * 1024)
        );
        if (block0.sbDevType   != 0) printf("008: sbDevType  : %d\n", block0.sbDevType);
        if (block0.sbDevId     != 0) printf("00a: sbDevId    : %d\n", block0.sbDevId);
        if (block0.sbData      != 0) printf("00c: sbData     : %d\n", block0.sbData);
        if (block0.sbDrvrCount > 10) printf("010: sbDrvrCount: %d\n", block0.sbDrvrCount);

        DDMap *ddmap = (DDMap *)&block0.ddBlock;
        for (int i = 0; i < block0.sbDrvrCount; i++) {
            printf("%03x: DDMap[%d]:%4d @ %-4d 0x%04x = %-27s\n", 0x012 + i*8, i, ddmap->ddSize, ddmap->ddBlock, ddmap->ddType,
                (ddmap->ddType == 0x0001) ? "kDriverTypeMacSCSI"       :
                (ddmap->ddType == 0x0701) ? "kDriverTypeMacATA"        :
                (ddmap->ddType == 0xFFFF) ? "kDriverTypeMacSCSIChained":
                (ddmap->ddType == 0xF8FF) ? "kDriverTypeMacATAChained" :
                "?"
            );
            ddmap++;
        }

        printf("\n");
        printf("Partition Map contents (%d partitions)\n", num_partitions);

        for (int i = 1; i <= num_partitions; i++) {
            part = *(Partition*)&data[(uint64_t)this->block_size * i];
            if (not_zero(&part, sizeof(part))) {
                Partition_BE_to_host(part);
                this->dump_partition(part, i, num_partitions);
            }
        }
    }
    
    printf("\n");
    printf("Chunks\n");
    for (auto it = this->chunks.begin(); it != this->chunks.end(); it++) {
        MetaChunk *chunk = it->get();
        printf("%10lld @ %-10lld bytes:[%lld %lld) offset:%lld",
            (chunk->chunk_end - chunk->chunk_begin) / this->block_size,
            chunk->chunk_begin / this->block_size,
            chunk->chunk_begin, chunk->chunk_end,
            chunk->src_offset
        );
        if (chunk->chunk_begin & 0x1ff || chunk->chunk_end & 0x1ff)
            printf(" (error)");
        printf("\n");
    }
}

void MetaImgFile::dump_partition(Partition &part, int index, int num_partitions)
{
    printf("%2d:", index);
    printf(" %10d @ %-10d", part.pmPartBlkCnt, part.pmPyPartStart);

    if (part.pmSig != 0x504d) printf(" Sig:0x%04x?", part.pmSig);
    if (part.pmParType[0]) printf(" Type:\"%s\"", part.pmParType);
    if (part.pmPartName[0]) printf(" Name:\"%s\"", part.pmPartName);
    if (part.pmSigPad) printf(" SigPad:0x%04X", part.pmSigPad);
    if (num_partitions >= 0 && part.pmMapBlkCnt != num_partitions) printf(" MapBlkCnt:%d", part.pmMapBlkCnt);
    if (part.pmLgDataStart) printf(" LgDataStart:%d", part.pmLgDataStart);

    if (not_zero(&part.pmLgDataStart, sizeof(part) - offsetof(Partition, pmLgDataStart)))
    {
        if (part.pmDataCnt != part.pmPartBlkCnt) printf(" DataCnt:%d", part.pmDataCnt);
        if (part.pmPartStatus) {
            char statustext[400];
            int len = 0;
            #define sprintone(x, ...) \
                if (len < sizeof(statustext) && (x)) \
                    len += snprintf(statustext + len, sizeof(statustext) - len, __VA_ARGS__);
            sprintone(part.pmPartStatus & 0x00000001, ",Valid"); // AUX
            sprintone(part.pmPartStatus & 0x00000002, ",Allocated"); // AUX
            sprintone(part.pmPartStatus & 0x00000004, ",InUse"); // AUX
            sprintone(part.pmPartStatus & 0x00000008, ",Bootable"); // AUX
            sprintone(part.pmPartStatus & 0x00000010, ",Readable"); // AUX
            sprintone(part.pmPartStatus & 0x00000020, ",Writeable"); // AUX and Mac OS
            sprintone(part.pmPartStatus & 0x00000040, ",BootCodePositionIndependent"); // AUX
            sprintone(part.pmPartStatus & 0x00000080, ",OSSpecific2"); // ?
            sprintone(part.pmPartStatus & 0x00000100, ",ChainCompatible"); // driver
            sprintone(part.pmPartStatus & 0x00000200, ",RealDeviceDriver"); // driver
            sprintone(part.pmPartStatus & 0x00000400, ",CanChainToNext"); // driver
            sprintone(part.pmPartStatus & 0x40000000, ",MountedAtStartup"); // Mac OS
            sprintone(part.pmPartStatus & 0x80000000, ",Startup"); // Mac OS
            sprintone(part.pmPartStatus & 0x3FFFF800, ",0x%x?", part.pmPartStatus & 0x3FFFF800);
            printf(" Status:%08X=%s", part.pmPartStatus, &statustext[1]);
        }

        if (part.pmLgBootStart) printf(" LgBootStart:%d", part.pmLgBootStart);
        if (part.pmBootSize) printf(" BootSize:%d", part.pmBootSize);
        if (part.pmBootAddr) printf(" BootAddr:0x%08X", part.pmBootAddr);
        if (part.pmBootAddr2) printf(" BootAddr2:0x%08X", part.pmBootAddr2);
        if (part.pmBootEntry) printf(" BootEntry:0x%08X", part.pmBootEntry);
        if (part.pmBootEntry2) printf(" BootEntry2:0x%08X", part.pmBootEntry2);
        if (part.pmBootCksum) printf(" BootCksum:0x%08X", part.pmBootCksum);
        if (part.pmProcessor[0]) printf(" Processor:\"%s\"", part.pmProcessor);

        if (not_zero(part.pmPad, sizeof(part.pmPad))) {
            printf(" Pad:");
            int len = sizeof(part.pmPad);
            uint8_t *p = (uint8_t *)(&part.pmPad) + len - 1;
            while (len > 0) {
                if (*p != 0)
                    break;
                len--;
                p--;
            }
            p = (uint8_t *)(&part.pmPad);
            for (int i = 0; i < len; i++)
                printf("%02x", *p++);
            uint32_t sig = READ_DWORD_BE_A(&part.pmPad);
            switch (sig) {
            case 0x70744452: printf(  " = 'ptDR' = kPatchDriverSignature"); break;
            case 0x00010600: printf("00 = kSCSIDriverSignature"); break;
            case 0x77696b69: printf(  " = 'wiki' = kATADriverSignature"); break;
            case 0x43447672: printf(  " = 'CDvr' = kSCSICDDriverSignature"); break;
            case 0x41545049: printf(  " = 'ATPI' = kATAPIDriverSignature"); break;
            case 0x44535531: printf(  " = 'DSU1' = kDriveSetupHFSSignature"); break;
            }
        }
    }
    printf("\n");
}
