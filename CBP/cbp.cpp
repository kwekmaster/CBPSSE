#include "pch.h"

namespace CBP
{
    static auto MainInitHook_Target = IAL::Addr(35548, 0xFE);
    static auto BSTaskPool_Enter1 = IAL::Addr(35565, 0x6B8);
    static auto BSTaskPool_Enter2 = IAL::Addr(35582, 0x1C);

    static _MainInitHook MainInitHook_O;
    static BSTaskPoolProc_T SKSE_BSTaskPoolProc1_O;
    static BSTaskPoolProc_T SKSE_BSTaskPoolProc2_O;

    static UpdateTask g_updateTask;
    static ConfigReloadTask g_confReloadTask;

    static IThreadSafeBasicMemPool<AddRemoveActorTask, 8192> s_addRemoveActorTaskPool;

    static bool isHooked = false;

    static void TaskInterface1_Hook(BSTaskPool* taskpool)
    {
        SKSE_BSTaskPoolProc1_O(taskpool);
        g_updateTask.Run();
    }

    static void TaskInterface2_Hook(BSTaskPool* taskpool)
    {
        SKSE_BSTaskPoolProc2_O(taskpool);
        g_updateTask.Run();
    }

    /*static bool HookTrampoline()
    {
        uintptr_t dst1, dst2;

        // get trampoline dest addrs
        if (!Hook::GetTrampolineDst<0xE8>(BSTaskPool_Enter1, dst1) ||
            !Hook::GetTrampolineDst<0xE8>(BSTaskPool_Enter2, dst2))
        {
            return false;
        }

        // make sure both point to the same address
        if (dst1 != dst2) {
            return false;
        }

        SKSE_BSTaskPoolProc1_O = reinterpret_cast<BSTaskPoolProc_T>(dst1);

        auto ht = uintptr_t(TaskInterface1_Hook);

        // assume we'll succeed
        Hook::WriteTrampolineDst<0xE8>(BSTaskPool_Enter1, ht);
        Hook::WriteTrampolineDst<0xE8>(BSTaskPool_Enter2, ht);

        FlushInstructionCache(GetCurrentProcess(), NULL, 0);

        return true;
    }*/

    static void MainInit_Hook()
    {
        isHooked = Hook::Call5(BSTaskPool_Enter1, uintptr_t(TaskInterface1_Hook), SKSE_BSTaskPoolProc1_O) &&
            Hook::Call5(BSTaskPool_Enter2, uintptr_t(TaskInterface2_Hook), SKSE_BSTaskPoolProc2_O);

        FlushInstructionCache(GetCurrentProcess(), NULL, 0);

        if (!isHooked) {
            _FATALERROR("Hook failed");
        }
        else {
            _MESSAGE("BSTaskPool procs hooked");

            if (static_cast<int>(config["Tuning"]["reloadOnChange"]) > 0) {
                ConfigObserver::GetSingleton()->Start();
            }
        }

        MainInitHook_O();
    }

    void MessageHandler(SKSEMessagingInterface::Message* message)
    {
        if (!isHooked) {
            return;
        }

        switch (message->type)
        {
        case SKSEMessagingInterface::kMessage_InputLoaded:
        {
            SKSE::GetEventDispatcherList()->objectLoadedDispatcher.AddEventSink(
                ObjectLoadedEventHandler::GetSingleton());
            _DMESSAGE("ObjectLoaded event sink added");
        }
        break;
        }
    }

    static void QueueConfigReload()
    {
        SKSE::g_taskInterface->AddTask(&g_confReloadTask);
    }

    bool Initialize()
    {
        if (!CBPLoadConfig()) {
            _FATALERROR("Couldn't load %s", PLUGIN_CBP_CONFIG);
            return false;
        }

        // delay hooking BSTaskPool until SKSE installs it's hooks
        if (!Hook::Call5(MainInitHook_Target, uintptr_t(MainInit_Hook), MainInitHook_O)) {
            _FATALERROR("MainInit hook failed");
            return false;
        }

        SKSE::g_messaging->RegisterListener(SKSE::g_pluginHandle, "SKSE", MessageHandler);

        return true;
    }

    EventResult ObjectLoadedEventHandler::ReceiveEvent(TESObjectLoadedEvent* evn, SKSE::EventDispatcherEx<TESObjectLoadedEvent>* dispatcher)
    {
        if (evn) {
            auto form = SKSE::LookupFormByID(evn->formId);
            if (form->formType == Actor::kTypeID)
            {
                auto actor = IRTTI::Cast<Actor>(form, RTTI::TESForm, RTTI::Actor);
                if (actor != NULL)
                {
                    SKSE::ObjectHandle handle;
                    if (SKSE::GetHandle(actor, actor->formType, handle))
                    {
                        auto action = evn->loaded ?
                            AddRemoveActorTask::kActionAdd :
                            AddRemoveActorTask::kActionRemove;

                        auto cmd = AddRemoveActorTask::Create(action, handle);
                        if (cmd != NULL) {
                            SKSE::g_taskInterface->AddTask(cmd);
                        }
                    }
                }
            }
        }

        return kEvent_Continue;
    }

    __forceinline static bool isActorValid(Actor* actor)
    {
        if (actor == NULL || actor->loadedState == NULL ||
            actor->loadedState->node == NULL ||
            (actor->flags & TESForm::kFlagIsDeleted))
        {
            return false;
        }
        return true;
    }

