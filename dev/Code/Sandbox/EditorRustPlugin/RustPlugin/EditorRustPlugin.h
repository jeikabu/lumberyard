
#pragma once
#ifndef CRYINCLUDE_EDITOR_RUST_PLUGIN_H

#if defined(_DEBUG)
#error debug!
#endif
//#undef AZ_ENABLE_TRACING
#undef USE_CRY_ASSERT
#include <Include/IPlugin.h>

extern "C" {
    void rust_editor_plugin_init(void* p_iSystem);
    const char* rust_editor_plugin_module_name();
    const char* rust_editor_plugin_guid();
    int rust_editor_plugin_version();
    const char* rust_editor_plugin_name();
    bool rust_editor_plugin_can_exit_now();
    void rust_editor_plugin_release();
}

class CEditorRustPlugin : public IPlugin
{
public:
	CEditorRustPlugin(IEditor* pIEditorInterface);

	void Release() override
	{
		rust_editor_plugin_release();
	}
	void ShowAbout() override
	{

	}
	const char* GetPluginGUID() override
	{
		return rust_editor_plugin_guid();
	}
	DWORD GetPluginVersion() override
	{
		return rust_editor_plugin_version();
	}
	const char* GetPluginName() override
	{
		return rust_editor_plugin_name();
	}
	bool CanExitNow() override
	{
		return rust_editor_plugin_can_exit_now();
	}
	void OnEditorNotify(EEditorNotifyEvent aEventId) override
	{

	}
};

#endif // CRYINCLUDE_EDITOR_RUST_PLUGIN_H