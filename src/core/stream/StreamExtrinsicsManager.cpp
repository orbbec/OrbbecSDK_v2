#include "StreamExtrinsicsManager.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

OBExtrinsic multiplyExtrinsics(const OBExtrinsic &a, const OBExtrinsic &b) {
    OBExtrinsic result;
    result.rot[0]   = a.rot[0] * b.rot[0] + a.rot[1] * b.rot[3] + a.rot[2] * b.rot[6];
    result.rot[1]   = a.rot[0] * b.rot[1] + a.rot[1] * b.rot[4] + a.rot[2] * b.rot[7];
    result.rot[2]   = a.rot[0] * b.rot[2] + a.rot[1] * b.rot[5] + a.rot[2] * b.rot[8];
    result.rot[3]   = a.rot[3] * b.rot[0] + a.rot[4] * b.rot[3] + a.rot[5] * b.rot[6];
    result.rot[4]   = a.rot[3] * b.rot[1] + a.rot[4] * b.rot[4] + a.rot[5] * b.rot[7];
    result.rot[5]   = a.rot[3] * b.rot[2] + a.rot[4] * b.rot[5] + a.rot[5] * b.rot[8];
    result.rot[6]   = a.rot[6] * b.rot[0] + a.rot[7] * b.rot[3] + a.rot[8] * b.rot[6];
    result.rot[7]   = a.rot[6] * b.rot[1] + a.rot[7] * b.rot[4] + a.rot[8] * b.rot[7];
    result.rot[8]   = a.rot[6] * b.rot[2] + a.rot[7] * b.rot[5] + a.rot[8] * b.rot[8];
    result.trans[0] = a.trans[0] + a.rot[0] * b.trans[0] + a.rot[1] * b.trans[1] + a.rot[2] * b.trans[2];
    result.trans[1] = a.trans[1] + a.rot[3] * b.trans[0] + a.rot[4] * b.trans[1] + a.rot[5] * b.trans[2];
    result.trans[2] = a.trans[2] + a.rot[6] * b.trans[0] + a.rot[7] * b.trans[1] + a.rot[8] * b.trans[2];
    return result;
}

OBExtrinsic calculateExtrinsics(const std::vector<std::pair<uint64_t, OBExtrinsic>> &path) {
    OBExtrinsic extrinsics = IdentityExtrinsics;
    for(auto &pair: path) {
        const OBExtrinsic &transform = pair.second;
        extrinsics                   = multiplyExtrinsics(transform, extrinsics);
    }
    return extrinsics;
}

OBExtrinsic inverseExtrinsics(const OBExtrinsic &extrinsics) {
    OBExtrinsic invExtrinsic;
    // Transpose the rotation matrix
    invExtrinsic.rot[0] = extrinsics.rot[0];
    invExtrinsic.rot[1] = extrinsics.rot[3];
    invExtrinsic.rot[2] = extrinsics.rot[6];
    invExtrinsic.rot[3] = extrinsics.rot[1];
    invExtrinsic.rot[4] = extrinsics.rot[4];
    invExtrinsic.rot[5] = extrinsics.rot[7];
    invExtrinsic.rot[6] = extrinsics.rot[2];
    invExtrinsic.rot[7] = extrinsics.rot[5];
    invExtrinsic.rot[8] = extrinsics.rot[8];

    // Calculate the inverse translation vector
    float invRot[9] = { invExtrinsic.rot[0], invExtrinsic.rot[1], invExtrinsic.rot[2], invExtrinsic.rot[3], invExtrinsic.rot[4],
                        invExtrinsic.rot[5], invExtrinsic.rot[6], invExtrinsic.rot[7], invExtrinsic.rot[8] };

    invExtrinsic.trans[0] = -invRot[0] * extrinsics.trans[0] - invRot[1] * extrinsics.trans[1] - invRot[2] * extrinsics.trans[2];
    invExtrinsic.trans[1] = -invRot[3] * extrinsics.trans[0] - invRot[4] * extrinsics.trans[1] - invRot[5] * extrinsics.trans[2];
    invExtrinsic.trans[2] = -invRot[6] * extrinsics.trans[0] - invRot[7] * extrinsics.trans[1] - invRot[8] * extrinsics.trans[2];

    return invExtrinsic;
}

