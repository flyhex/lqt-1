/*
 * Copyright (c) 2007-2008 Mauro Iazzi
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "lqt_common.hpp"
#include <cstring>

#include <QDebug>

static void lqtL_getenumtable (lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, LQT_ENUMS);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, LQT_ENUMS);
	}
}

static void lqtL_getpointertable (lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, LQT_POINTERS); // (1) get storage for pointers
	if (lua_isnil(L, -1)) { // (1) if there is not
		lua_pop(L, 1); // (0) pop the nil value
		lua_newtable(L); // (1) create a new one
		lua_newtable(L); // (2) create an empty metatable
		lua_pushstring(L, "v"); // (3) push the mode value: weak values are enough
		lua_setfield(L, -2, "__mode"); // (2) set the __mode field
		lua_setmetatable(L, -2); // (1) set it as the metatable
		lua_pushvalue(L, -1); // (2) duplicate the new pointer table
		lua_setfield(L, LUA_REGISTRYINDEX, LQT_POINTERS); // (1) put one copy as storage
	}
}

static void lqtL_getreftable (lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, LQT_REFS); // (1) get storage for pointers
	if (lua_isnil(L, -1)) { // (1) if there is not
		lua_pop(L, 1); // (0) pop the nil value
		lua_newtable(L); // (1) create a new one
		lua_newtable(L); // (2) create an empty metatable
		lua_pushstring(L, "kv"); // (3) push the mode value: weak values are enough
		lua_setfield(L, -2, "__mode"); // (2) set the __mode field
		lua_setmetatable(L, -2); // (1) set it as the metatable
		lua_pushvalue(L, -1); // (2) duplicate the new pointer table
		lua_setfield(L, LUA_REGISTRYINDEX, LQT_REFS); // (1) put one copy as storage
	}
}

void * lqtL_getref (lua_State *L, size_t sz) {
	void *ret = NULL;
	lqtL_getreftable(L); // (1)
	ret = lua_newuserdata(L, sz); // (2)
	lua_rawseti(L, -2, 1+lua_objlen(L, -2));
	return ret;
}

int * lqtL_tointref (lua_State *L, int index) {
	int *ret = (int*)lqtL_getref(L, sizeof(int));
	*ret = lua_tointeger(L, index);
	return ret;
}

void lqtL_pusharguments (lua_State *L, char **argv) {
	int i = 0;
	lua_newtable(L);
	for (i=0;*argv /* fix the maximum number? */;argv++,i++) {
		lua_pushstring(L, *argv);
		lua_rawseti(L, -2, i+1);
	}
	return;
}

char ** lqtL_toarguments (lua_State *L, int index) {
	char ** ret = (char**)lqtL_getref(L, sizeof(char*)*(lua_objlen(L, index)+1));
	const char *str = NULL;
	size_t strlen = 0;
	int i = 0;
	for (i=0;;i++) {
		lua_rawgeti(L, index, i+1);
		if (!lua_isstring(L, -1)) {
			str = NULL; strlen = 0;
			ret[i] = NULL;
			lua_pop(L, 1);
			break;
		} else {
			str = lua_tolstring(L, -1, &strlen);
			ret[i] = (char*)lqtL_getref(L, sizeof(char)*(strlen+1));
			strncpy(ret[i], str, strlen+1);
			lua_pop(L, 1);
		}
	}
	return ret;
}

static int lqtL_createenum (lua_State *L, lqt_Enum e[], const char *n) {
	lqtL_getenumtable(L); // (1)
	lua_newtable(L); // (2)
	lua_pushvalue(L, -1); // (3)
	lua_setfield(L, -3, n); // (2)
	while ( (e->name!=0) ) { // (2)
		lua_pushstring(L, e->name); // (3)
		lua_pushinteger(L, e->value); // (4)
		lua_settable(L, -3); // (2)
		lua_pushinteger(L, e->value); // (3)
		lua_pushstring(L, e->name); // (4)
		lua_settable(L, -3); // (2)
		e++; // (2)
	}
	lua_pop(L, 2); // (0)
	return 0;
}

int lqtL_createenumlist (lua_State *L, lqt_Enumlist list[]) {
	while (list->enums!=0 && list->name!=0) {
		lqtL_createenum(L, list->enums, list->name); // (0)
		list++;
	}
	return 0;
}

