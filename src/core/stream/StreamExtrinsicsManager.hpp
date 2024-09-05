#pragma once

#include "libobsensor/h/ObTypes.h"
#include "StreamProfile.hpp"
#include <map>
#include <memory>
#include <vector>

namespace libobsensor {
constexpr OBExtrinsic IdentityExtrinsics = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, { 0, 0, 0 } };
class StreamExtrinsicsManager {
private:
    StreamExtrinsicsManager();

    static std::mutex                             instanceMutex_;
    static std::weak_ptr<StreamExtrinsicsManager> instanceWeakPtr_;

public:
    static std::shared_ptr<StreamExtrinsicsManager> getInstance();

    ~StreamExtrinsicsManager() noexcept;

    void registerExtrinsics(const std::shared_ptr<const StreamProfile> &from, const std::shared_ptr<const StreamProfile> &to, const OBExtrinsic &extrinsics);
    void registerSameExtrinsics(const std::shared_ptr<const StreamProfile> &from, const std::shared_ptr<const StreamProfile> &to);

    bool        hasExtrinsics(std::shared_ptr<const StreamProfile> from, std::shared_ptr<const StreamProfile> to);
    OBExtrinsic getExtrinsics(std::shared_ptr<const StreamProfile> from, std::shared_ptr<const StreamProfile> to);

private:
    void cleanExpiredStreamProfiles();
    void eraseStreamProfile(std::shared_ptr<const StreamProfile> sp);
    void eraseNodeFromExtrinsicsGraph(uint64_t id);

    bool searchPath(std::vector<std::pair<uint64_t, OBExtrinsic>> &path, uint64_t fromId, uint64_t toId) const;

    uint64_t getOrRegisterStreamProfileId(std::shared_ptr<const StreamProfile> profile);
    uint64_t getStreamProfileId(std::shared_ptr<const StreamProfile> profile) const;

private:
    std::recursive_mutex                                                mutex_;
    std::map<uint64_t, std::vector<std::weak_ptr<const StreamProfile>>> streamProfileMap_;  // vertices
    std::map<uint64_t, std::vector<std::pair<uint64_t, OBExtrinsic>>>   extrinsicsGraph_;   // graph adjacency list
};

}  // namespace libobsensor