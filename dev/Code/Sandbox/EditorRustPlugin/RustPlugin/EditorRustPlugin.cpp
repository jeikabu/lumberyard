#include "EditorRustPlugin.h"


CEditorRustPlugin::CEditorRustPlugin(IEditor* pIEditorInterface)
{
	ISystem* pSystem = pIEditorInterface->GetSystem();
	rust_editor_plugin_init(pSystem);
}