std::mutex                               StreamExtrinsicsManager::instanceMutex_;
std::weak_ptr<StreamExtrinsicsManager>   StreamExtrinsicsManager::instanceWeakPtr_;
std::shared_ptr<StreamExtrinsicsManager> StreamExtrinsicsManager::getInstance() {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         instance = instanceWeakPtr_.lock();
    if(!instance) {
        instance         = std::shared_ptr<StreamExtrinsicsManager>(new StreamExtrinsicsManager());
        instanceWeakPtr_ = instance;
    }
    return instance;
}

StreamExtrinsicsManager::StreamExtrinsicsManager() {}

StreamExtrinsicsManager::~StreamExtrinsicsManager() noexcept = default;

void StreamExtrinsicsManager::registerExtrinsics(const std::shared_ptr<const StreamProfile> &from, const std::shared_ptr<const StreamProfile> &to,
                                                 const OBExtrinsic &extrinsics) {
    if(!from || !to) {
        throw invalid_value_exception("Invalid stream profile, from or to is null");
    }

    std::unique_lock<std::recursive_mutex> lock(mutex_);
    cleanExpiredStreamProfiles();  // clean expired stream profiles first

    // judge if already registered and if the extrinsics is the same
    bool alreadyRegistered   = false;
    bool differentExtrinsics = false;
    try {
        if(hasExtrinsics(from, to)) {
            auto tryResult    = getExtrinsics(from, to);  // try to get the extrinsics
            alreadyRegistered = true;
            if(memcmp(&tryResult, &extrinsics, sizeof(OBExtrinsic)) != 0) {
                differentExtrinsics = true;
            }
        }
    }
    catch(...) {
        alreadyRegistered = false;
    }

    // if already registered, and the extrinsics is the same, then return
    if(alreadyRegistered && !differentExtrinsics) {
        // LOG_TRACE("Extrinsics already registered, {} to {}", (uint64_t)from.get(), (uint64_t)to.get());
        return;
    }

    // judge if the extrinsics is identity
    bool isIdentityExtrinsics = (memcmp(&extrinsics, &IdentityExtrinsics, sizeof(OBExtrinsic)) == 0);

    // register the extrinsics
    if(alreadyRegistered) {
        // if already registered, remove the old one, and then register the new one
        eraseStreamProfile(from);
        auto toId = getOrRegisterStreamProfileId(to);
        if(isIdentityExtrinsics) {
            // if the extrinsics is identity, we can just push the `from` stream profile to the list of `toId`
            streamProfileMap_[toId].push_back(std::weak_ptr<const StreamProfile>(from));
        }
        else {
            auto fromId = getOrRegisterStreamProfileId(from);                             // get or create the id of `from`
            extrinsicsGraph_[fromId].push_back({ toId, extrinsics });                     // add the extrinsics to the graph: from -> to
            extrinsicsGraph_[toId].push_back({ fromId, inverseExtrinsics(extrinsics) });  // add the inverse extrinsics to the graph: to -> from
        }
    }
    else {
        // if not registered, just register the new one
        auto fromId = getStreamProfileId(from);
        auto toId   = getOrRegisterStreamProfileId(to);
        if(isIdentityExtrinsics) {
            if(fromId == 0) {
                // if is identity extrinsics, and the `from` stream profile is not registered, we can just push the `from` stream profile to the list of `toId`
                streamProfileMap_[toId].push_back(std::weak_ptr<const StreamProfile>(from));
            }
            else {
                // if is identity extrinsics, and the `from` stream profile is registered, we need to move oll the stream profiles from `fromId` to `toId`
                auto spList = streamProfileMap_[fromId];
                for(auto it = spList.begin(); it != spList.end(); ++it) {
                    streamProfileMap_[toId].push_back(*it);
                }
                streamProfileMap_.erase(fromId);

                // after moving the stream profiles, we need to update the extrinsics graph with the new id： `toId`
                auto &extrPairVec = extrinsicsGraph_[fromId];
                for(auto &extrPair: extrPairVec) {
                    auto &toExtrPairVec = extrinsicsGraph_[extrPair.first];
                    for(auto it = toExtrPairVec.begin(); it != toExtrPairVec.end(); ++it) {
                        if(it->first == fromId) {
                            it->first = toId;
                        }
                    }
                }
                extrinsicsGraph_.erase(fromId);
            }
        }
        else {
            // if not identity extrinsics, we need to register the extrinsics to the graph
            if(fromId == 0) {
                // if the `from` stream profile is not registered, we need to register it first
                fromId = getOrRegisterStreamProfileId(from);
            }
            extrinsicsGraph_[fromId].push_back({ toId, extrinsics });                     // add the extrinsics to the graph: from -> to
            extrinsicsGraph_[toId].push_back({ fromId, inverseExtrinsics(extrinsics) });  // add the inverse extrinsics to the graph: to -> from
        }
    }
}

