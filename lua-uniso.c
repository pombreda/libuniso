
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <uniso.h>

#define MODULE_NAME "uniso"

struct l_uniso_context {
	lua_State *L;
	int callback_ref;
};

static void l_callback(size_t current, size_t total, const char *filename,
		       void *userdata)
{
	struct l_uniso_context *ctx = (struct l_uniso_context *)userdata;
	lua_State *L = ctx->L;

	lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->callback_ref);

	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	lua_pushnumber(L, current);
	lua_pushnumber(L, total);
	lua_pushstring(L, filename);
	lua_call(L, 3, 0);
}

static int l_uniso(lua_State *L)
{
	int fd = luaL_checkinteger(L, 1);
	int result;
	struct l_uniso_context ctx;

	if (!lua_isnil(L, 2))
		luaL_checktype(L, 2, LUA_TFUNCTION);
	ctx.callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	ctx.L = L;

	result = uniso(fd, &l_callback, &ctx);
	lua_pushinteger(L, result);
	return 1;
}

static const luaL_reg methods[] = {
	{"uniso",	l_uniso},
	{NULL,		NULL},
};

LUALIB_API int luaopen_uniso(lua_State *L)
{
	luaL_openlib(L, MODULE_NAME, methods, 0);
	return 1;
}

