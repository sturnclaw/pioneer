
#include "LuaLayout.h"
#include "LuaMetaType.h"
#include "LuaPushPull.h"
#include "LuaVector2.h"
#include "pigui/LuaFlags.h"
#include "src/lua.h"
#include "vector2.h"

#include <layout/layout.h>

static LuaMetaTypeGeneric s_metaType("LayoutContext");

void LuaLayout::PushToLua(lua_State *l, lay_context *value)
{
	auto newudata = static_cast<lay_context **>(lua_newuserdata(l, sizeof(lay_context *)));
	*newudata = value;
	s_metaType.GetMetatable();
	lua_setmetatable(l, -2);
}

lay_context *LuaLayout::GetFromLua(lua_State *l, int index)
{
	if (!lua_isuserdata(l, index))
		return nullptr;

	lua_getmetatable(l, index);
	if (lua_isnil(l, -1)) {
		lua_pop(l, 1);
		return nullptr;
	}

	s_metaType.GetMetatable();
	bool eq = lua_rawequal(l, -1, -2);
	lua_pop(l, 2);
	return eq ? *static_cast<lay_context **>(lua_touserdata(l, index)) : nullptr;
}

lay_context *LuaLayout::CheckFromLua(lua_State *l, int index)
{
	lay_context *ret = GetFromLua(l, index);
	if (ret == nullptr)
		luaL_error(l, "%s expected, got %s", s_metaType.GetTypeName(), lua_typename(l, lua_type(l, index)));

	return ret;
}

LuaFlags<lay_layout_flags> s_layoutFlags{
	{ "Center", LAY_CENTER },
	{ "Fill", LAY_FILL },
	{ "HFill", LAY_HFILL },
	{ "VFill", LAY_VFILL },
	{ "Left", LAY_LEFT },
	{ "Top", LAY_TOP },
	{ "Right", LAY_RIGHT },
	{ "Bottom", LAY_BOTTOM }
};

LuaFlags<lay_box_flags> s_boxFlags{
	{ "Free", LAY_LAYOUT },
	{ "Row", LAY_ROW },
	{ "Column", LAY_COLUMN },
	{ "Start", LAY_START },
	{ "Middle", LAY_MIDDLE },
	{ "End", LAY_END },
	{ "Justify", LAY_JUSTIFY },
};

static lay_layout_flags parse_layout_flags(lua_State *l, int val)
{
	std::string flags = LuaPull<std::string>(l, val, "Center");
	int ret = LAY_CENTER;

	const size_t pipe_pos = flags.find_first_of('|');
	auto iter = std::find_if(s_layoutFlags.LUT.cbegin(), s_layoutFlags.LUT.cend(),
		[&](const std::pair<const char *, lay_layout_flags> &val) -> bool {
			return flags.compare(0, pipe_pos, val.first) == 0;
		});
	if (iter != s_layoutFlags.LUT.cend())
		ret |= iter->second;

	if (pipe_pos != std::string::npos) {
		iter = std::find_if(s_layoutFlags.LUT.cbegin(), s_layoutFlags.LUT.cend(),
			[&](const std::pair<const char *, lay_layout_flags> &val) -> bool {
				return flags.compare(pipe_pos + 1, std::string::npos, val.first) == 0;
			});
		if (iter != s_layoutFlags.LUT.cend())
			ret |= iter->second;
	}

	return lay_layout_flags(ret);
}

static lay_box_flags parse_box_flags(lua_State *l, int val)
{
	std::string flags = LuaPull<std::string>(l, val, "Free");
	int ret = LAY_CENTER;

	const size_t pipe_pos = flags.find_first_of('|');
	auto iter = std::find_if(s_boxFlags.LUT.cbegin(), s_boxFlags.LUT.cend(),
		[&](const std::pair<const char *, lay_box_flags> &val) -> bool {
			return flags.compare(0, pipe_pos, val.first) == 0;
		});
	if (iter != s_boxFlags.LUT.cend())
		ret |= iter->second;

	if (pipe_pos != std::string::npos) {
		iter = std::find_if(s_boxFlags.LUT.cbegin(), s_boxFlags.LUT.cend(),
			[&](const std::pair<const char *, lay_box_flags> &val) -> bool {
				return flags.compare(pipe_pos + 1, std::string::npos, val.first) == 0;
			});
		if (iter != s_boxFlags.LUT.cend())
			ret |= iter->second;
	}

	if (lua_isboolean(l, val + 1) && lua_toboolean(l, val + 1))
		ret |= LAY_WRAP;

	return lay_box_flags(ret);
}

void LuaLayout::Register()
{
	s_metaType.CreateMetaType(Lua::manager->GetLuaState());

	s_metaType.StartRecording()
		.AddFunction("item", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id item = lay_item(ctx);
			LuaPush<uint32_t>(l, item);
			return 1;
		})
		.AddFunction("set_size", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id item = LuaPull<uint32_t>(l, 2);
			vector2f size = LuaVector2::CheckFromLuaF(l, 3);
			lay_set_size_xy(ctx, item, size.x, size.y);
			return 0;
		})
		.AddFunction("set_margins", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id item = LuaPull<uint32_t>(l, 2);
			LuaTable t(l, 3);
			lay_vec4 margins = {};
			margins[0] = (lay_scalar)t.Get<float>(1, 0);
			margins[1] = (lay_scalar)t.Get<float>(2, 0);
			margins[2] = (lay_scalar)t.Get<float>(3, 0);
			margins[3] = (lay_scalar)t.Get<float>(4, 0);
			lay_set_margins(ctx, item, margins);
			return 0;
		})
		.AddFunction("set_behavior", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id item = LuaPull<uint32_t>(l, 2);
			lay_set_behave(ctx, item, parse_layout_flags(l, 3));
			return 0;
		})
		.AddFunction("set_container", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id item = LuaPull<uint32_t>(l, 2);
			lay_set_contain(ctx, item, parse_box_flags(l, 3));
			return 0;
		})
		.AddFunction("append", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id earlier = LuaPull<uint32_t>(l, 2);
			lay_id later = LuaPull<uint32_t>(l, 3);
			lay_append(ctx, earlier, later);
			return 0;
		})
		.AddFunction("insert", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id parent = LuaPull<uint32_t>(l, 2);
			lay_id child = LuaPull<uint32_t>(l, 3);
			lay_insert(ctx, parent, child);
			return 0;
		})
		.AddFunction("get_rect", [](lua_State *l) -> int {
			lay_context *ctx = LuaPull<lay_context *>(l, 1);
			lay_id item = LuaPull<uint32_t>(l, 2);
			lay_vec4 rect = lay_get_rect(ctx, item);
			LuaVector2::PushToLuaF(l, vector2f(rect[0], rect[1]));
			LuaVector2::PushToLuaF(l, vector2f(rect[2], rect[3]));
			return 2;
		})
		.StopRecording();
}