void StreamExtrinsicsManager::registerSameExtrinsics(const std::shared_ptr<const StreamProfile> &from, const std::shared_ptr<const StreamProfile> &to) {
    registerExtrinsics(from, to, IdentityExtrinsics);  // register the identity extrinsics
}

bool StreamExtrinsicsManager::hasExtrinsics(std::shared_ptr<const StreamProfile> from, std::shared_ptr<const StreamProfile> to) {
    if(!from || !to) {
        return false;
    }

    std::unique_lock<std::recursive_mutex> lock(mutex_);
    auto                                   fromId = getStreamProfileId(from);
    if(fromId == 0) {
        return false;
    }
    auto toId = getStreamProfileId(to);
    if(toId == 0) {
        return false;
    }

    if(fromId == toId) {
        return true;
    }

    std::vector<std::pair<uint64_t, OBExtrinsic>> path = { { fromId, IdentityExtrinsics } };
    if(!searchPath(path, fromId, toId)) {
        return false;
    }

    return true;
}

OBExtrinsic StreamExtrinsicsManager::getExtrinsics(std::shared_ptr<const StreamProfile> from, std::shared_ptr<const StreamProfile> to) {
    if(!from || !to) {
        throw invalid_value_exception("Invalid stream profile");
    }

    std::unique_lock<std::recursive_mutex> lock(mutex_);
    auto                                   fromId = getStreamProfileId(from);
    if(fromId == 0) {
        throw invalid_value_exception("From Stream profile not registered!");
    }
    auto toId = getStreamProfileId(to);
    if(toId == 0) {
        throw invalid_value_exception("To Stream profile not registered!");
    }
    if(fromId == toId) {
        return IdentityExtrinsics;
    }

    std::vector<std::pair<uint64_t, OBExtrinsic>> path = { { fromId, IdentityExtrinsics } };
    if(!searchPath(path, fromId, toId)) {
        throw invalid_value_exception(utils::string::to_string() << "Can not find path to calculate the extrinsics from" << fromId << "to" << toId);
    }

    // LOG_TRACE("Extrinsics path:");
    // for(auto iter = path.begin(); iter != path.end(); ++iter) {
    //     auto target = iter + 1;
    //     if(target == path.end()) {
    //         break;
    //     }
    //     LOG_TRACE(" - {} -> {} ", iter->first, target->first);
    // }
    if(path.size() == 2) {
        return path[1].second;
    }

    // Calculate the extrinsics according to the path
    auto extrinsics = calculateExtrinsics(path);

    // register the extrinsics `from`->`to` directly to avoid redundant calculation
    // registerExtrinsics(from, to, extrinsics);
    extrinsicsGraph_[fromId].push_back({ toId, extrinsics });                     // add the extrinsics to the graph: from -> to
    extrinsicsGraph_[toId].push_back({ fromId, inverseExtrinsics(extrinsics) });  // add the inverse extrinsics to the graph: to -> from

    return extrinsics;
}