    void UpdateTask::Run()
    {
        auto player = *SKSE::g_thePlayer;
        if (!player || !player->loadedState)
            return;

        auto cell = player->parentCell;
        if (!cell)
            return;

#ifdef _MEASURE_PERF
        auto s = PerfCounter::Query();
#endif

        // still need this since TESObjectLoadedEvent doesn't seem to give us all the actors
        for (UInt32 i = 0; i < cell->refData.maxSize; i++) {
            auto ref = cell->refData.refArray[i];
            if (ref.unk08 == NULL || ref.ref == NULL) {
                continue;
            }

            if (ref.ref->formType != Actor::kTypeID) {
                continue;
            }

            auto actor = IRTTI::Cast<Actor>(ref.ref, RTTI::TESObjectREFR, RTTI::Actor);
            if (!isActorValid(actor)) {
                continue;
            }

            SKSE::ObjectHandle handle;
            if (!SKSE::GetHandle(actor, actor->formType, handle)) {
                continue;
            }

            AddActor(actor, handle);
        }

        auto it = actors.begin();
        while (it != actors.end())
        {
            auto actor = SKSE::ResolveObject<Actor>(it->first, Actor::kTypeID);

            if (!isActorValid(actor)) {
                //Debug("Actor 0x%llX no longer valid", it->first);
                it = actors.erase(it);
            }
            else {
                it->second.update(actor);
                ++it;
            }
        }

#ifdef _MEASURE_PERF
        auto e = PerfCounter::Query();
        ee += PerfCounter::delta_us(s, e);
        c++;
        a += actors.size();

        if (e - ss > 50000000LL) {
            ss = e;
            Debug("Perf: %lld us (%zu actors)", ee / c, a / c);
            ee = 0;
            c = 0;
            a = 0;
        }
#endif
    }

    void UpdateTask::AddActor(Actor* actor, SKSE::ObjectHandle handle)
    {
        if (actors.find(handle) == actors.end())
        {
            //_DMESSAGE("Adding %llX (%s)", handle, CALL_MEMBER_FN(actor, GetReferenceName)());

            auto obj = SimObj();
            obj.bind(actor, config);

            actors.emplace(handle, obj);
        }
    }

    void UpdateTask::RemoveActor(SKSE::ObjectHandle handle)
    {
        actors.erase(handle);
    }

    void UpdateTask::UpdateConfig()
    {
        for (auto& a : actors) {
            a.second.updateConfig(config);
        }

        Message("Configuration updated");
    }

    AddRemoveActorTask* AddRemoveActorTask::Create(CBPUpdateActorAction action, SKSE::ObjectHandle handle)
    {
        auto cmd = s_addRemoveActorTaskPool.Allocate();
        if (cmd != NULL) {
            cmd->m_action = action;
            cmd->m_handle = handle;
        }
        return cmd;
    }

    void AddRemoveActorTask::Run()
    {
        if (m_action == kActionAdd) {
            auto actor = SKSE::ResolveObject<Actor>(m_handle, Actor::kTypeID);
            if (isActorValid(actor)) {
                g_updateTask.AddActor(actor, m_handle);
            }
        }
        else {
            g_updateTask.RemoveActor(m_handle);
        }
    }

    void AddRemoveActorTask::Dispose()
    {
        s_addRemoveActorTaskPool.Free(this);
    }

    void ConfigReloadTask::Run()
    {
        if (CBPLoadConfig()) {
            g_updateTask.UpdateConfig();
        }
    }

    ConfigObserver::ConfigObserver() :
        conf(PLUGIN_CBP_CONFIG),
        dir(PLUGIN_BASE_PATH),
        _thread(nullptr)
    {
        lastT.QuadPart = 0;
    }

    bool ConfigObserver::Start()
    {
        if (_thread != nullptr) {
            return false;
        }

        ULARGE_INTEGER t;
        if (GetTimestamp(&t)) {
            lastT = t;
        }

        observerHandle = ::FindFirstChangeNotification(
            dir,
            FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE);

        if (observerHandle == INVALID_HANDLE_VALUE || observerHandle == NULL) {
            Error("FindFirstChangeNotification failed");
            return false;
        }

        _thread = new std::thread(&ConfigObserver::Worker, this);

        return true;
    }

    void ConfigObserver::Shutdown()
    {
        if (_thread == nullptr) {
            return;
        }

        ::FindCloseChangeNotification(observerHandle);

        _thread->join();
        delete _thread;

        _thread = nullptr;
    }

    void ConfigObserver::Worker()
    {
        DWORD dwWaitStatus;

        Message("Starting");

        while (TRUE)
        {
            dwWaitStatus = ::WaitForSingleObject(observerHandle, INFINITE);

            if (dwWaitStatus == WAIT_OBJECT_0)
            {
                Sleep(200);
                QueueReloadOnChange();

                if (::FindNextChangeNotification(observerHandle) == FALSE) {
                    Error("FindNextChangeNotification failed");
                    break;
                }
            }
            else {
                break;
            }
        }

        ::FindCloseChangeNotification(observerHandle);
    }

    bool ConfigObserver::GetTimestamp(ULARGE_INTEGER* ul)
    {
        HANDLE fh = ::CreateFile(
            conf,
            GENERIC_READ,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (fh == INVALID_HANDLE_VALUE) {
            return false;
        }

        FILETIME ft;
        bool ret;

        if ((ret = ::GetFileTime(fh, NULL, NULL, &ft)))
        {
            ul->HighPart = ft.dwHighDateTime;
            ul->LowPart = ft.dwLowDateTime;
        }

        if (!::CloseHandle(fh)) {
            Warning("Couldn't close handle");
        }

        return ret;
    }

    void ConfigObserver::QueueReloadOnChange()
    {
        ULARGE_INTEGER t;
        if (!GetTimestamp(&t)) {
            return;
        }

        if (t.QuadPart != lastT.QuadPart) {
            lastT = t;
            QueueConfigReload();
        }
    }

}