#include <platform_impl.h>
#include "EditorRustPlugin.h"
#include <Include/IPlugin.h>

extern "C" PLUGIN_API IPlugin* CreatePluginInstance(PLUGIN_INIT_PARAM* pInitParam)
{
    if (pInitParam->pluginVersion != SANDBOX_PLUGIN_SYSTEM_VERSION)
    {
        pInitParam->outErrorCode = IPlugin::eError_VersionMismatch;
        return 0;
    }

    //ISystem* pSystem = pInitParam->pIEditorInterface->GetSystem();
    //ModuleInitISystem(pSystem, rust_editor_plugin_module_name());
    // pSystem->GetILog()->Log("ParticleEditor plugin: CreatePluginInstance");

    return new CEditorRustPlugin(pInitParam->pIEditorInterface);
}

#ifdef _WIN32
HINSTANCE g_hInstance = 0;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_hInstance = hinstDLL;
    }

    return TRUE;
}
#endif