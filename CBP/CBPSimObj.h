#pragma once

namespace CBP
{
    struct nodeDesc_t 
    {
        std::string nodeName;
        BSFixedString cs;
        NiAVObject* bone;
        const std::string &confGroup;
        const configComponent_t& conf;
        bool collisions;
        bool movement;
    };

    typedef std::vector< nodeDesc_t> nodeDescList_t;

    class SimObject
    {
    public:
        SimObject(
            SKSE::ObjectHandle a_handle,
            Actor* actor, 
            char a_sex,
            uint64_t a_Id,
            const nodeDescList_t& a_desc);

        SimObject() = delete;
        SimObject(const SimObject& a_rhs) = delete;
        SimObject(SimObject&& a_rhs) = delete;

        void Update(Actor* actor, uint32_t a_step);
        void UpdateConfig(const configComponents_t& config);
        void Reset(Actor* a_actor);

        void ApplyForce(uint32_t a_steps, const std::string& a_component, const NiPoint3& a_force);
        void UpdateGroupInfo();

        void Release();

        [[nodiscard]] inline bool HasNode(const std::string &a_node) const {
            return m_things.contains(a_node);
        }
        
        [[nodiscard]] inline bool HasConfigGroup(const std::string &a_cg) const {
            return m_configGroups.contains(a_cg);
        }

        [[nodiscard]] static auto CreateNodeDescriptorList(
            SKSE::ObjectHandle a_handle,
            Actor* a_actor, 
            char a_sex,
            const configComponents_t& a_config, 
            const nodeMap_t& a_nodeMap, 
            nodeDescList_t &a_out) 
            -> nodeDescList_t::size_type;

    private:

        std::unordered_map<std::string, SimComponent> m_things;
        std::unordered_set<std::string> m_configGroups;

        uint64_t m_Id;
        SKSE::ObjectHandle m_handle;

        char m_sex;

       // [[nodiscard]] static bool CheckNode(Actor *a_actor, const configComponents_t& a_config, const nodeMap_t::value_type &a_pair);
    };

    typedef std::unordered_map<SKSE::ObjectHandle, SimObject> simActorList_t;
}