bool StreamExtrinsicsManager::searchPath(std::vector<std::pair<uint64_t, OBExtrinsic>> &path, uint64_t fromId, uint64_t toId) const {

    // LOG_TRACE("searchPath: {} -> {}", fromId, toId);
    // Check if the from node is directly connected to the to node
    auto extList = extrinsicsGraph_.at(fromId);
    for(const auto &extPair: extList) {
        if(extPair.first == toId) {
            path.push_back(extPair);
            return true;
        }
    }

    // Recursively search for a path in the subgraph
    for(const auto &extPair: extList) {
        auto iter = streamProfileMap_.find(extPair.first);
        if(iter == streamProfileMap_.end()) {
            continue;
        }
        auto subFromId = iter->first;

        // Check if the subgraph has already been searched
        bool found = false;
        for(auto &&item: path) {
            if(item.first == subFromId) {
                found = true;
                break;
            }
        }
        if(found) {
            continue;
        }

        // recursively search for a path in the subgraph
        auto tempPath = path;
        tempPath.push_back(extPair);
        if(searchPath(tempPath, subFromId, toId)) {
            path = tempPath;
            return true;
        }
    }
    return false;
}

void StreamExtrinsicsManager::eraseStreamProfile(std::shared_ptr<const StreamProfile> sp) {
    if(!sp) {
        return;
    }
    auto uid = getStreamProfileId(sp);
    if(uid == 0) {
        return;
    }

    auto spListIter = streamProfileMap_.find(uid);
    if(spListIter == streamProfileMap_.end()) {
        return;
    }

    for(auto iter = spListIter->second.begin(); iter != spListIter->second.end();) {
        if(iter->lock() == sp) {
            iter = spListIter->second.erase(iter);
        }
        else {
            ++iter;
        }
    }

    // If the list is empty, erase the node
    if(spListIter->second.empty()) {
        streamProfileMap_.erase(spListIter);
        eraseNodeFromExtrinsicsGraph(uid);
    }
}

void StreamExtrinsicsManager::eraseNodeFromExtrinsicsGraph(uint64_t id) {
    for(auto extIter = extrinsicsGraph_.begin(); extIter != extrinsicsGraph_.end();) {
        if(extIter->first == id) {  // erase the node
            extIter = extrinsicsGraph_.erase(extIter);
        }
        else {  // erase the related edges
            for(auto iter = extIter->second.begin(); iter != extIter->second.end();) {
                if(iter->first == id) {
                    iter = extIter->second.erase(iter);
                }
                else {
                    ++iter;
                }
            }
            ++extIter;
        }
    }
}

void StreamExtrinsicsManager::cleanExpiredStreamProfiles() {
    for(auto profileEntry = streamProfileMap_.begin(); profileEntry != streamProfileMap_.end();) {
        auto &profileId            = profileEntry->first;
        auto &profileSharedPtrList = profileEntry->second;
        for(auto weakProfileIter = profileSharedPtrList.begin(); weakProfileIter != profileSharedPtrList.end();) {
            if(weakProfileIter->expired()) {
                weakProfileIter = profileSharedPtrList.erase(weakProfileIter);
            }
            else {
                ++weakProfileIter;
            }
        }

        if(profileSharedPtrList.empty()) {
            eraseNodeFromExtrinsicsGraph(profileId);
            profileEntry = streamProfileMap_.erase(profileEntry);
            continue;
        }

        if(profileSharedPtrList.size() == 1) {
            auto iter = extrinsicsGraph_.find(profileId);
            if(iter == extrinsicsGraph_.end() || iter->second.empty()) {
                eraseNodeFromExtrinsicsGraph(profileId);
                profileEntry = streamProfileMap_.erase(profileEntry);
                continue;
            }
        }

        ++profileEntry;
    }
}

uint64_t StreamExtrinsicsManager::getStreamProfileId(std::shared_ptr<const StreamProfile> profile) const {
    for(auto spIter = streamProfileMap_.begin(); spIter != streamProfileMap_.end();) {
        const auto &spList = spIter->second;
        for(auto weakSp: spList) {
            if(weakSp.lock() == profile) {
                return spIter->first;
            }
        }
        ++spIter;
    }
    return 0;  // return 0 if the stream profile is not registered
}

uint64_t StreamExtrinsicsManager::getOrRegisterStreamProfileId(std::shared_ptr<const StreamProfile> profile) {
    for(auto spIter = streamProfileMap_.begin(); spIter != streamProfileMap_.end();) {
        for(auto weakSp: spIter->second) {
            if(weakSp.lock() == profile) {
                return spIter->first;
            }
        }
        ++spIter;
    }
    uint64_t uid = 1;
    for(uint64_t i = 1; i < 0xFFFFFFFFFFFFFFFF; ++i) {
        if(streamProfileMap_.find(i) == streamProfileMap_.end()) {
            uid = i;  // find a unique id
            break;
        }
    }
    streamProfileMap_[uid].push_back(std::weak_ptr<const StreamProfile>(profile));
    return uid;
}

