
#include "layouttest.h"
#include "FileSystem.h"
#include "Pi.h"
#include "core/Application.h"
#include "core/IniConfig.h"
#include "core/Log.h"
#include "graphics/opengl/RendererGL.h"
#include "imgui/imgui.h"
#include "lua/LuaEngine.h"
#include "lua/LuaLayout.h"
#include "lua/LuaObject.h"
#include "lua/LuaUtils.h"
#include "pigui/LuaPiGui.h"
#include "src/lua.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui_internal.h"
#include "lua/Lua.h"
#include <memory>

#define LAY_IMPLEMENTATION
#include <layout/layout.h>

class TestLifecycle : public Application::Lifecycle {
public:
	void Update(float deltaTime);

	lay_context *layout_ctx;

	void RenderItem(lay_id item, int depth = 0);

	LuaRef updateFunc;
};

LayoutTestApp::LayoutTestApp() :
	GuiApplication("Layout Test")
{}

void LayoutTestApp::Startup()
{
	Application::Startup();
	Log::GetLog()->SetLogFile("layouttest.txt");

	std::unique_ptr<IniConfig> config(new IniConfig());
	config->SetInt("ScrWidth", 1600);
	config->SetInt("ScrHeight", 900);
	config->SetInt("VSync", 1);

	Graphics::RendererOGL::RegisterRenderer();

	Lua::Init();
	StartupRenderer(config.get());

	StartupInput(config.get());
	// FIXME: this is to satisfy LuaEngine.cpp's reference to Pi::pigui
	Pi::pigui = StartupPiGui();

	ImGui::StyleColorsLight();

	LuaEngine::Register();
	LuaLayout::Register();
	LuaObject<Random>::RegisterClass();
	PiGui::Lua::Init();

	lua_State *l = Lua::manager->GetLuaState();
	// load the test widget file
	pi_lua_loadfile(l, *FileSystem::gameDataFiles.ReadFile("pigui/views/layouttest.lui"));
	pi_lua_protected_call(l, 0, 1);
	LuaRef exec(l, -1);

	auto *ctx = new lay_context;
	lay_init_context(ctx);
	lay_reserve_items_capacity(ctx, 1024);

	m_lifecycle = std::make_shared<TestLifecycle>();
	m_lifecycle->layout_ctx = ctx;
	m_lifecycle->updateFunc = std::move(exec);
	QueueLifecycle(m_lifecycle);
}

void LayoutTestApp::Shutdown()
{
	lay_destroy_context(m_lifecycle->layout_ctx);
	m_lifecycle.reset();

	ShutdownInput();
	ShutdownPiGui();
	ShutdownRenderer();
}

void LayoutTestApp::PreUpdate()
{
	HandleEvents();
	GetPiGui()->NewFrame();
}

void LayoutTestApp::PostUpdate()
{
	GetRenderer()->ClearDepthBuffer();
	GetPiGui()->Render();
}

void TestLifecycle::RenderItem(lay_id item, int depth)
{
	lay_vec4 root_region_rect = lay_get_rect(layout_ctx, item);

	ImDrawList *drawList = ImGui::GetWindowDrawList();
	ImVec2 item_pos = ImGui::GetWindowPos() + ImVec2(root_region_rect[0], root_region_rect[1]);
	ImVec2 item_size = ImVec2(root_region_rect[2], root_region_rect[3]);
	uint8_t backcol = std::min(20 + 20 * depth, 255);
	uint8_t textcol = backcol > 140 ? 0 : 255;
	drawList->AddRectFilled(item_pos, item_pos + item_size, IM_COL32(backcol, backcol, backcol, 255));
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(textcol, textcol, textcol, 255));
	ImGui::SetCursorPos(item_pos - ImGui::GetWindowPos());
	ImGui::Text("%x (%.f, %.f, %.f, %.f)", item, (float)root_region_rect[0], (float)root_region_rect[1], (float)root_region_rect[2], (float)root_region_rect[3]);
	ImGui::PopStyleColor(1);
}

void TestLifecycle::Update(float deltaTime)
{
	lay_reset_context(layout_ctx);

	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	ImVec2 windowPos = ImVec2(0, 0);
	ImVec2 windowSize = ImGui::GetIO().DisplaySize;

	ImGui::SetNextWindowPos(windowPos);
	ImGui::SetNextWindowSize(windowSize);
	ImGui::Begin("##fullscreen-container", nullptr, flags);

	lay_id root_region = lay_item(layout_ctx);
	lay_set_size_xy(layout_ctx, root_region, windowSize.x, windowSize.y);
	lay_set_contain(layout_ctx, root_region, LAY_ROW);

	updateFunc.PushCopyToStack();
	LuaPush<lay_context *>(updateFunc.GetLua(), layout_ctx);
	pi_lua_protected_call(updateFunc.GetLua(), 1, 0);

	lay_run_context(layout_ctx);

	std::vector<std::pair<lay_id, int>> id_stack{
		{ root_region, 0 }
	};
	lay_id current = id_stack.back().first;
	int depth = 0;

	while (!id_stack.empty()) {

		// depth-first search, push the start of the *next* subtree onto the stack as we process this tree to the root.
		lay_id child = lay_first_child(layout_ctx, current);
		lay_id sibling = lay_next_sibling(layout_ctx, current);
		RenderItem(current, depth);

		if (sibling != LAY_INVALID_ID)
			id_stack.push_back({ sibling, depth });

		if (child != LAY_INVALID_ID) {
			current = child;
			depth++;
			continue;
		} else {
			current = id_stack.back().first;
			depth = id_stack.back().second;
			id_stack.pop_back();
		}
	}

	ImGui::End();
}
