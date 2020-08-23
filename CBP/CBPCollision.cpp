#include "pch.h"

namespace CBP
{
    namespace r3d = reactphysics3d;

    ICollision ICollision::m_Instance;

    void ICollision::onContact(const CollisionCallback::CallbackData& callbackData)
    {
        using EventType = CollisionCallback::ContactPair::EventType;

        auto& globalConf = IConfig::GetGlobalConfig();

        auto nbContactPairs = callbackData.getNbContactPairs();

        for (r3d::uint p = 0; p < nbContactPairs; p++)
        {
            auto contactPair = callbackData.getContactPair(p);

            auto col1 = contactPair.getCollider1();
            auto col2 = contactPair.getCollider2();

            auto sc1 = static_cast<SimComponent*>(col1->getUserData());
            auto sc2 = static_cast<SimComponent*>(col2->getUserData());

            if (sc1->IsSameGroup(*sc2))
                continue;

            auto type = contactPair.getEventType();

            switch (type) 
            {
            case EventType::ContactStart:                
                sc1->SetInContact(true);
                sc2->SetInContact(true);
            case EventType::ContactStay:
            {
                auto& conf1 = sc1->GetConfig();
                auto& conf2 = sc2->GetConfig();

                float dampingMul = 1.0f;

                auto nbContactPoints = contactPair.getNbContactPoints();

                for (r3d::uint c = 0; c < nbContactPoints; c++)
                {
                    auto contactPoint = contactPair.getContactPoint(c);

                    auto depth = min(contactPoint.getPenetrationDepth(),
                        globalConf.phys.colMaxPenetrationDepth);

                    dampingMul = max(depth, dampingMul);

                    auto& v1 = sc1->GetVelocity();
                    auto& v2 = sc2->GetVelocity();

                    auto& normal = contactPoint.getWorldNormal();

                    NiPoint3 vaf, vbf;

                    CollisionResponse(
                        conf1.colDepthMul,
                        conf2.colDepthMul,
                        depth,
                        NiPoint3(normal.x, normal.y, normal.z),
                        v1,
                        v2,
                        vaf,
                        vbf
                    );

                    if (type == EventType::ContactStart)
                    {
                        if (sc1->HasMovement())
                            sc1->SetDampingMul(std::clamp(dampingMul * conf1.colDampingCoef, 1.0f, 100.0f));

                        if (sc2->HasMovement())
                            sc2->SetDampingMul(std::clamp(dampingMul * conf2.colDampingCoef, 1.0f, 100.0f));
                    }

                    if (sc1->HasMovement())
                        sc1->SetVelocity2(vaf, m_timeStep);

                    if (sc2->HasMovement()) 
                        sc2->SetVelocity2(vbf, m_timeStep);                    

                }

            }
            break;
            case EventType::ContactExit:
                sc1->SetInContact(false);
                sc2->SetInContact(false);
                break;
            }

        }
    }

    void ICollision::CollisionResponse(
        float dma,
        float dmb,
        float depth,
        const NiPoint3& normal,
        const NiPoint3& vai,
        const NiPoint3& vbi,
        NiPoint3& vaf,
        NiPoint3& vbf
    )
    {
        auto len = (vai - vbi).Length();

        auto maga = len + (depth * dma);
        auto magb = len + (depth * dmb);

        vaf = (normal * (maga * depth));
        vbf = (normal * (-magb * depth));
    }
}