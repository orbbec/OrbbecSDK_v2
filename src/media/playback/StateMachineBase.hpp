// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "logger/Logger.hpp"

#include <mutex>
#include <functional>
#include <vector>
#include <unordered_map>
#include <queue>

namespace libobsensor {

class TaskQueue {
public:
    TaskQueue() : running_(false) {}
    virtual ~TaskQueue() {
        stop();
    }

    void start() {
        running_ = true;
        worker_  = std::thread([this]() {
            while(running_) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() { return !running_ || !tasks_.empty(); });
                    if(!running_ && tasks_.empty())
                        break;
                    if(!tasks_.empty()) {
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                }
                if(task)
                    task();
            }
        });
    }

    void stop() {
        LOG_DEBUG("Try stop StateMachine...");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_all();
        if(worker_.joinable()) {
            worker_.join();
        }
        LOG_DEBUG("Stop StateMachine done...");
    }

    void pushTask(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    bool isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

private:
    std::atomic<bool>                 running_;
    mutable std::mutex                mutex_;
    std::condition_variable           cv_;
    std::queue<std::function<void()>> tasks_;
    std::thread                       worker_;
};

template <typename State> class StateMachineBase {
public:
    using Callback = std::function<void()>;

    explicit StateMachineBase(State initialState) : started_(false), shutdown_(false), currentState_(initialState) {
        LOG_DEBUG("StateMachine init");
    }

    virtual ~StateMachineBase() {
        LOG_DEBUG("StateMachine destory");
        shutdown();
    }

    // todo: return a token to unregister the callback
    void registerEnterCallback(State state, Callback callback, bool sync = false) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(sync) {
            enterSyncCallbacks_[state].emplace_back(std::move(callback));
        }
        else {
            enterAsyncCallbacks_[state].emplace_back(std::move(callback));
            startIfNeed();
        }
    }

    void registerExitCallback(State state, Callback callback, bool sync = false) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(sync) {
            exitSyncCallbacks_[state].emplace_back(std::move(callback));
        }
        else {
            exitAsyncCallbacks_[state].emplace_back(std::move(callback));
            startIfNeed();
        }
    }

    void registerGlobalCallback(Callback callback, bool sync = false) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(sync) {
            globalSyncCallbacks_.emplace_back(std::move(callback));
        }
        else {
            globalAsyncCallbacks_.emplace_back(std::move(callback));
            startIfNeed();
        }
    }

    bool transitionTo(State newState) {
        State oldState{};
        bool  success = false;

        std::vector<Callback> syncExitCbs;
        std::vector<Callback> syncEnterCbs;
        std::vector<Callback> syncGlobalCbs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            success = validateTransition(currentState_, newState);
            if(success) {
                oldState      = currentState_;
                currentState_ = newState;

                syncExitCbs   = getSyncCallbacks(exitSyncCallbacks_, oldState);
                syncEnterCbs  = getSyncCallbacks(enterSyncCallbacks_, newState);
                syncGlobalCbs = globalSyncCallbacks_;
            }
        }

        if(success) {
            triggerCallbacks(syncExitCbs);
            triggerCallbacks(syncEnterCbs);
            triggerCallbacks(syncGlobalCbs);

            taskQueue_.pushTask([this, oldState, newState]() {
                // Collecting callbacks that need to be executed (without holding a lock to execute them)
                auto exitCbs   = getAsyncCallbacks(exitAsyncCallbacks_, oldState);
                auto enterCbs  = getAsyncCallbacks(enterAsyncCallbacks_, newState);
                auto globalCbs = getAsyncGlobalCallbacks();

                triggerCallbacks(exitCbs);    // trigger exit callbacks first
                triggerCallbacks(enterCbs);   // trigger enter callbacks second
                triggerCallbacks(globalCbs);  // trigger global callbacks last
            });
        }

        return success;
    }

    State getCurrentState() const {
        return currentState_;
    }

    void clearGlobalCallbacks() {
        std::lock_guard<std::mutex> lock(mutex_);
        globalSyncCallbacks_.clear();
        globalAsyncCallbacks_.clear();
    }

    void clearAllCallbacks() {
        std::lock_guard<std::mutex> lock(mutex_);
        enterSyncCallbacks_.clear();
        exitSyncCallbacks_.clear();
        globalSyncCallbacks_.clear();

        enterAsyncCallbacks_.clear();
        exitAsyncCallbacks_.clear();
        globalAsyncCallbacks_.clear();
    }

protected:
    // inheritors should implement this to validate the transition, or overload operator==()
    virtual bool validateTransition(const State &oldState, const State &newState) const {
        return oldState != newState;
    }

    void triggerCallbacks(std::vector<Callback> &callbacks) {
        for(const auto &cb: callbacks) {
            try {
                cb();
            }
            catch(const std::exception &e) {
                LOG_DEBUG("Exception in callback: {}", e.what());
            }
        }
    }

    std::vector<Callback> getSyncCallbacks(const std::unordered_map<State, std::vector<Callback>> &callbackMap, State state) {
        auto it = callbackMap.find(state);
        if(it != callbackMap.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<Callback> getAsyncCallbacks(const std::unordered_map<State, std::vector<Callback>> &callbackMap, State state) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = callbackMap.find(state);
        if(it != callbackMap.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<Callback> getAsyncGlobalCallbacks() {
        std::lock_guard<std::mutex> lock(mutex_);
        return globalAsyncCallbacks_;  // return copy of vector
    }

protected:
    void startIfNeed() {
        if (!started_.exchange(true)) {
            taskQueue_.start();
            LOG_DEBUG("TaskQueue started by async callback registration");
        }
    }

    void shutdown() {
        if (!shutdown_.exchange(true)) {
            clearAllCallbacks();
            taskQueue_.stop();
        }
    }

protected:
    std::atomic<bool> started_;
    std::atomic<bool> shutdown_;

    mutable std::mutex mutex_;
    State              currentState_;

    std::unordered_map<State, std::vector<Callback>> enterAsyncCallbacks_;
    std::unordered_map<State, std::vector<Callback>> exitAsyncCallbacks_;
    std::vector<Callback>                            globalAsyncCallbacks_;

    std::unordered_map<State, std::vector<Callback>> enterSyncCallbacks_;
    std::unordered_map<State, std::vector<Callback>> exitSyncCallbacks_;
    std::vector<Callback>                            globalSyncCallbacks_;

    TaskQueue taskQueue_;
};

}  // namespace libobsensor
