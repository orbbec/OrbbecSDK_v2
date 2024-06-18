#pragma once
#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"

#include <mutex>
#include <condition_variable>
#include <thread>

namespace libobsensor {

/**
 * @brief ObUsageEnvironment 覆盖BasicUsageEnvironment的log输出
 *
 */
class ObUsageEnvironment : public BasicUsageEnvironment {
public:
    static ObUsageEnvironment *createNew(TaskScheduler &taskScheduler);

    // virtual UsageEnvironment &operator<<(const std::string str);

    virtual UsageEnvironment &operator<<(char const *str) override;
    virtual UsageEnvironment &operator<<(int i) override;
    virtual UsageEnvironment &operator<<(unsigned u) override;
    virtual UsageEnvironment &operator<<(double d) override;
    virtual UsageEnvironment &operator<<(void *p) override;

protected:
    ObUsageEnvironment(TaskScheduler &taskScheduler);
    // called only by "createNew()" (or subclass constructors)
    virtual ~ObUsageEnvironment();

private:
    void outputLog();

private:
    std::thread             outputLogThread_;
    std::string             logMsg_;
    std::mutex              mutex_;
    std::condition_variable newLogCv_;
    bool                    destroy_;
    bool                    newLog_;
};

}  // namespace libobsensor