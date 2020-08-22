#include "pch.h"

namespace CBP
{
    namespace fs = std::filesystem;

    DCBP DCBP::m_Instance;

    constexpr char* SECTION_CBP = "CBP";
    constexpr char* CKEY_CBPFEMALEONLY = "FemaleOnly";
    constexpr char* CKEY_COMBOKEY = "ComboKey";
    constexpr char* CKEY_SHOWKEY = "ToggleKey";
    constexpr char* CKEY_UIENABLED = "UIEnabled";
    constexpr char* CKEY_DEBUGRENDERER = "DebugRenderer";
    constexpr char* CKEY_FORCEINIKEYS = "ForceINIKeys";

    inline static bool ActorValid(const Actor* actor)
    {
        if (actor == nullptr || actor->loadedState == nullptr ||
            actor->loadedState->node == nullptr ||
            (actor->flags & TESForm::kFlagIsDeleted))
        {
            return false;
        }
        return true;
    }

    void DCBP::DispatchActorTask(Actor* actor, UTTask::UTTAction action)
    {
        if (actor != nullptr) {
            SKSE::ObjectHandle handle;
            if (SKSE::GetHandle(actor, actor->formType, handle))
                m_Instance.m_updateTask.AddTask(action, handle);
        }
    }

    void DCBP::DispatchActorTask(SKSE::ObjectHandle handle, UTTask::UTTAction action)
    {
        m_Instance.m_updateTask.AddTask(action, handle);
    }

    void DCBP::UpdateConfigOnAllActors()
    {
        m_Instance.m_updateTask.AddTask(
            UTTask::kActionUpdateConfigAll);
    }

    void DCBP::UpdateGroupInfoOnAllActors()
    {
        m_Instance.m_updateTask.AddTask(
            CBP::UTTask::kActionUpdateGroupInfoAll);
    }

    void DCBP::ResetPhysics()
    {
        m_Instance.m_updateTask.AddTask(
            UTTask::kActionPhysicsReset);
    }

    void DCBP::NiNodeUpdate()
    {
        m_Instance.m_updateTask.AddTask(
            CBP::UTTask::kActionNiNodeUpdateAll);
    }

    void DCBP::NiNodeUpdate(SKSE::ObjectHandle a_handle)
    {
        m_Instance.m_updateTask.AddTask(
            CBP::UTTask::kActionNiNodeUpdate, a_handle);
    }

    void DCBP::WeightUpdate()
    {
        m_Instance.m_updateTask.AddTask(
            CBP::UTTask::kActionWeightUpdateAll);
    }

    void DCBP::ResetActors()
    {
        m_Instance.m_updateTask.AddTask(
            UTTask::kActionReset);
    }

    void DCBP::UpdateDebugRendererState()
    {
        if (!m_Instance.conf.debug_renderer)
            return;

        auto& globalConf = CBP::IConfig::GetGlobalConfig();
        if (globalConf.debugRenderer.enabled) {
            m_Instance.m_world->setIsDebugRenderingEnabled(true);
        }
        else
            m_Instance.m_world->setIsDebugRenderingEnabled(false);
    }

    void DCBP::UpdateDebugRendererSettings()
    {
        if (!m_Instance.conf.debug_renderer)
            return;

        auto& globalConf = CBP::IConfig::GetGlobalConfig();

        auto& debugRenderer = m_Instance.m_world->getDebugRenderer();

        debugRenderer.setContactPointSphereRadius(
            globalConf.debugRenderer.contactPointSphereRadius);
        debugRenderer.setContactNormalLength(
            globalConf.debugRenderer.contactNormalLength);
    }

    void DCBP::UpdateProfilerSettings()
    {
        auto& globalConf = CBP::IConfig::GetGlobalConfig();
        auto& profiler = GetProfiler();

        profiler.SetInterval(static_cast<long long>(
            max(globalConf.general.profilingInterval, 100)) * 1000);
    }

    void DCBP::ApplyForce(
        SKSE::ObjectHandle a_handle,
        uint32_t a_steps,
        const std::string& a_component,
        const NiPoint3& a_force)
    {
        DTasks::AddTask(
            new ApplyForceTask(
                a_handle,
                a_steps,
                a_component,
                a_force
            )
        );
    }

    bool DCBP::ActorHasNode(SKSE::ObjectHandle a_handle, const std::string& a_node)
    {
        auto& actors = GetSimActorList();
        auto it = actors.find(a_handle);
        if (it == actors.end())
            return false;

        return it->second.HasNode(a_node);
    }

    bool DCBP::ActorHasConfigGroup(SKSE::ObjectHandle a_handle, const std::string& a_cg)
    {
        auto& actors = GetSimActorList();
        auto it = actors.find(a_handle);
        if (it != actors.end())
            return it->second.HasConfigGroup(a_cg);

        auto& cgMap = CBP::IConfig::GetConfigGroupMap();
        auto itc = cgMap.find(a_cg);
        if (itc == cgMap.end())
            return false;

        for (const auto& e : itc->second) {
            CBP::configNode_t tmp;
            if (CBP::IConfig::GetActorNodeConfig(a_handle, e, tmp))
                if (tmp)
                    return true;
        }

        return false;
    }


