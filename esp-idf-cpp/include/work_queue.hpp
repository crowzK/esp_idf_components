#pragma once

#include <functional>
#include <list>
#include <mutex>
#include <condition_variable>
#include "task.hpp"
#include "blocking_queue.hpp"

class WorkQueue : public Task
{
public:
    using WorkFunc = std::function<void()>;
    static WorkQueue& get();
    ~WorkQueue();
    void invoke(WorkFunc&& func);

protected:
    BlockingQueue<WorkFunc> workQ;

    WorkQueue();
    void task() override;
};