static int lqtL_gcfunc (lua_State *L) {
	if (!lua_isuserdata(L, 1) || lua_islightuserdata(L, 1)) return 0;
	lua_getfenv(L, 1); // (1)
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1); // (0)
		return 0;
	}
	lua_getfield(L, -1, "__gc"); // (2)
	lua_remove(L, -2); // (1)
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1); // (0)
		return 0;
	}
	lua_pushvalue(L, 1); // (2)
	if (lua_pcall(L, 1, 0, 0)) { // (-2;+1/+0)
		// (1)
		return lua_error(L);
	}
	return 0; // (+0)
}

static int lqtL_newindexfunc (lua_State *L) {
	lua_settop(L, 3); // (=3)
	if (!lua_isuserdata(L, 1) || lua_islightuserdata(L, 1)) return 0;
	lua_getfenv(L, 1); // (+1)
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1); // (+0)
		return 0;
	}
	lua_remove(L, 1); // (+0)
	lua_insert(L, 1); // (+0)
	lua_rawset(L, 1); // (-2)
	return 0;
}
static int lqtL_indexfunc (lua_State *L) {
	int i = 1;
	if (lua_isuserdata(L, 1) || !lua_islightuserdata(L, 1)) {
		lua_getfenv(L, 1); // (1)
		lua_pushvalue(L, 2); // (2)
		lua_gettable(L, -2); // (2)
		if (!lua_isnil(L, -1)) {
			lua_remove(L, -2);
			return 1;
		}
		lua_pop(L, 2); // (0)
	}
	lua_pushnil(L); // (+1)
	while (!lua_isnone(L, lua_upvalueindex(i))) { // (+1)
		lua_pop(L, 1); // (+0)
		lua_pushvalue(L, 2); // (+1)
		if (i==1) {
			lua_rawget(L, lua_upvalueindex(i)); // (+1)
		} else {
			lua_gettable(L, lua_upvalueindex(i)); // (+1)
		}
		if (!lua_isnil(L, -1)) break;
		i++;
	}
	return 1; // (+1)
}

static int lqtL_pushindexfunc (lua_State *L, const char *name, lqt_Base *bases) {
	int upnum = 1;
	luaL_newmetatable(L, name); // (1)
	while (bases->basename!=NULL) {
		luaL_newmetatable(L, bases->basename); // (upnum)
		upnum++;
		bases++;
	}
	lua_pushcclosure(L, lqtL_indexfunc, upnum); // (1)
	return 1;
}

int lqtL_createclasses (lua_State *L, lqt_Class *list) {
	while (list->name!=0) { // (0)
		luaL_newmetatable(L, list->name); // (1)
		luaL_register(L, NULL, list->mt); // (1)
		lua_pushstring(L, list->name); // (2)
		lua_pushboolean(L, 1); // (3)
		lua_settable(L, -3); // (1)
		lqtL_pushindexfunc(L, list->name, list->bases); // (2)
		lua_setfield(L, -2, "__index"); // (1)
		lua_pushcfunction(L, lqtL_newindexfunc); // (2)
		lua_setfield(L, -2, "__newindex"); // (1)
		lua_pushcfunction(L, lqtL_gcfunc); // (2)
		lua_setfield(L, -2, "__gc"); // (1)
		lua_pushvalue(L, -1); // (2)
		lua_setmetatable(L, -2); // (1)
		lua_pop(L, 1); // (0)
		lua_pushlstring(L, list->name, strlen(list->name)-1); // (1)
		lua_newtable(L); // (2)
		luaL_register(L, NULL, list->mt); // (2)
		lua_settable(L, LUA_GLOBALSINDEX); // (0)
		list++;
	}
	return 0;
}

bool lqtL_isinteger (lua_State *L, int i) {
	if (lua_type(L, i)==LUA_TNUMBER)
		return lua_tointeger(L, i)==lua_tonumber(L, i);
	else
		return false;
}
bool lqtL_isnumber (lua_State *L, int i) {
	return lua_type(L, i)==LUA_TNUMBER;
}
bool lqtL_isstring (lua_State *L, int i) {
	return lua_type(L, i)==LUA_TSTRING;
}
bool lqtL_isboolean (lua_State *L, int i) {
	return lua_type(L, i)==LUA_TBOOLEAN;
}
bool lqtL_missarg (lua_State *L, int index, int n) {
	bool ret = true;
	int i = 0;
	for (i=index;i<index+n;i++) {
		if (!lua_isnoneornil(L, i)) {
			ret = false;
			break;
		}
	}
	return ret;
}

