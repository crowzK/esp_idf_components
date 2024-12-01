#pragma once

#include <functional>
#include <list>
#include <mutex>
#include <condition_variable>
#include "task.hpp"

class WorkQueue : public Task
{
public:
    using WorkFunc = std::function<void()>;
    static WorkQueue& get();
    ~WorkQueue();
    void invoke(WorkFunc&& func);

protected:
    std::mutex mutex;
    std::condition_variable cv;
    std::list<WorkFunc> workQ;

    WorkQueue();
    void task() override;
};