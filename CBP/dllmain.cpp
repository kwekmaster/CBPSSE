#include "pch.h"

static bool Initialize(const SKSEInterface* skse)
{
    if (!IAL::IsLoaded()) {
        gLogger.FatalError("Could not load the address library");
        return false;
    }

    if (IAL::HasBadQuery()) {
        gLogger.FatalError("One or more addresses could not be retrieved from the database");
        return false;
    }

    if (!SKSE::Initialize(skse)) {
        return false;
    }

    if (!CBP::DTasks::Initialize()) {
        gLogger.FatalError("Couldn't initialize task interface");
        return false;
    }

    if (IConfigINI::Load() != 0) {
        gLogger.Warning("Couldn't load %s", PLUGIN_INI_FILE);
    }

    CBP::IEvents::Initialize();
    CBP::DInput::Initialize();
    CBP::DCBP::Initialize();
    CBP::DRender::Initialize();

    return true;
}

extern "C"
{
    bool SKSEPlugin_Query(const SKSEInterface* skse, PluginInfo* info)
    {
        return SKSE::Query(skse, info);
    }

    bool SKSEPlugin_Load(const SKSEInterface* skse)
    {
        gLogger.Message("Initializing %s version %s (runtime %u.%u.%u.%u)",
            PLUGIN_NAME, PLUGIN_VERSION_VERSTRING,
            GET_EXE_VERSION_MAJOR(skse->runtimeVersion),
            GET_EXE_VERSION_MINOR(skse->runtimeVersion),
            GET_EXE_VERSION_BUILD(skse->runtimeVersion),
            GET_EXE_VERSION_SUB(skse->runtimeVersion));

        bool ret = Initialize(skse);

        IAL::Release();

        return ret;
    }
};