    bool DCBP::GlobalHasConfigGroup(const std::string& a_cg)
    {
        auto& cgMap = CBP::IConfig::GetConfigGroupMap();
        auto itc = cgMap.find(a_cg);
        if (itc == cgMap.end())
            return true;

        for (const auto& e : itc->second) {
            CBP::configNode_t tmp;
            if (CBP::IConfig::GetGlobalNodeConfig(e, tmp)) {
                if (tmp)
                    return true;
            }
        }

        return false;
    }

    void DCBP::UIQueueUpdateCurrentActor()
    {
        if (m_Instance.conf.ui_enabled)
            m_Instance.m_updateTask.AddTask({
                UTTask::kActionUIUpdateCurrentActor });
    }

    bool DCBP::ExportData(const std::filesystem::path& a_path)
    {
        auto& iface = m_Instance.m_serialization;
        return iface.Export(a_path);
    }

    bool DCBP::ImportData(const std::filesystem::path& a_path)
    {
        auto& iface = m_Instance.m_serialization;

        bool res = iface.Import(SKSE::g_serialization, a_path);
        if (res)
            ResetActors();

        return res;
    }

    bool DCBP::ImportGetInfo(const std::filesystem::path& a_path, CBP::importInfo_t& a_out)
    {
        auto& iface = m_Instance.m_serialization;
        return iface.ImportGetInfo(a_path, a_out);
    }

    bool DCBP::SaveAll()
    {
        auto& iface = m_Instance.m_serialization;

        bool failed = false;

        failed |= !iface.SaveGlobals();
        failed |= !iface.SaveCollisionGroups();

        return !failed;
    }

    void DCBP::ResetProfiler()
    {
        m_Instance.m_updateTask.GetProfiler().Reset();
    }

    void DCBP::SetProfilerInterval(long long a_interval)
    {
        m_Instance.m_updateTask.GetProfiler().SetInterval(a_interval);
    }

    uint32_t DCBP::ConfigGetComboKey(int32_t param)
    {
        switch (param) {
        case 0:
            return 0;
        case 1:
            return DIK_LSHIFT;
        case 2:
            return DIK_RSHIFT;
        case 3:
            return DIK_LCONTROL;
        case 4:
            return DIK_RCONTROL;
        case 5:
            return DIK_LALT;
        case 6:
            return DIK_RALT;
        case 7:
            return DIK_LWIN;
        case 8:
            return DIK_RWIN;
        default:
            return DIK_LSHIFT;
        }
    }

    DCBP::DCBP() :
        m_loadInstance(0),
        uiState({ false, false }),
        m_backlog(1000)
    {
    }

    void DCBP::LoadConfig()
    {
        conf.ui_enabled = GetConfigValue(SECTION_CBP, CKEY_UIENABLED, true);
        conf.debug_renderer = GetConfigValue(SECTION_CBP, CKEY_DEBUGRENDERER, false);
        conf.force_ini_keys = GetConfigValue(SECTION_CBP, CKEY_FORCEINIKEYS, false);

        auto& globalConfig = CBP::IConfig::GetGlobalConfig();

        globalConfig.general.femaleOnly = GetConfigValue(SECTION_CBP, CKEY_CBPFEMALEONLY, true);
        globalConfig.ui.comboKey = conf.comboKey = ConfigGetComboKey(GetConfigValue(SECTION_CBP, CKEY_COMBOKEY, 1));
        globalConfig.ui.showKey = conf.showKey = std::clamp<UInt32>(
            GetConfigValue<UInt32>(SECTION_CBP, CKEY_SHOWKEY, DIK_END),
            1, InputMap::kMacro_NumKeyboardKeys - 1);
    }

    void DCBP::MainLoop_Hook(void* p1) {
        m_Instance.mainLoopUpdateFunc_o(p1);
        //DTasks::AddTask(&m_Instance.m_updateTask);
        m_Instance.m_updateTask.PhysicsTick();
    }

