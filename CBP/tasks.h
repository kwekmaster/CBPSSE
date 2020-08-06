#pragma once

class ITask
{
    typedef void (*RTTaskEnter_t)(void);
public:
    static void AddTaskFixed(TaskDelegateFixed* cmd);
    static void AddTask(TaskDelegate* cmd);

    static bool Initialize();
private:
    ITask() = default;

    static void RTTaskEnter_Hook();

    static std::vector<TaskDelegateFixed*> s_tasks_fixed;
    static TaskQueue s_tasks;

    static RTTaskEnter_t RTTaskEnter_O;
};
