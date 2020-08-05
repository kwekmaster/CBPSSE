#include "pch.h"

namespace CBP
{
    namespace r3d = reactphysics3d;

    ICollision ICollision::m_Instance;

    void ICollision::RegisterCollider(SimComponent& a_sc, reactphysics3d::Collider* a_collider)
    {
        m_idMap.emplace(a_collider->getEntity().id, a_sc);
    }

    void ICollision::UnregisterCollider(reactphysics3d::Collider* a_collider)
    {
        m_idMap.erase(a_collider->getEntity().id);
    }

    void ICollision::onContact(const CollisionCallback::CallbackData& callbackData)
    {
        using EventType = CollisionCallback::ContactPair::EventType;

        for (r3d::uint p = 0; p < callbackData.getNbContactPairs(); p++)
        {
            auto contactPair = callbackData.getContactPair(p);

            //auto& globalConf = IConfig::GetGlobalConfig();

            auto col1 = contactPair.getCollider1();
            auto col2 = contactPair.getCollider2();

            auto& sc1 = m_idMap.at(col1->getEntity().id);
            auto& sc2 = m_idMap.at(col2->getEntity().id);

            auto& conf1 = sc1.GetConfig();
            auto& conf2 = sc2.GetConfig();

            float dampingMul = 1.0f;

            ASSERT(contactPair.getNbContactPoints() < 2);

            for (r3d::uint c = 0; c < contactPair.getNbContactPoints(); c++)
            {
                auto contactPoint = contactPair.getContactPoint(c);

                auto worldPoint1 = col1->getLocalToWorldTransform() * contactPoint.getLocalPointOnCollider1();
                auto worldPoint2 = col2->getLocalToWorldTransform() * contactPoint.getLocalPointOnCollider2();

                auto depth = contactPoint.getPenetrationDepth();

                dampingMul = max(depth, dampingMul);

                auto& v1 = sc1.GetVelocity();
                auto& v2 = sc2.GetVelocity();

                r3d::Vector3 vaf, vbf;

                ResolveCollision(
                    conf1.mass,
                    conf2.mass,
                    depth,
                    contactPoint.getWorldNormal(),
                    r3d::Vector3(v1.x, v1.y, v1.z),
                    r3d::Vector3(v2.x, v2.y, v2.z),
                    vaf,
                    vbf
                );

               /* _DMESSAGE("A (%s): %u,%u | %f %f: %d ye %f | %f %f %f | %f %f %f",
                    sc1.boneName.c_str(),
                    col1->getEntity().id,
                    col2->getEntity().id,
                    r3d::Vector3(v1.x, v1.y, v1.z).length(),
                    (r3d::Vector3(v1.x, v1.y, v1.z) - r3d::Vector3(v2.x, v2.y, v2.z)).length(),
                    contactPair.getEventType(),
                    contactPoint.getPenetrationDepth(),
                    v1.x,
                    v1.y,
                    v1.z,
                    vaf.x,
                    vaf.y,
                    vaf.z
                );

                _DMESSAGE("B (%s): %u,%u | %f %f: %d ye %f | %f %f %f | %f %f %f",
                    sc2.boneName.c_str(),
                    col1->getEntity().id,
                    col2->getEntity().id,
                    r3d::Vector3(v2.x, v2.y, v2.z).length(),
                    (r3d::Vector3(v1.x, v1.y, v1.z) - r3d::Vector3(v2.x, v2.y, v2.z)).length(),
                    contactPair.getEventType(),
                    contactPoint.getPenetrationDepth(),
                    v2.x,
                    v2.y,
                    v2.z,
                    vbf.x,
                    vbf.y,
                    vbf.z
                );*/


                sc1.SetVelocity(vaf);
                sc2.SetVelocity(vbf);

            }

            switch (contactPair.getEventType()) {
            case EventType::ContactStart:
            case EventType::ContactStay:
                sc2.dampingMul = sc1.dampingMul = min(dampingMul, 100.0f);
                break;
            case EventType::ContactExit:
                sc2.dampingMul = sc1.dampingMul = 1.0f;
                break;
            }

        }
    }

    void ICollision::ResolveCollision(
        float ma,
        float mb,
        float depth,
        const r3d::Vector3& normal,
        const r3d::Vector3& vai,
        const r3d::Vector3& vbi,
        r3d::Vector3& vaf,
        r3d::Vector3& vbf
    )
    {
        auto& globalConf = IConfig::GetGlobalConfig();

        float Jmod = (vbi - vai).length() * depth;
        auto Ja = (normal * Jmod);
        auto Jb = (normal * -Jmod);

        auto Bmod = globalConf.phys.timeStep * (1.0f + depth);
        auto Ba = (normal * depth) * Bmod;
        auto Bb = (normal * -depth) * Bmod;

        /*_DMESSAGE(">> %f %f %f", d.x, d.y, d.z);
        _DMESSAGE(">> %f %f %f", Ba.x, Ba.y, Ba.z);
        _DMESSAGE("");*/

        vaf = vai - (Ja * (1.0f / max(ma / globalConf.phys.timeStep, 1.0f)) + Ba);
        vbf = vbi - (Jb * (1.0f / max(mb / globalConf.phys.timeStep, 1.0f)) + Bb);
    }
}