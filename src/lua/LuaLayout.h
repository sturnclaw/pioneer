
struct lay_context;
struct lua_State;

namespace LuaLayout {
	void Register();

	void PushToLua(lua_State *l, lay_context *value);
	lay_context *GetFromLua(lua_State *l, int index);
	lay_context *CheckFromLua(lua_State *l, int index);
} // namespace LuaLayout

inline void pi_lua_generic_push(lua_State *l, lay_context *value) { LuaLayout::PushToLua(l, value); }

inline void pi_lua_generic_pull(lua_State *l, int index, lay_context *&out)
{
	out = LuaLayout::CheckFromLua(l, index);
}

inline bool pi_lua_strict_pull(lua_State *l, int index, lay_context *&out)
{
	lay_context *tmp = LuaLayout::GetFromLua(l, index);
	if (tmp) {
		out = tmp;
		return true;
	}
	return false;
}
