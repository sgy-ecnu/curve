/*
 * Project: curve
 * Created Date: Wednesday September 26th 2018
 * Author: hzsunjianliang
 * Copyright (c) 2018 netease
 */

#ifndef TEST_MDS_NAMESERVER2_FAKES_H_
#define TEST_MDS_NAMESERVER2_FAKES_H_

#include <glog/logging.h>
#include <vector>
#include <mutex>   // NOLINT
#include <utility>
#include <map>
#include <string>
#include "src/mds/nameserver2/define.h"
#include "src/mds/nameserver2/inode_id_generator.h"
#include "src/mds/nameserver2/chunk_allocator.h"
#include "src/mds/nameserver2/namespace_storage.h"
#include "src/mds/nameserver2/chunk_id_generator.h"
#include "src/mds/topology/topology_admin.h"

using ::curve::mds::topology::TopologyAdmin;

const uint64_t FACK_INODE_INITIALIZE = 0;
const uint64_t FACK_CHUNKID_INITIALIZE = 0;

namespace curve {
namespace mds {
class FakeInodeIDGenerator : public InodeIDGenerator {
 public:
    explicit FakeInodeIDGenerator(uint64_t initValue = FACK_INODE_INITIALIZE) {
        value_ = initValue;
    }
    bool GenInodeID(InodeID *id) {
        *id = ++value_;
        return true;
    }
 private:
    std::atomic<uint64_t> value_;
};

class FackChunkIDGenerator: public ChunkIDGenerator {
 public:
    explicit FackChunkIDGenerator(uint64_t val = FACK_CHUNKID_INITIALIZE) {
        value_ = val;
    }
    bool GenChunkID(ChunkID *id) override {
        *id = ++value_;
        return true;
    }
 private:
    std::atomic<uint64_t> value_;
};

class FackTopologyAdmin: public TopologyAdmin {
 public:
    using CopysetIdInfo = ::curve::mds::topology::CopysetIdInfo;
    using FileType = ::curve::mds::FileType;

    FackTopologyAdmin() {}

    bool AllocateChunkRandomInSingleLogicalPool(
            FileType fileType, uint32_t chunkNumer,
            std::vector<CopysetIdInfo> *infos) override {
        for (uint32_t i = 0; i != chunkNumer; i++) {
            CopysetIdInfo copysetIdInfo{0, i};
            infos->push_back(copysetIdInfo);
        }
        return true;
    }
};

class FakeNameServerStorage : public NameServerStorage {
 public:
    StoreStatus PutFile(const std::string & storeKey,
          const FileInfo & fileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);

        auto iter = memKvMap_.find(storeKey);
        if (iter != memKvMap_.end()) {
            memKvMap_.erase(iter);
        }

        std::string value = fileInfo.SerializeAsString();
        memKvMap_.insert(
            std::move(std::pair<std::string, std::string>
            (storeKey, std::move(value))));
        return StoreStatus::OK;
    }

    StoreStatus GetFile(const std::string & storeKey,
          FileInfo * fileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storeKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        fileInfo->ParseFromString(iter->second);
        return StoreStatus::OK;
    }

    StoreStatus DeleteFile(const std::string & storekey) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storekey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        memKvMap_.erase(iter);
        return StoreStatus::OK;
    }

    StoreStatus RenameFile(const std::string & oldStoreKey,
                           const FileInfo &oldfileInfo,
                           const std::string & newStoreKey,
                           const FileInfo &newfileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);

        auto iter = memKvMap_.find(oldStoreKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        memKvMap_.erase(iter);

        std::string value = newfileInfo.SerializeAsString();
        memKvMap_.insert(
            std::move(std::pair<std::string, std::string>
            (newStoreKey, std::move(value))));

        return StoreStatus::OK;
    }

    StoreStatus ListFile(const std::string & startStoreKey,
                         const std::string & endStoreKey,
                         std::vector<FileInfo> * files) override {
        std::lock_guard<std::mutex> guard(lock_);

        auto iter = memKvMap_.begin();
        for ( iter; iter != memKvMap_.end(); iter++ ) {
            if (iter->first.compare(startStoreKey) >= 0) {
                if (iter->first.compare(endStoreKey) < 0) {
                    FileInfo  validFile;
                    validFile.ParseFromString(iter->second);
                    files->push_back(validFile);
                }
            }
        }

        return StoreStatus::OK;
    }


    StoreStatus GetSegment(const std::string & storeKey,
                           PageFileSegment *segment) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storeKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        segment->ParseFromString(iter->second);
        return StoreStatus::OK;
    }

    StoreStatus PutSegment(const std::string & storeKey,
                           const PageFileSegment * segment) override {
        std::lock_guard<std::mutex> guard(lock_);
        std::string value = segment->SerializeAsString();
        memKvMap_.insert(std::move(std::pair<std::string, std::string>
            (storeKey, std::move(value))));
        return StoreStatus::OK;
    }

    StoreStatus DeleteSegment(const std::string &storeKey) override {
        std::lock_guard<std::mutex> guard(lock_);
        auto iter = memKvMap_.find(storeKey);
        if (iter == memKvMap_.end()) {
            return StoreStatus::KeyNotExist;
        }
        memKvMap_.erase(iter);
        return StoreStatus::OK;
    }

    StoreStatus SnapShotFile(const std::string & originalFileKey,
                            const FileInfo *originalFileInfo,
                            const std::string & snapshotFileKey,
                            const FileInfo * snapshotFileInfo) override {
        std::lock_guard<std::mutex> guard(lock_);
        std::string originalFileData = originalFileInfo->SerializeAsString();
        std::string snapshotFileData = snapshotFileInfo->SerializeAsString();
        memKvMap_.erase(originalFileKey);
        memKvMap_.insert(std::move(std::pair<std::string, std::string>
            (originalFileKey, std::move(originalFileData))));
        memKvMap_.insert(std::move(std::pair<std::string, std::string>
            (snapshotFileKey, std::move(snapshotFileData))));
        return StoreStatus::OK;
    }

    StoreStatus LoadSnapShotFile(std::vector<FileInfo> *snapShotFiles)
    override {
        std::lock_guard<std::mutex> guard(lock_);
        std::string snapshotStartKey = SNAPSHOTFILEINFOKEYPREFIX;

        auto iter = memKvMap_.begin();
        for ( iter; iter != memKvMap_.end(); iter++ ) {
            if ( iter->first.length() > snapshotStartKey.length() ) {
                if (iter->first.substr(0, snapshotStartKey.length()).
                    compare(snapshotStartKey) == 0) {
                    FileInfo  validFile;
                    validFile.ParseFromString(iter->second);
                    snapShotFiles->push_back(validFile);
                }
            }
        }
        return StoreStatus::OK;
    }

 private:
    std::mutex lock_;
    std::map<std::string, std::string> memKvMap_;
};
}  // namespace mds
}  // namespace curve
#endif  // TEST_MDS_NAMESERVER2_FAKES_H_
