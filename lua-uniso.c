
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <uniso.h>

#define MODULE_NAME "uniso"

#ifndef VERSION
#define VERSION "unknown version"
#endif

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
	int fd, result;
	struct l_uniso_context ctx;
	const char *filename = NULL;

	if (lua_isnumber(L, 1)) {
		fd =luaL_checkinteger(L, 1);
	} else if (lua_isstring(L, 1)) {
		filename = luaL_checkstring(L, 1);
		fd = open(filename, O_RDONLY);
		if (fd < 0) {
			lua_pushnil(L);
			lua_pushstring(L, strerror(errno));
			return 2;
		}
	} else {
		luaL_typerror(L, 1, "integer or string");
	}

	if (!lua_isnil(L, 2))
		luaL_checktype(L, 2, LUA_TFUNCTION);
	ctx.callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	ctx.L = L;

	result = uniso(fd, &l_callback, &ctx);
	if (filename != NULL)
		close(fd);

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
	lua_pushliteral(L, "version");
	lua_pushliteral(L, VERSION);
	lua_settable(L, -3);
	return 1;
}

