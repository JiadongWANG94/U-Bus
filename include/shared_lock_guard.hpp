/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <mutex>
#include <shared_mutex>

class ReadingSharedLockGuard {
 public:
    ReadingSharedLockGuard() = delete;
    ReadingSharedLockGuard(std::shared_mutex &mutex) : mutex_(mutex) { mutex_.lock_shared(); }
    ~ReadingSharedLockGuard() { mutex_.unlock_shared(); }

 private:
    std::shared_mutex &mutex_;
};

class WritingSharedLockGuard {
 public:
    WritingSharedLockGuard() = delete;
    WritingSharedLockGuard(std::shared_mutex &mutex) : mutex_(mutex) { mutex_.lock(); }
    ~WritingSharedLockGuard() { mutex_.unlock(); }

 private:
    std::shared_mutex &mutex_;
};