#if 0
class unit_test_extrinsics_manager {
public:
    unit_test_extrinsics_manager() {
        // auto ctx = Context::getInstance();
        auto manager = StreamExtrinsicsManager::getInstance();

        // Using the width as the identifier of the stream profile for print purpose
        auto sp1 = std::make_shared<VideoStreamProfile>(nullptr, OB_STREAM_VIDEO, OB_FORMAT_YUYV, 1, 0, 0);
        auto sp2 = std::make_shared<VideoStreamProfile>(nullptr, OB_STREAM_VIDEO, OB_FORMAT_YUYV, 2, 0, 0);
        auto sp3 = std::make_shared<VideoStreamProfile>(nullptr, OB_STREAM_VIDEO, OB_FORMAT_YUYV, 3, 0, 0);
        auto sp4 = std::make_shared<VideoStreamProfile>(nullptr, OB_STREAM_VIDEO, OB_FORMAT_YUYV, 4, 0, 0);
        auto sp5 = std::make_shared<VideoStreamProfile>(nullptr, OB_STREAM_VIDEO, OB_FORMAT_YUYV, 5, 0, 0);
        auto sp6 = std::make_shared<VideoStreamProfile>(nullptr, OB_STREAM_VIDEO, OB_FORMAT_YUYV, 6, 0, 0);
        auto sp7 = std::make_shared<VideoStreamProfile>(nullptr, OB_STREAM_VIDEO, OB_FORMAT_YUYV, 7, 0, 0);

        manager->registerExtrinsics(sp1, sp2, { 1, 0, 0, 0, 1, 0, 0, 0, 1, 10, 0, 0 });
        manager->registerExtrinsics(sp1, sp3, { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 10, 0 });
        manager->registerExtrinsics(sp2, sp4, { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 10 });
        manager->registerExtrinsics(sp3, sp5, { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, -8, 0 });
        manager->registerExtrinsics(sp4, sp6, { 1, 0, 0, 0, 1, 0, 0, 0, 1, -2, 0, -4 });
        manager->registerSameExtrinsics(sp7, sp6);

        // auto extrinsics12 = manager->getExtrinsics(sp1, sp2);
        // auto extrinsics13 = manager->getExtrinsics(sp1, sp3);
        // auto extrinsics24 = manager->getExtrinsics(sp2, sp4);
        // auto extrinsics35 = manager->getExtrinsics(sp3, sp5);
        // auto extrinsics46 = manager->getExtrinsics(sp4, sp6);
        // auto extrinsics16 = manager->getExtrinsics(sp1, sp6);
        // auto extrinsics61 = manager->getExtrinsics(sp6, sp1);
        auto extrinsics56 = manager->getExtrinsics(sp5, sp6);
        std::cout << "extrinsics56: " << extrinsics56.trans[0] << "," << extrinsics56.trans[1] << "," << extrinsics56.trans[2] << std::endl;
        auto extrinsics65 = manager->getExtrinsics(sp6, sp5);
        std::cout << "extrinsics65: " << extrinsics65.trans[0] << "," << extrinsics65.trans[1] << "," << extrinsics65.trans[2] << std::endl;
        auto newExtrinsics56 = manager->getExtrinsics(sp5, sp6);
        std::cout << "new extrinsics56: " << newExtrinsics56.trans[0] << "," << newExtrinsics56.trans[1] << "," << newExtrinsics56.trans[2] << std::endl;
        auto extrinsics63 = manager->getExtrinsics(sp6, sp3);
        std::cout << "extrinsics63: " << extrinsics63.trans[0] << "," << extrinsics63.trans[1] << "," << extrinsics63.trans[2] << std::endl;
        auto extrinsics57 = manager->getExtrinsics(sp5, sp7);
        std::cout << "extrinsics57: " << extrinsics57.trans[0] << "," << extrinsics57.trans[1] << "," << extrinsics57.trans[2] << std::endl;
    }
};

static unit_test_extrinsics_manager ut;
#endif

}  // namespace libobsensor