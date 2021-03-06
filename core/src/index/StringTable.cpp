#include "hashing/MurmurHash3.h"
#include "StringTable.hpp"

#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <vector>

using std::ios;
using namespace utymap::index;

class StringTable::StringTableImpl
{
    typedef std::vector<std::uint32_t> IdList;
    typedef std::unordered_map<std::uint32_t, IdList> HashIdMap;

    std::fstream indexFile_;
    std::fstream dataFile_;
    std::uint32_t seed_;
    std::uint32_t nextId_;

    // TODO think about better data structure alternative
    HashIdMap map_;
    std::vector<std::uint32_t> offsets_;

public:
    StringTableImpl(const std::string& indexPath, const std::string& dataPath, std::uint32_t seed) :
        indexFile_(indexPath, ios::in | ios::out | ios::binary | ios::ate | ios::app),
        dataFile_(dataPath, ios::in | ios::out | ios::binary | ios::app),
        seed_(seed),
        nextId_(0),
        map_(),
        offsets_()
    {
        nextId_ = static_cast<std::uint32_t>(indexFile_.tellg() / (sizeof(std::uint32_t) * 2));
        if (nextId_ > 0)
        {
            int32_t count = nextId_;
            offsets_.reserve(count);
            indexFile_.seekg(0, ios::beg);
            for (int i = 0; i < count; ++i)
            {
                std::uint32_t hash, offset;
                indexFile_.read(reinterpret_cast<char*>(&hash), sizeof(hash));
                indexFile_.read(reinterpret_cast<char*>(&offset), sizeof(offset));
                offsets_.push_back(offset);
                map_[hash].push_back(i);
            }
        }
    }

    ~StringTableImpl()
    {
        indexFile_.close();
        dataFile_.close();
    }

    std::uint32_t getId(const std::string& str)
    {
        std::uint32_t hash;
        MurmurHash3_x86_32(str.c_str(), static_cast<int>(str.size()), seed_, &hash);

        HashIdMap::iterator hashLookupResult = map_.find(hash);
        if (hashLookupResult != map_.end())
        {
            for (std::uint32_t id : hashLookupResult->second)
            {
                std::string data;
                readString(id, data);
                if (str == data)
                    return id;
            }
        }

        writeString(hash, str);
        return nextId_++;
    }

    std::string getString(std::uint32_t id)
    {
        std::string str;
        readString(id, str);
        return str;
    }

    void flush() { /* TODO */ }

private:

    // reads string by id.
    void readString(std::uint32_t id, std::string& data)
    {
        if (id < offsets_.size())
        {
            std::uint32_t offset = offsets_[id];
            std::string::size_type size = id + 1 < nextId_ ? offsets_[id] : 8;
            data.reserve(size);

            dataFile_.seekg(offset, ios::beg);
            std::getline(dataFile_, data, '\0');
        }
    }

    // write string to index.
    void writeString(std::uint32_t hash, const std::string& data)
    {
        // get offset as file size
        dataFile_.seekg(0, ios::end);
        std::uint32_t offset = static_cast<std::uint32_t>(dataFile_.tellg());

        // write string
        dataFile_.seekp(0, ios::end);
        dataFile_ << data.c_str() << '\0';

        // write index entry
        indexFile_.seekp(0, ios::end);
        indexFile_.write(reinterpret_cast<char*>(&hash), sizeof(hash));
        indexFile_.write(reinterpret_cast<char*>(&offset), sizeof(offset));

        map_[hash].push_back(nextId_);
        offsets_.push_back(offset);
    }
};

StringTable::StringTable(const std::string& path) :
    pimpl_(new StringTable::StringTableImpl(path + "string.idx", path + "string.dat", 0)) 
{
}

StringTable::~StringTable() { }

std::uint32_t StringTable::getId(const std::string& str)
{
    return pimpl_->getId(str);
}

std::string StringTable::getString(std::uint32_t id)
{
    return pimpl_->getString(id);
}

void StringTable::flush()
{
    pimpl_->flush();
}
