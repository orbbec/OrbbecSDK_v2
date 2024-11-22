#include "ObRTPPacketQueue.hpp"

namespace libobsensor {

ObRTPPacketQueue::ObRTPPacketQueue() : destroy_(false) {}

ObRTPPacketQueue::~ObRTPPacketQueue() noexcept {
    destroy();
}

void ObRTPPacketQueue::push(const std::vector<uint8_t> &data) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(destroy_) {
            return;
        }
        queue_.push(data);
    }
    cond_var_.notify_one();
}

bool ObRTPPacketQueue::pop(std::vector<uint8_t> &data) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this]() { return !queue_.empty() || destroy_; });
    if(destroy_ && queue_.empty()) {
        return false;
    }
    data = queue_.front();
    queue_.pop();
    return true;
}

void ObRTPPacketQueue::destroy() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        destroy_ = true;
    }  
    cond_var_.notify_all();
}

void ObRTPPacketQueue::reset() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        destroy_ = false;
    }
    cond_var_.notify_all();
}

}  // namespace libobsensor