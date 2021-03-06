#include "pch.h"

namespace CBP
{
    __forceinline static bool ActorValid(const Actor* actor)
    {
        if (actor == nullptr ||
            actor->loadedState == nullptr ||
            actor->loadedState->node == nullptr ||
            (actor->flags & TESForm::kFlagIsDeleted))
        {
            return false;
        }
        return true;
    }

    std::atomic<uint64_t> UpdateTask::m_nextGroupId = 0;

    UpdateTask::UpdateTask() :
        m_timeAccum(0.0f),
        m_averageInterval(1.0f / 60.0f),
        m_profiler(1000000),
        m_markedActor(0)
    {
    }

    void UpdateTask::UpdateDebugRenderer()
    {
        auto& globalConf = IConfig::GetGlobalConfig();

        if (globalConf.debugRenderer.enabled &&
            DCBP::GetDriverConfig().debug_renderer)
        {
            auto& renderer = DCBP::GetRenderer();

            try
            {
                renderer->Clear();

                if (globalConf.debugRenderer.enableMovingNodes)
                {
                    renderer->UpdateMovingNodes(
                        GetSimActorList(),
                        globalConf.debugRenderer.movingNodesRadius,
                        m_markedActor);
                }

                renderer->Update(
                    DCBP::GetWorld()->getDebugRenderer());
            }
            catch (...) {}
        }
    }

    void UpdateTask::UpdatePhase1()
    {
        for (auto& e : m_actors)
            e.second.UpdateVelocity();
    }

    void UpdateTask::UpdateActorsPhase2(float a_timeStep)
    {
        for (auto& e : m_actors)
            e.second.UpdateMovement(a_timeStep);
    }

    uint32_t UpdateTask::UpdatePhase2(float a_timeStep, float a_timeTick, float a_maxTime)
    {
        uint32_t c = 1;

        while (a_timeStep >= a_maxTime)
        {
            UpdateActorsPhase2(a_timeTick);
            a_timeStep -= a_timeTick;

            c++;
        }

        UpdateActorsPhase2(a_timeStep);

        return c;
    }

    uint32_t UpdateTask::UpdatePhase2Collisions(float a_timeStep, float a_timeTick, float a_maxTime)
    {
        uint32_t c = 1;

        auto world = DCBP::GetWorld();

        bool debugRendererEnabled = world->getIsDebugRenderingEnabled();
        world->setIsDebugRenderingEnabled(false);

        ICollision::SetTimeStep(a_timeTick);

        while (a_timeStep >= a_maxTime)
        {
            UpdateActorsPhase2(a_timeTick);
            world->update(a_timeTick);
            a_timeStep -= a_timeTick;

            c++;
        }

        UpdateActorsPhase2(a_timeStep);

        ICollision::SetTimeStep(a_timeStep);

        world->setIsDebugRenderingEnabled(debugRendererEnabled);
        world->update(a_timeStep);

        return c;
    }

#ifdef _CBP_ENABLE_DEBUG
    void UpdateTask::UpdatePhase3()
    {
        for (auto& e : m_actors)
            e.second.UpdateDebugInfo();
    }
#endif

    void UpdateTask::PhysicsTick()
    {
        auto mm = MenuManager::GetSingleton();
        if (mm && mm->InPausedMenu())
            return;

        float interval = *Game::frameTimerSlow;

        if (interval < _EPSILON)
            return;

        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

        DCBP::Lock();

        auto& globalConf = IConfig::GetGlobalConfig();

        if (globalConf.general.enableProfiling)
            m_profiler.Begin();

        UpdateDebugRenderer();

        m_averageInterval = m_averageInterval * 0.875f + interval * 0.125f;
        auto timeTick = std::min(m_averageInterval, globalConf.phys.timeTick);

        m_timeAccum += interval;

        uint32_t steps;

        if (m_timeAccum > timeTick * 0.25f)
        {
            float timeStep = std::min(m_timeAccum,
                timeTick * globalConf.phys.maxSubSteps);

            float maxTime = timeTick * 1.25f;

            UpdatePhase1();

            if (globalConf.phys.collisions)
                steps = UpdatePhase2Collisions(timeStep, timeTick, maxTime);
            else
                steps = UpdatePhase2(timeStep, timeTick, maxTime);

#ifdef _CBP_ENABLE_DEBUG
            UpdatePhase3();
#endif

            m_timeAccum = 0.0f;
        }
        else {
            steps = 0;
        }

        if (globalConf.general.enableProfiling)
            m_profiler.End(m_actors.size(), steps);

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
        if (m_actors.find(a_handle) != m_actors.end())
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

        if (globalConfig.general.armorOverrides) {
            armorOverrideResults_t ovResult;
            if (IArmor::FindOverrides(actor, ovResult))
                ApplyArmorOverride(a_handle, ovResult);
        }

        auto& actorConf = IConfig::GetActorConfAO(a_handle);
        auto& nodeMap = IConfig::GetNodeMap();

        nodeDescList_t descList;
        if (!SimObject::CreateNodeDescriptorList(
            a_handle,
            actor,
            sex,
            actorConf,
            nodeMap,
            globalConfig.phys.collisions,
            descList))
        {
            return;
        }

        IData::UpdateActorRaceMap(a_handle, actor);

#ifdef _CBP_SHOW_STATS
        Debug("Adding %.16llX (%s)", a_handle, CALL_MEMBER_FN(actor, GetReferenceName)());
#endif

        m_actors.try_emplace(a_handle, a_handle, actor, sex, m_nextGroupId++, descList);
    }

