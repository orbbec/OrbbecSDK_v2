#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include <queue>


namespace libobsensor {

class ObRTPPacketQueue {
public:
    ObRTPPacketQueue();
    ~ObRTPPacketQueue() noexcept;

    void push(const std::vector<uint8_t> &data);
    bool pop(std::vector<uint8_t> &data);
    void destroy();

private:
    std::queue<std::vector<uint8_t>> queue_;
    std::mutex                       mutex_;
    std::condition_variable          cond_var_;
    bool                             destroy_;

};

}  // namespace libobsensor