    void DCBP::Initialize()
    {
        m_Instance.LoadConfig();

        ASSERT(Hook::Call5(
            IAL::Addr(35551, 0x11f),
            reinterpret_cast<uintptr_t>(MainLoop_Hook),
            m_Instance.mainLoopUpdateFunc_o));

        DTasks::AddTaskFixed(&m_Instance.m_updateTask);

        IEvents::RegisterForEvent(Event::OnMessage, MessageHandler);
        IEvents::RegisterForEvent(Event::OnRevert, RevertHandler);
        IEvents::RegisterForLoadGameEvent('DPBC', LoadGameHandler);
        IEvents::RegisterForEvent(Event::OnGameSave, SaveGameHandler);
        IEvents::RegisterForEvent(Event::OnLogMessage, OnLogMessage);

        SKSE::g_papyrus->Register(RegisterFuncs);

        m_Instance.m_world = m_Instance.m_physicsCommon.createPhysicsWorld();
        m_Instance.m_world->setEventListener(std::addressof(CBP::ICollision::GetSingleton()));

        if (m_Instance.conf.debug_renderer) {
            IEvents::RegisterForEvent(Event::OnD3D11PostCreate, OnD3D11PostCreate_CBP);

            DRender::AddPresentCallback(Present_Pre);

            auto& debugRenderer = m_Instance.m_world->getDebugRenderer();
            debugRenderer.setIsDebugItemDisplayed(r3d::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
            debugRenderer.setIsDebugItemDisplayed(r3d::DebugRenderer::DebugItem::CONTACT_POINT, true);
            debugRenderer.setIsDebugItemDisplayed(r3d::DebugRenderer::DebugItem::CONTACT_NORMAL, true);

            m_Instance.Message("Debug renderer enabled");
        }

        if (m_Instance.conf.ui_enabled)
        {
            if (DUI::Initialize()) {
                DInput::RegisterForKeyEvents(&m_Instance.inputEventHandler);

                m_Instance.Message("UI enabled");
            }
        }

        IConfig::LoadConfig();

        auto& pms = CBP::GlobalProfileManager::GetSingleton<CBP::SimProfile>();
        pms.Load(PLUGIN_CBP_PROFILE_PATH);

        auto& pmn = CBP::GlobalProfileManager::GetSingleton<CBP::NodeProfile>();
        pmn.Load(PLUGIN_CBP_PROFILE_NODE_PATH);
    }

    void DCBP::OnD3D11PostCreate_CBP(Event, void* data)
    {
        auto info = static_cast<D3D11CreateEventPost*>(data);

        if (m_Instance.conf.debug_renderer)
        {
            m_Instance.m_renderer = std::make_unique<CBP::Renderer>(
                info->m_pDevice, info->m_pImmediateContext);

            m_Instance.Debug("Renderer initialized");
        }
    }

    void DCBP::Present_Pre()
    {
        Lock();

        auto& globalConf = CBP::IConfig::GetGlobalConfig();

        if (globalConf.debugRenderer.enabled &&
            globalConf.phys.collisions)
        {
            auto mm = MenuManager::GetSingleton();
            if (!mm || !mm->InPausedMenu())
                m_Instance.m_renderer->Draw();
        }

        Unlock();
    }

    void DCBP::OnLogMessage(Event, void* args)
    {
        auto str = static_cast<const char*>(args);

        m_Instance.m_backlog.Add(str);
        m_Instance.m_uiContext.LogNotify();
    }

    void DCBP::MessageHandler(Event, void* args)
    {
        auto message = static_cast<SKSEMessagingInterface::Message*>(args);

        switch (message->type)
        {
        case SKSEMessagingInterface::kMessage_InputLoaded:
            GetEventDispatcherList()->objectLoadedDispatcher.AddEventSink(EventHandler::GetSingleton());
            m_Instance.Debug("Object loaded event sink added");
            break;
        case SKSEMessagingInterface::kMessage_DataLoaded:
        {
            GetEventDispatcherList()->initScriptDispatcher.AddEventSink(EventHandler::GetSingleton());
            m_Instance.Debug("Init script event sink added");

            if (CBP::IData::PopulateRaceList())
                m_Instance.Debug("%zu TESRace forms found", CBP::IData::RaceListSize());

            auto& iface = m_Instance.m_serialization;

            PerfTimer pt;
            pt.Start();

            Lock();

            iface.LoadGlobals();
            iface.LoadCollisionGroups();
            if (iface.LoadDefaultGlobalProfile())
                CBP::IConfig::StoreDefaultGlobalProfile();

            UpdateDebugRendererState();
            UpdateDebugRendererSettings();
            UpdateProfilerSettings();

            DCBP::GetUpdateTask().UpdateTimeTick(CBP::IConfig::GetGlobalConfig().phys.timeTick);

            Unlock();

            m_Instance.Debug("%s: data loaded (%f)", __FUNCTION__, pt.Stop());
        }
        break;
        case SKSEMessagingInterface::kMessage_NewGame:
            ResetActors();
            break;
        }
    }

    template <typename T>
    bool DCBP::LoadRecord(SKSESerializationInterface* intfc, UInt32 a_type, T a_func)
    {
        PerfTimer pt;
        pt.Start();

        UInt32 actorsLength;
        if (!intfc->ReadRecordData(&actorsLength, sizeof(actorsLength)))
        {
            m_Instance.Error("[%.4s]: Couldn't read record data length", &a_type);
            return false;
        }

        if (actorsLength == 0)
        {
            m_Instance.Error("[%.4s]: Record data length == 0", &a_type);
            return false;
        }

        std::unique_ptr<char[]> data(new char[actorsLength]);

        if (!intfc->ReadRecordData(data.get(), actorsLength)) {
            m_Instance.Error("[%.4s]: Couldn't read record data", &a_type);
            return false;
        }

        auto& iface = m_Instance.m_serialization;
        auto& func = std::bind(
            a_func,
            std::addressof(iface),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);

        size_t num = func(intfc, data.get(), actorsLength);

        m_Instance.Debug("%s [%.4s]: %zu record(s), %f s", __FUNCTION__, &a_type, num, pt.Stop());

        return true;
    }

    void DCBP::LoadGameHandler(SKSESerializationInterface* intfc, UInt32, UInt32, UInt32 version)
    {
        PerfTimer pt;
        pt.Start();

        UInt32 type, length, currentVersion;

        Lock();

        while (intfc->GetNextRecordInfo(&type, &currentVersion, &length))
        {
            if (!length)
                continue;

            switch (type) {
            case 'GPBC':
                LoadRecord(intfc, type, &ISerialization::LoadGlobalProfile);
                break;
            case 'APBC':
                LoadRecord(intfc, type, &ISerialization::LoadActorProfiles);
                break;
            case 'RPBC':
                LoadRecord(intfc, type, &ISerialization::LoadRaceProfiles);
                break;
            default:
                m_Instance.Warning("Unknown record '%.4s'", &type);
                break;
            }
        }

        GetProfiler().Reset();

        Unlock();

        m_Instance.Debug("%s: %f", __FUNCTION__, pt.Stop());

        ResetActors();
    }

    template <typename T>
    bool DCBP::SaveRecord(SKSESerializationInterface* intfc, UInt32 a_type, T a_func)
    {
        PerfTimer pt;
        pt.Start();

        auto& iface = m_Instance.m_serialization;

        std::ostringstream data;
        UInt32 length;

        intfc->OpenRecord(a_type, kDataVersion1);

        size_t num = std::bind(a_func, std::addressof(iface), std::placeholders::_1)(data);
        if (num == 0)
            return false;

        std::string strData(data.str());

        length = static_cast<UInt32>(strData.length()) + 1;

        intfc->WriteRecordData(&length, sizeof(length));
        intfc->WriteRecordData(strData.c_str(), length);

        m_Instance.Debug("%s [%.4s]: %zu record(s), %f s", __FUNCTION__, &a_type, num, pt.Stop());

        return true;
    }

    void DCBP::SaveGameHandler(Event, void* args)
    {
        auto intfc = static_cast<SKSESerializationInterface*>(args);
        auto& iface = m_Instance.m_serialization;

        PerfTimer pt;
        pt.Start();

        Lock();

        iface.SaveGlobals();
        iface.SaveCollisionGroups();

        intfc->OpenRecord('DPBC', kDataVersion1);

        SaveRecord(intfc, 'GPBC', &ISerialization::SerializeGlobalProfile);
        SaveRecord(intfc, 'APBC', &ISerialization::SerializeActorProfiles);
        SaveRecord(intfc, 'RPBC', &ISerialization::SerializeRaceProfiles);

        Unlock();

        m_Instance.Debug("%s: %f", __FUNCTION__, pt.Stop());
    }

    void DCBP::RevertHandler(Event, void*)
    {
        m_Instance.Debug("Reverting..");

        Lock();

        auto& globalConf = CBP::IConfig::GetGlobalConfig();

        if (GetDriverConfig().debug_renderer)
            GetRenderer()->Clear();

        m_Instance.m_loadInstance++;

        CBP::IConfig::ClearActorConfigHolder();
        CBP::IConfig::ClearActorNodeConfigHolder();
        CBP::IConfig::ClearRaceConfigHolder();

        auto& iface = m_Instance.m_serialization;
        auto& dgp = CBP::IConfig::GetDefaultGlobalProfile();

        if (dgp.stored) {
            CBP::IConfig::SetGlobalPhysicsConfig(dgp.components);
            CBP::IConfig::SetGlobalNodeConfig(dgp.nodes);
        }
        else {
            CBP::IConfig::ClearGlobalPhysicsConfig();
            CBP::IConfig::ClearGlobalNodeConfig();

            if (iface.LoadDefaultGlobalProfile())
                CBP::IConfig::StoreDefaultGlobalProfile();
        }

        GetUpdateTask().ClearActors();

        Unlock();
    }

    auto DCBP::EventHandler::ReceiveEvent(TESObjectLoadedEvent* evn, EventDispatcher<TESObjectLoadedEvent>* dispatcher)
        -> EventResult
    {
        if (evn) {
            auto form = LookupFormByID(evn->formId);
            if (form != nullptr && form->formType == Actor::kTypeID) {
                DispatchActorTask(
                    DYNAMIC_CAST(form, TESForm, Actor),
                    evn->loaded ?
                    UTTask::kActionAdd :
                    UTTask::kActionRemove);
            }
        }

        return kEvent_Continue;
    }

    auto DCBP::EventHandler::ReceiveEvent(TESInitScriptEvent* evn, EventDispatcher<TESInitScriptEvent>* dispatcher)
        -> EventResult
    {
        if (evn != nullptr && evn->reference != nullptr) {
            if (evn->reference->formType == Actor::kTypeID) {
                DispatchActorTask(
                    DYNAMIC_CAST(evn->reference, TESObjectREFR, Actor),
                    UTTask::kActionAdd);
            }
        }

        return kEvent_Continue;
    }

    std::atomic<uint64_t> UpdateTask::m_nextGroupId = 0;

    UpdateTask::UpdateTask() :
        m_timeAccum(0.0f),
        m_averageInterval(1.0f / 60.0f),
        m_profiler(1000000)
    {
    }

    void UpdateTask::UpdateDebugRenderer()
    {
        auto& globalConf = IConfig::GetGlobalConfig();

        if (globalConf.debugRenderer.enabled &&
            DCBP::GetDriverConfig().debug_renderer)
        {
            DCBP::GetRenderer()->Update(
                DCBP::GetWorld()->getDebugRenderer());
        }
    }

    static auto frameTimerSlowTime = IAL::Addr<float*>(523660);

    void UpdateTask::UpdatePhase1()
    {
        for (auto& e : m_actors)
            e.second.UpdateVelocity();
    }

    void UpdateTask::UpdateActorsPhase2(float timeStep)
    {
        for (auto& e : m_actors)
            e.second.UpdateMovement(timeStep);
    }

    void UpdateTask::UpdatePhase2(float timeStep, float timeTick, float maxTime)
    {
        while (timeStep >= maxTime)
        {
            UpdateActorsPhase2(timeTick);
            timeStep -= timeTick;
        }

        UpdateActorsPhase2(timeStep);
    }

    void UpdateTask::UpdatePhase2Collisions(float timeStep, float timeTick, float maxTime)
    {
        auto world = DCBP::GetWorld();

        bool debugRendererEnabled = world->getIsDebugRenderingEnabled();
        world->setIsDebugRenderingEnabled(false);

        ICollision::SetTimeStep(timeTick);

        while (timeStep >= maxTime)
        {
            UpdateActorsPhase2(timeTick);
            world->update(timeTick);
            timeStep -= timeTick;
        }

        UpdateActorsPhase2(timeStep);

        ICollision::SetTimeStep(timeStep);

        world->setIsDebugRenderingEnabled(debugRendererEnabled);
        world->update(timeStep);
    }

    void UpdateTask::PhysicsTick()
    {
        auto mm = MenuManager::GetSingleton();
        if (mm && mm->InPausedMenu())
            return;

        float interval = *frameTimerSlowTime;

        if (interval < _EPSILON)
            return;

        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

        DCBP::Lock();

        auto& globalConf = IConfig::GetGlobalConfig();

        if (globalConf.phys.collisions)
            UpdateDebugRenderer();

        if (globalConf.general.enableProfiling)
            m_profiler.Begin();

        m_averageInterval = m_averageInterval * 0.875f + interval * 0.125f;
        auto timeTick = min(m_averageInterval, globalConf.phys.timeTick);

        m_timeAccum += interval;

        if (m_timeAccum > timeTick * 0.25f)
        {
            float timeStep = min(m_timeAccum, timeTick * 5.0f);
            float maxTimeStep = timeTick * 5.0f;

            if (timeStep > maxTimeStep)
                timeStep = maxTimeStep;

            float maxTime = timeTick * 1.25f;

            UpdatePhase1();

            if (globalConf.phys.collisions)
                UpdatePhase2Collisions(timeStep, timeTick, maxTime);
            else
                UpdatePhase2(timeStep, timeTick, maxTime);

            m_timeAccum = 0.0f;
        }

        if (globalConf.general.enableProfiling)
            m_profiler.End(m_actors.size());

        DCBP::Unlock();
    }

    void UpdateTask::Run()
    {
        DCBP::Lock();

        CullActors();

        auto player = *g_thePlayer;
        if (player && player->loadedState && player->parentCell)
            ProcessTasks();

        DCBP::Unlock();
    }

    void UpdateTask::CullActors()
    {
        auto it = m_actors.begin();
        while (it != m_actors.end())
        {
            auto actor = SKSE::ResolveObject<Actor>(it->first, Actor::kTypeID);

            if (!ActorValid(actor))
            {
#ifdef _CBP_SHOW_STATS
                Debug("Actor 0x%llX (%s) no longer valid", it->first, actor ? CALL_MEMBER_FN(actor, GetReferenceName)() : nullptr);
#endif
                it->second.Release();
                it = m_actors.erase(it);
            }
            else
                ++it;
        }
    }

    void UpdateTask::AddActor(SKSE::ObjectHandle a_handle)
    {
        if (m_actors.contains(a_handle))
            return;

        auto actor = SKSE::ResolveObject<Actor>(a_handle, Actor::kTypeID);
        if (!ActorValid(actor))
            return;

        if (actor->race != nullptr) {
            if (actor->race->data.raceFlags & TESRace::kRace_Child)
                return;

            if (IData::IsIgnoredRace(actor->race->formID))
                return;
        }

        auto& globalConfig = IConfig::GetGlobalConfig();

        char sex;
        auto npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
        if (npc != nullptr)
            sex = CALL_MEMBER_FN(npc, GetSex)();
        else
            sex = 0;

        if (sex == 0 && globalConfig.general.femaleOnly)
            return;

        auto& actorConf = IConfig::GetActorConf(a_handle);
        auto& nodeMap = IConfig::GetNodeMap();

        nodeDescList_t descList;
        if (!SimObject::CreateNodeDescriptorList(a_handle, actor, sex, actorConf, nodeMap, descList))
            return;

        IData::UpdateActorRaceMap(a_handle, actor);

#ifdef _CBP_SHOW_STATS
        Debug("Adding %.16llX (%s)", a_handle, CALL_MEMBER_FN(actor, GetReferenceName)());
#endif

        m_actors.try_emplace(a_handle, a_handle, actor, sex, m_nextGroupId++, descList);
    }

    void UpdateTask::RemoveActor(SKSE::ObjectHandle handle)
    {
        auto it = m_actors.find(handle);
        if (it != m_actors.end())
        {
#ifdef _CBP_SHOW_STATS
            auto actor = SKSE::ResolveObject<Actor>(handle, Actor::kTypeID);
            Debug("Removing %llX (%s)", handle, actor ? CALL_MEMBER_FN(actor, GetReferenceName)() : "nullptr");
#endif
            it->second.Release();
            m_actors.erase(it);
        }
    }

    void UpdateTask::UpdateGroupInfoOnAllActors()
    {
        for (auto& a : m_actors)
            a.second.UpdateGroupInfo();
    }

    void UpdateTask::UpdateConfigOnAllActors()
    {
        for (auto& e : m_actors)
        {
            auto actor = SKSE::ResolveObject<Actor>(e.first, Actor::kTypeID);

            if (!ActorValid(actor))
                continue;

            e.second.UpdateConfig(
                actor,
                CBP::IConfig::GetActorConf(e.first));
        }
    }

    void UpdateTask::UpdateConfig(SKSE::ObjectHandle handle)
    {
        auto it = m_actors.find(handle);
        if (it == m_actors.end())
            return;

        auto actor = SKSE::ResolveObject<Actor>(it->first, Actor::kTypeID);

        if (!ActorValid(actor))
            return;

        it->second.UpdateConfig(
            actor,
            CBP::IConfig::GetActorConf(it->first));
    }

    void UpdateTask::ApplyForce(
        SKSE::ObjectHandle a_handle,
        uint32_t a_steps,
        const std::string& a_component,
        const NiPoint3& a_force)
    {
        if (a_handle) {
            auto it = m_actors.find(a_handle);
            if (it != m_actors.end())
                it->second.ApplyForce(a_steps, a_component, a_force);
        }
        else {
            for (auto& e : m_actors)
                e.second.ApplyForce(a_steps, a_component, a_force);
        }
    }

    void UpdateTask::ClearActors()
    {
        for (auto& e : m_actors)
        {
            auto actor = SKSE::ResolveObject<Actor>(e.first, Actor::kTypeID);

            if (ActorValid(actor))
                e.second.Reset();

#ifdef _CBP_SHOW_STATS
            Debug("CLR: Removing %llX (%s)", e.first, actor ? CALL_MEMBER_FN(actor, GetReferenceName)() : "nullptr");
#endif

            e.second.Release();
        }

        m_actors.clear();
    }

    void UpdateTask::Reset()
    {
        handleSet_t handles;

        for (const auto& e : m_actors)
            handles.emplace(e.first);

        GatherActors(handles);

        ClearActors();
        for (const auto e : handles)
            AddActor(e);

        CBP::IData::UpdateActorCache(m_actors);
    }

    void UpdateTask::PhysicsReset()
    {
        for (auto& e : m_actors)
            e.second.Reset();

        auto& globalConf = IConfig::GetGlobalConfig();

        if (globalConf.phys.collisions)
            UpdateDebugRenderer();
    }

    void UpdateTask::WeightUpdate(SKSE::ObjectHandle a_handle)
    {
        auto actor = SKSE::ResolveObject<Actor>(a_handle, Actor::kTypeID);
        if (ActorValid(actor)) {
            CALL_MEMBER_FN(actor, QueueNiNodeUpdate)(true);
            DTasks::AddTask(new UpdateWeightTask(a_handle));
        }
    }

    void UpdateTask::WeightUpdateAll()
    {
        for (const auto& e : m_actors)
            WeightUpdate(e.first);
    }

    void UpdateTask::NiNodeUpdate(SKSE::ObjectHandle a_handle)
    {
        auto actor = SKSE::ResolveObject<Actor>(a_handle, Actor::kTypeID);
        if (ActorValid(actor))
            CALL_MEMBER_FN(actor, QueueNiNodeUpdate)(true);
    }

    void UpdateTask::NiNodeUpdateAll()
    {
        for (const auto& e : m_actors)
            NiNodeUpdate(e.first);
    }

    void UpdateTask::AddTask(const UTTask& task)
    {
        m_taskLock.Enter();
        m_taskQueue.push(task);
        m_taskLock.Leave();
    }

    void UpdateTask::AddTask(UTTask&& task)
    {
        m_taskLock.Enter();
        m_taskQueue.emplace(std::forward<UTTask>(task));
        m_taskLock.Leave();
    }

    void UpdateTask::AddTask(UTTask::UTTAction a_action)
    {
        m_taskLock.Enter();
        m_taskQueue.emplace(UTTask{ a_action });
        m_taskLock.Leave();
    }

    void UpdateTask::AddTask(UTTask::UTTAction a_action, SKSE::ObjectHandle a_handle)
    {
        m_taskLock.Enter();
        m_taskQueue.emplace(UTTask{ a_action, a_handle });
        m_taskLock.Leave();
    }

    bool UpdateTask::IsTaskQueueEmpty()
    {
        m_taskLock.Enter();
        bool r = m_taskQueue.size() == 0;
        m_taskLock.Leave();
        return r;
    }

    void UpdateTask::ProcessTasks()
    {
        while (!IsTaskQueueEmpty())
        {
            m_taskLock.Enter();
            auto task = m_taskQueue.front();
            m_taskQueue.pop();
            m_taskLock.Leave();

            switch (task.m_action)
            {
            case UTTask::kActionAdd:
                AddActor(task.m_handle);
                break;
            case UTTask::kActionRemove:
                RemoveActor(task.m_handle);
                break;
            case UTTask::kActionUpdateConfig:
                UpdateConfig(task.m_handle);
                break;
            case UTTask::kActionUpdateConfigAll:
                UpdateConfigOnAllActors();
                break;
            case UTTask::kActionReset:
                Reset();
                break;
            case UTTask::kActionUIUpdateCurrentActor:
                DCBP::UIQueueUpdateCurrentActorA();
                break;
            case UTTask::kActionUpdateGroupInfoAll:
                UpdateGroupInfoOnAllActors();
                break;
            case UTTask::kActionPhysicsReset:
                PhysicsReset();
                break;
            case UTTask::kActionNiNodeUpdate:
                NiNodeUpdate(task.m_handle);
                break;
            case UTTask::kActionNiNodeUpdateAll:
                NiNodeUpdateAll();
                break;
            case UTTask::kActionWeightUpdate:
                WeightUpdate(task.m_handle);
                break;
            case UTTask::kActionWeightUpdateAll:
                WeightUpdateAll();
                break;
            }
        }
    }

    void UpdateTask::GatherActors(handleSet_t& a_out)
    {
        auto player = *g_thePlayer;

        if (ActorValid(player)) {
            SKSE::ObjectHandle handle;
            if (SKSE::GetHandle(player, player->formType, handle))
                a_out.emplace(handle);
        }

        auto pl = SKSE::ProcessLists::GetSingleton();
        if (pl == nullptr)
            return;

        for (UInt32 i = 0; i < pl->highActorHandles.count; i++)
        {
            NiPointer<TESObjectREFR> ref;
            LookupREFRByHandle(pl->highActorHandles[i], ref);

            if (ref == nullptr)
                continue;

            if (ref->formType != Actor::kTypeID)
                continue;

            auto actor = DYNAMIC_CAST(ref, TESObjectREFR, Actor);
            if (!ActorValid(actor))
                continue;

            SKSE::ObjectHandle handle;
            if (!SKSE::GetHandle(actor, actor->formType, handle))
                continue;

            a_out.emplace(handle);
        }
    }

    UpdateTask::UpdateWeightTask::UpdateWeightTask(
        SKSE::ObjectHandle a_handle)
        :
        m_handle(a_handle)
    {
    }

    void UpdateTask::UpdateWeightTask::Run()
    {
        auto actor = SKSE::ResolveObject<Actor>(m_handle, Actor::kTypeID);
        if (!actor)
            return;

        TESNPC* npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
        if (npc)
        {
            BSFaceGenNiNode* faceNode = actor->GetFaceGenNiNode();
            if (faceNode) {
                CALL_MEMBER_FN(faceNode, AdjustHeadMorph)(BSFaceGenNiNode::kAdjustType_Neck, 0, 0.0f);
                UpdateModelFace(faceNode);
            }

            if (actor->actorState.IsWeaponDrawn()) {
                actor->DrawSheatheWeapon(false);
                actor->DrawSheatheWeapon(true);
            }


        }
    }

    void UpdateTask::UpdateWeightTask::Dispose() {
        delete this;
    }

    static UInt32 controlDisableFlags =
        USER_EVENT_FLAG::kAll;

    static UInt8 byChargenDisableFlags =
        PlayerCharacter::kDisableSaving |
        PlayerCharacter::kDisableWaiting;

    bool DCBP::ProcessUICallbackImpl()
    {
        Lock();

        if (m_loadInstance != m_uiContext.GetLoadInstance()) {
            uiState.show = false;
        }
        else {
            auto mm = MenuManager::GetSingleton();
            if (mm && mm->InPausedMenu())
                uiState.show = false;
            else
                m_uiContext.Draw(&uiState.show);
        }

        bool ret = uiState.show;
        if (!ret)
            DTasks::AddTask(new SwitchUITask(false));

        Unlock();

        return ret;
    }

    bool DCBP::UICallback()
    {
        return m_Instance.ProcessUICallbackImpl();
    }

    void DCBP::EnableUI()
    {
        auto player = *g_thePlayer;
        if (player) {
            player->byCharGenFlag |= byChargenDisableFlags;
        }

        auto& globalConf = CBP::IConfig::GetGlobalConfig();

        uiState.lockControls = globalConf.ui.lockControls;

        if (uiState.lockControls) {
            auto im = InputManager::GetSingleton();
            if (im) {
                im->EnableControls(controlDisableFlags, false);
            }
        }

        m_uiContext.Reset(m_loadInstance);

        CBP::IData::UpdateActorCache(GetSimActorList());
    }

    void DCBP::DisableUI()
    {
        ImGui::SaveIniSettingsToDisk(PLUGIN_IMGUI_INI_FILE);

        auto& io = ImGui::GetIO();
        if (io.WantSaveIniSettings) {
            io.WantSaveIniSettings = false;
        }

        SavePending();

        m_uiContext.Reset(m_loadInstance);

        if (uiState.lockControls) {
            auto im = InputManager::GetSingleton();
            if (im) {
                im->EnableControls(controlDisableFlags, true);
            }
        }

        auto player = *g_thePlayer;
        if (player) {
            player->byCharGenFlag &= ~byChargenDisableFlags;
        }
    }

    bool DCBP::RunEnableUIChecks()
    {
        auto mm = MenuManager::GetSingleton();
        if (mm && mm->InPausedMenu()) {
            Game::Debug::Notification("CBP UI not available while in menu");
            return false;
        }

        auto player = *g_thePlayer;
        if (player)
        {
            if (player->IsInCombat()) {
                Game::Debug::Notification("CBP UI not available while in combat");
                return false;
            }

            auto pl = SKSE::ProcessLists::GetSingleton();
            if (pl && pl->GuardsPursuing(player)) {
                Game::Debug::Notification("CBP UI not available while pursued by guards");
                return false;
            }

            auto tm = MenuTopicManager::GetSingleton();
            if (tm && tm->GetDialogueTarget() != nullptr) {
                Game::Debug::Notification("CBP UI not available while in a conversation");
                return false;
            }

            if (player->unkBDA & PlayerCharacter::FlagBDA::kAIDriven) {
                Game::Debug::Notification("CBP UI unavailable while player is AI driven");
                return false;
            }

            if (player->byCharGenFlag & PlayerCharacter::ByCharGenFlag::kAll) {
                Game::Debug::Notification("CBP UI currently unavailable");
                return false;
            }

            return true;
        }

        return false;
    }

    void DCBP::KeyPressHandler::ReceiveEvent(KeyEvent ev, UInt32 keyCode)
    {
        auto& globalConfig = CBP::IConfig::GetGlobalConfig();
        auto& driverConf = GetDriverConfig();

        UInt32 comboKey;
        UInt32 showKey;

        if (driverConf.force_ini_keys) {
            comboKey = driverConf.comboKey;
            showKey = driverConf.showKey;
        }
        else {
            comboKey = globalConfig.ui.comboKey;
            showKey = globalConfig.ui.showKey;
        }

        switch (ev)
        {
        case KeyEvent::KeyDown:
            if (comboKey && keyCode == comboKey) {
                combo_down = true;
            }
            else if (keyCode == showKey) {
                if (comboKey && !combo_down)
                    break;

                auto mm = MenuManager::GetSingleton();
                if (mm && mm->InPausedMenu())
                    break;

                DTasks::AddTask(&m_Instance.m_taskToggle);
            }
            break;
        case KeyEvent::KeyUp:
            if (comboKey && keyCode == comboKey)
                combo_down = false;
            break;
        }
    }

    void DCBP::ToggleUITask::Run()
    {
        switch (Toggle())
        {
        case ToggleResult::kResultEnabled:
            DUI::AddCallback(1, UICallback);
            break;
        case ToggleResult::kResultDisabled:
            DUI::RemoveCallback(1);
            break;
        }
    }

    auto DCBP::ToggleUITask::Toggle() ->
        ToggleResult
    {
        IScopedCriticalSection m(std::addressof(DCBP::GetLock()));

        if (m_Instance.uiState.show) {
            m_Instance.uiState.show = false;
            m_Instance.DisableUI();
            return ToggleResult::kResultDisabled;
        }
        else {
            if (m_Instance.RunEnableUIChecks()) {
                m_Instance.uiState.show = true;
                m_Instance.EnableUI();
                return ToggleResult::kResultEnabled;
            }
        }

        return ToggleResult::kResultNone;
    }

    void DCBP::SwitchUITask::Run()
    {
        if (!m_switch) {
            Lock();
            m_Instance.DisableUI();
            Unlock();
        }
    }

    DCBP::ApplyForceTask::ApplyForceTask(
        SKSE::ObjectHandle a_handle,
        uint32_t a_steps,
        const std::string& a_component,
        const NiPoint3& a_force)
        :
        m_handle(a_handle),
        m_steps(a_steps),
        m_component(a_component),
        m_force(a_force)
    {
    }

    void DCBP::ApplyForceTask::Run()
    {
        m_Instance.m_updateTask.ApplyForce(m_handle, m_steps, m_component, m_force);
    }

    void DCBP::UpdateActorCacheTask::Run()
    {
        Lock();
        CBP::IData::UpdateActorCache(GetSimActorList());
        Unlock();
    }
}