    void UpdateTask::RemoveActor(SKSE::ObjectHandle a_handle)
    {
        auto it = m_actors.find(a_handle);
        if (it != m_actors.end())
        {
#ifdef _CBP_SHOW_STATS
            auto actor = SKSE::ResolveObject<Actor>(a_handle, Actor::kTypeID);
            Debug("Removing %llX (%s)", a_handle, actor ? CALL_MEMBER_FN(actor, GetReferenceName)() : "nullptr");
#endif
            it->second.Release();
            m_actors.erase(it);

            IConfig::RemoveArmorOverride(a_handle);
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

            DoConfigUpdate(e.first, actor, e.second);
        }
    }

    void UpdateTask::UpdateConfig(SKSE::ObjectHandle a_handle)
    {
        auto it = m_actors.find(a_handle);
        if (it == m_actors.end())
            return;

        auto actor = SKSE::ResolveObject<Actor>(it->first, Actor::kTypeID);

        if (!ActorValid(actor))
            return;

        DoConfigUpdate(a_handle, actor, it->second);
    }

    void UpdateTask::DoConfigUpdate(SKSE::ObjectHandle a_handle, Actor* a_actor, SimObject& a_obj)
    {
        auto& globalConfig = IConfig::GetGlobalConfig();

        a_obj.UpdateConfig(
            a_actor,
            globalConfig.phys.collisions,
            IConfig::GetActorConfAO(a_handle));
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

    void UpdateTask::ClearActors(bool a_reset)
    {
        for (auto& e : m_actors)
        {
            if (a_reset)
            {
                auto actor = SKSE::ResolveObject<Actor>(e.first, Actor::kTypeID);
                if (ActorValid(actor))
                    e.second.Reset();
            }

#ifdef _CBP_SHOW_STATS
            auto actor = SKSE::ResolveObject<Actor>(e.first, Actor::kTypeID);
            Debug("CLR: Removing %llX (%s)", e.first, actor ? CALL_MEMBER_FN(actor, GetReferenceName)() : "nullptr");
#endif

            e.second.Release();
        }

        m_actors.clear();

        IConfig::ClearArmorOverrides();
    }

    void UpdateTask::Clear()
    {
        for (auto& e : m_actors)
            e.second.Release();

        m_actors.clear();

        IConfig::ClearArmorOverrides();
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

        IData::UpdateActorCache(m_actors);

        if (DCBP::GetDriverConfig().debug_renderer)
        {
            DCBP::GetRenderer()->Clear();
            DCBP::GetWorld()->getDebugRenderer().reset();
        }
    }

    void UpdateTask::PhysicsReset()
    {
        for (auto& e : m_actors)
            e.second.Reset();

        auto& globalConf = IConfig::GetGlobalConfig();
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

    void UpdateTask::AddArmorOverride(SKSE::ObjectHandle a_handle, SKSE::FormID a_formid)
    {
        auto& globalConfig = IConfig::GetGlobalConfig();
        if (!globalConfig.general.armorOverrides)
            return;

        auto it = m_actors.find(a_handle);
        if (it == m_actors.end())
            return;

        auto actor = SKSE::ResolveObject<Actor>(a_handle, Actor::kTypeID);
        if (!actor)
            return;

        auto form = LookupFormByID(a_formid);
        if (!form)
            return;

        if (form->formType != TESObjectARMO::kTypeID)
            return;

        auto armor = DYNAMIC_CAST(form, TESForm, TESObjectARMO);
        if (!armor)
            return;

        armorOverrideResults_t ovResults;
        if (!IArmor::FindOverrides(actor, armor, ovResults))
            return;

        auto current = IConfig::GetArmorOverride(a_handle);
        if (current) {
            armorOverrideDescriptor_t r;
            if (!BuildArmorOverride(a_handle, ovResults, *current))
                IConfig::RemoveArmorOverride(a_handle);
        }
        else {
            armorOverrideDescriptor_t r;
            if (!BuildArmorOverride(a_handle, ovResults, r))
                return;

            IConfig::SetArmorOverride(a_handle, std::move(r));
        }

        DoConfigUpdate(a_handle, actor, it->second);
    }

    void UpdateTask::UpdateArmorOverride(SKSE::ObjectHandle a_handle)
    {
        auto& globalConfig = IConfig::GetGlobalConfig();
        if (!globalConfig.general.armorOverrides)
            return;

        auto it = m_actors.find(a_handle);
        if (it == m_actors.end())
            return;

        auto actor = SKSE::ResolveObject<Actor>(a_handle, Actor::kTypeID);
        if (!actor)
            return;

        DoUpdateArmorOverride(*it, actor);
    }

    void UpdateTask::DoUpdateArmorOverride(simActorList_t::value_type& a_entry, Actor* a_actor)
    {
        bool updateConfig;

        armorOverrideResults_t ovResult;
        if (IArmor::FindOverrides(a_actor, ovResult))
            updateConfig = ApplyArmorOverride(a_entry.first, ovResult);
        else
            updateConfig = IConfig::RemoveArmorOverride(a_entry.first);

        if (updateConfig)
            DoConfigUpdate(a_entry.first, a_actor, a_entry.second);
    }

    bool UpdateTask::ApplyArmorOverride(SKSE::ObjectHandle a_handle, const armorOverrideResults_t& a_desc)
    {
        auto current = IConfig::GetArmorOverride(a_handle);

        if (current != nullptr)
        {
            if (current->first.size() == a_desc.size())
            {
                armorOverrideResults_t tmp;

                std::set_symmetric_difference(
                    current->first.begin(), current->first.end(),
                    a_desc.begin(), a_desc.end(),
                    std::inserter(tmp, tmp.begin()));

                if (tmp.empty())
                    return false;
            }
        }

        armorOverrideDescriptor_t r;
        if (!BuildArmorOverride(a_handle, a_desc, r))
            return false;

        IConfig::SetArmorOverride(a_handle, std::move(r));

        return true;
    }

    bool UpdateTask::BuildArmorOverride(
        SKSE::ObjectHandle a_handle,
        const armorOverrideResults_t& a_in,
        armorOverrideDescriptor_t& a_out)
    {
        for (const auto& e : a_in)
        {
            auto entry = IData::GetArmorCacheEntry(e);
            if (!entry) {
                Warning("[%llX] [%s] Couldn't read armor override data: %s",
                    a_handle, e.c_str(), IData::GetLastException().what());
                continue;
            }

            a_out.first.emplace(e);

            for (const auto& ea : *entry)
            {
                auto r = a_out.second.emplace(ea.first, ea.second);
                for (const auto& eb : ea.second)
                    r.first->second.insert_or_assign(eb.first, eb.second);
            }
        }

        return !a_out.first.empty();
    }

    void UpdateTask::UpdateArmorOverridesAll()
    {
        auto& globalConfig = IConfig::GetGlobalConfig();
        if (!globalConfig.general.armorOverrides)
            return;

        for (auto& e : m_actors) {

            auto actor = SKSE::ResolveObject<Actor>(e.first, Actor::kTypeID);
            if (!actor)
                return;

            DoUpdateArmorOverride(e, actor);
        }
    }

    void UpdateTask::ClearArmorOverrides()
    {
        IConfig::ClearArmorOverrides();
        UpdateConfigOnAllActors();
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

    void UpdateTask::AddTask(UTTask::UTTAction a_action, SKSE::ObjectHandle a_handle, SKSE::FormID a_formid)
    {
        m_taskLock.Enter();
        m_taskQueue.emplace(UTTask{ a_action, a_handle, a_formid });
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
            case UTTask::UTTAction::Add:
                AddActor(task.m_handle);
                break;
            case UTTask::UTTAction::Remove:
                RemoveActor(task.m_handle);
                break;
            case UTTask::UTTAction::UpdateConfig:
                UpdateConfig(task.m_handle);
                break;
            case UTTask::UTTAction::UpdateConfigAll:
                UpdateConfigOnAllActors();
                break;
            case UTTask::UTTAction::Reset:
                Reset();
                break;
            case UTTask::UTTAction::UIUpdateCurrentActor:
                DCBP::UIQueueUpdateCurrentActorA();
                break;
            case UTTask::UTTAction::UpdateGroupInfoAll:
                UpdateGroupInfoOnAllActors();
                break;
            case UTTask::UTTAction::PhysicsReset:
                PhysicsReset();
                break;
            case UTTask::UTTAction::NiNodeUpdate:
                NiNodeUpdate(task.m_handle);
                break;
            case UTTask::UTTAction::NiNodeUpdateAll:
                NiNodeUpdateAll();
                break;
            case UTTask::UTTAction::WeightUpdate:
                WeightUpdate(task.m_handle);
                break;
            case UTTask::UTTAction::WeightUpdateAll:
                WeightUpdateAll();
                break;
            case UTTask::UTTAction::AddArmorOverride:
                AddArmorOverride(task.m_handle, task.m_formid);
                break;
            case UTTask::UTTAction::UpdateArmorOverride:
                UpdateArmorOverride(task.m_handle);
                break;
            case UTTask::UTTAction::UpdateArmorOverridesAll:
                UpdateArmorOverridesAll();
                break;
            case UTTask::UTTAction::ClearArmorOverrides:
                ClearArmorOverrides();
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
        }
