#include "timer.h"
#include "params.h"
#include "helpers.h"

#include "lua.h"

#undef uthash_nonfatal_oom
#define uthash_nonfatal_oom(elt) ut_oom_recover(elt)

static bool oom = false;
static void ut_oom_recover(void *elem)
{
	oom = true;
}

static unsigned int timer_n=0;


static void TimerPoolDestroyItem(timer_pool *elem)
{
	free(elem->str);
	free(elem->func);
	luaL_unref(params.L, LUA_REGISTRYINDEX, elem->lua_ref);
}
void TimerPoolDel(timer_pool **pp, timer_pool *p)
{
	TimerPoolDestroyItem(p);
	HASH_DEL(*pp,p);
	free(p);
}
void TimerPoolDestroy(timer_pool **pp)
{
	timer_pool *elem, *tmp;
	HASH_ITER(hh, *pp, elem, tmp) TimerPoolDel(pp,elem);
}
struct timer_pool *TimerPoolSearch(timer_pool *p, const char *str)
{
	timer_pool *elem_find;
	HASH_FIND_STR(p, str, elem_find);
	return elem_find;
}
struct timer_pool *TimerPoolAdd(timer_pool **pp, const char *str, const char *func, uint64_t period, bool oneshot)
{
	ADD_STR_POOL(timer_pool, pp, str, strlen(str))
	if (!(elem->func = strdup(func)))
	{
		TimerPoolDel(pp,elem);
		return NULL;
	}
	elem->period = period;
	elem->oneshot = oneshot;
	elem->lua_ref = LUA_NOREF;
	elem->bt_prev = boottime_ms();
	elem->n = ++timer_n;
	elem->fires = 0;
	return elem;
}

static bool TimerPoolRunTimer(timer_pool *p)
{
	lua_getglobal(params.L, p->func);
	if (!lua_isfunction(params.L, -1))
	{
		lua_pop(params.L, 1);
		DLOG_ERR("timer: '%s' function '%s' does not exist\n",p->str,p->func);
		return false;
	}
	lua_pushstring(params.L, p->str);
	lua_rawgeti(params.L, LUA_REGISTRYINDEX, p->lua_ref);
	p->fires++;
	DLOG("\ntimer: '%s' function '%s' period %llu oneshot %u fires=%u\n",p->str,p->func,p->period,p->oneshot,p->fires);
	int status = lua_pcall(params.L, 2, 0, 0);
	if (status)
	{
		lua_dlog_error();
		return false;
	}
	return true;
}
uint64_t TimerPoolNext(const timer_pool *p, bool *dirty)
{
	const timer_pool *elem, *tmp;
	uint64_t nexttime, mintime=0x7FFFFFFFFFFFFFFF;

	if (!p) return 0;

	HASH_ITER(hh, p, elem, tmp)
	{
		nexttime = elem->bt_prev + elem->period;
		if (nexttime<mintime) mintime = nexttime;
	}
	*dirty = false;
	return mintime;
}
uint64_t TimerPoolRun(timer_pool **pp, bool *dirty, uint64_t bt)
{
	*dirty = false;
	if (!*pp) return 0; // no timers

	timer_pool *elem, *tmp, *p;
	char *name;
	const char *del;
	unsigned int n;
	uint64_t nexttime, mintime=0x7FFFFFFFFFFFFFFF;

	if (!bt) bt = boottime_ms();
	HASH_ITER(hh, *pp, elem, tmp)
	{
		nexttime = elem->bt_prev + elem->period;
		if (bt >= nexttime)
		{
			if (!(name = strdup(elem->str)))
				return 0;
			n = elem->n;

			del = NULL;
			if (!TimerPoolRunTimer(elem))
				del = "timer: '%s' deleted because of error\n";

			// timer function could delete the timer or recreate with the same name
			p = TimerPoolSearch(*pp, name);
			if (p==elem && p->n==n)
			{
				if (!del && elem->oneshot)
					del = "timer: '%s' deleted because of oneshot\n";
				// elem is valid, not deleted and not recreated
				if (del)
				{
					DLOG(del,name);
					TimerPoolDel(pp, elem);
					elem = NULL;
				}
				else
				{
					elem->bt_prev = bt;
					nexttime = elem->bt_prev + elem->period;
				}
			}

			free(name);
		}
		if (elem)
		{
			if (nexttime<mintime) mintime = nexttime;
		}
	}
	if (!*pp) return 0; // no timers
	if (*dirty)
		// something changed in the timer pool (deleted or added in the lua timer code)
		// rescan all timers for mintime
		mintime = TimerPoolNext(*pp, dirty);
	return mintime;
}