static void CS(lua_State *L) {
	qDebug() << "++++++++++";
	for (int i=1;i<=lua_gettop(L);i++) {
		qDebug() << luaL_typename(L, i) << lua_touserdata(L, i);
	}
	qDebug() << "----------";
}

static void lqtL_ensurepointer (lua_State *L, const void *p) { // (+1)
	lqtL_getpointertable(L); // (1)
	lua_pushlightuserdata(L, const_cast<void*>(p)); // (2)
	lua_gettable(L, -2); // (2)
	if (lua_isnil(L, -1)) { // (2)
		lua_pop(L, 1); // (1)
		const void **pp = static_cast<const void**>(lua_newuserdata(L, sizeof(void*))); // (2)
		*pp = p; // (2)
		lua_newtable(L); // (3)
		lua_setfenv(L, -2); // (2)
		lua_pushlightuserdata(L, const_cast<void*>(p)); // (3)
		lua_pushvalue(L, -2); // (4)
		lua_settable(L, -4); // (2)
	}
	// (2)
	lua_remove(L, -2); // (1)
}

void lqtL_register (lua_State *L, const void *p) { // (+0)
	lqtL_ensurepointer(L, p);
	lua_pop(L, 1);
}

void lqtL_unregister (lua_State *L, const void *p) {
	lqtL_getpointertable(L); // (1)
	lua_pushlightuserdata(L, const_cast<void*>(p)); // (2)
	lua_gettable(L, -2); // (2)
	if (lua_isuserdata(L, -1)) {
		const void **pp = static_cast<const void**>(lua_touserdata(L, -1)); // (2)
		*pp = 0;
	}
	lua_pop(L, 1); // (1)
	lua_pushlightuserdata(L, const_cast<void*>(p)); // (2)
	lua_pushnil(L); // (3)
	lua_settable(L, -3); // (1)
	lua_pop(L, 1); // (0)
}

void lqtL_passudata (lua_State *L, const void *p, const char *name) {
	lqtL_ensurepointer(L, p); // (1)
	luaL_newmetatable(L, name); // (2)
	lua_setmetatable(L, -2); // (1)
	return;
}

void lqtL_pushudata (lua_State *L, const void *p, const char *name) {
	lqtL_ensurepointer(L, p); // (1)
	luaL_newmetatable(L, name); // (2)
	lua_setmetatable(L, -2); // (1)
	return;
}

void *lqtL_toudata (lua_State *L, int index, const char *name) {
	void *ret = 0;
	if (!lqtL_testudata(L, index, name)) return 0;
	void **pp = static_cast<void**>(lua_touserdata(L, index));
	ret = *pp;
	return ret;
}

bool lqtL_testudata (lua_State *L, int index, const char *name) {
	if (!lua_isuserdata(L, index) || lua_islightuserdata(L, index)) return false;
	lua_getfield(L, index, name);
	if (!lua_isboolean(L, -1) || !lua_toboolean(L, -1)) {
		lua_pop(L, 1);
		return false;
	}
	lua_pop(L, 1);
	return true;
}

void lqtL_pushenum (lua_State *L, int value, const char *name) {
	lqtL_getenumtable(L);
	lua_getfield(L, -1, name);
	lua_remove(L, -2);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_pushnil(L);
		return;
	}
	lua_pushnumber(L, value);
	lua_gettable(L, -2);
	lua_remove(L, -2);
}

bool lqtL_isenum (lua_State *L, int index, const char *name) {
	bool ret = false;
	if (!lua_isstring(L, index)) return false;
	lqtL_getenumtable(L);
	lua_getfield(L, -1, name);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 2);
		return false;
	}
	lua_remove(L, -2);
	lua_pushvalue(L, index);
	lua_gettable(L, -2);
	ret = lua_isnumber(L, -1);
	lua_pop(L, 2);
	return ret;
}

int lqtL_toenum (lua_State *L, int index, const char *name) {
	int ret = -1;
	lqtL_getenumtable(L);
	lua_pushvalue(L, index);
	lua_gettable(L, -2);
	ret = lua_tointeger(L, -1);
	lua_pop(L, 2);
	return ret;
}
