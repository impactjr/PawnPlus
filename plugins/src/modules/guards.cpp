#include "guards.h"
#include "context.h"

#include "utils/set_pool.h"

struct guards_extra : public amx::extra
{
	aux::set_pool<dyn_object> pool;

	guards_extra(AMX *amx) : amx::extra(amx)
	{

	}

	virtual ~guards_extra()
	{
		for(auto &guard : pool)
		{
			guard->free();
		}
	}
};

size_t guards::count(AMX *amx)
{
	amx::object owner;
	auto &ctx = amx::get_context(amx, owner);
	auto &guards = ctx.get_extra<guards_extra>();
	return guards.pool.size();
}

dyn_object *guards::add(AMX *amx, dyn_object &&obj)
{
	amx::object owner;
	auto &ctx = amx::get_context(amx, owner);
	auto &guards = ctx.get_extra<guards_extra>();
	return guards.pool.add(std::move(obj));
}

bool guards::contains(AMX *amx, const dyn_object *obj)
{
	amx::object owner;
	auto &ctx = amx::get_context(amx, owner);
	auto &guards = ctx.get_extra<guards_extra>();
	return guards.pool.contains(obj);
}

bool guards::free(AMX *amx, dyn_object *obj)
{
	amx::object owner;
	auto &ctx = amx::get_context(amx, owner);
	auto &guards = ctx.get_extra<guards_extra>();

	auto it = guards.pool.find(obj);
	if(it != guards.pool.end())
	{
		obj->free();
		guards.pool.erase(it);
	}
	return false;
}