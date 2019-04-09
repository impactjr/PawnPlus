#include "capi.h"
#include "main.h"
#include "tags.h"
#include "variants.h"
#include "containers.h"
#include "strings.h"
#include "tasks.h"

#include <type_traits>
#include <unordered_map>
#include <string>

struct func_ptr
{
	void *ptr;

	func_ptr(std::nullptr_t) : ptr(nullptr)
	{

	}

	template <class Ret, class... Args>
	func_ptr(Ret(*func)(Args...)) : ptr(reinterpret_cast<void*>(func))
	{

	}
};

static std::unordered_map<std::string, void*> addons;

static func_ptr main_functions[] = {
	+[]/*version*/() -> cell
	{
		return PP_VERSION_NUMBER;
	},
	+[]/*collect*/() -> void
	{
		gc_collect();
	},
	+[]/*register_on_collect*/(void(*func)()) -> void*
	{
		return gc_register(func);
	},
	+[]/*unregister_on_collect*/(void *id) -> void
	{
		gc_unregister(id);
	},
	+[]/*add_addon*/(const char *name, void *ptr) -> cell
	{
		return addons.emplace(name, ptr).second;
	},
	+[]/*remove_addon*/(const char *name) -> cell
	{
		return addons.erase(name);
	},
	+[]/*get_addon*/(const char *name) -> void*
	{
		auto it = addons.find(name);
		if(it == addons.end())
		{
			return nullptr;
		}
		return it->second;
	},
	nullptr
};

static func_ptr tag_functions[] = {
	+[]/*find_tag_name*/(const char *name) -> const void*
	{
		return tags::find_tag(name);
	},
	+[]/*find_tag_uid*/(cell tag_uid) -> const void*
	{
		return tags::find_tag(tag_uid);
	},
	+[]/*get_tag_uid*/(const void *tag) -> cell
	{
		return static_cast<tag_ptr>(tag)->uid;
	},
	+[]/*tag_set_op*/(const void *tag, cell optype, cell(*handler)(void *cookie, const void *tag, cell *args, cell numargs), void *cookie) -> cell
	{
		auto ctl = static_cast<tag_ptr>(tag)->get_control();
		return ctl->set_op(static_cast<op_type>(optype), handler, cookie);
	},
	nullptr
};

static func_ptr dyn_object_functions[] = {
	+[]/*new_dyn_object_empty*/() -> void*
	{
		return new dyn_object();
	},
	+[]/*new_dyn_object_copy*/(const void *obj) -> void*
	{
		return new dyn_object(*static_cast<const dyn_object*>(obj));
	},
	+[]/*new_dyn_object_move*/(void *obj) -> void*
	{
		return new dyn_object(std::move(*static_cast<dyn_object*>(obj)));
	},
	+[]/*new_dyn_object_cell*/(cell value, const void *tag) -> void*
	{
		return new dyn_object(value, static_cast<tag_ptr>(tag));
	},
	+[]/*new_dyn_object_arr*/(const cell *arr, cell size, const void *tag) -> void*
	{
		return new dyn_object(arr, size, static_cast<tag_ptr>(tag));
	},
	+[]/*new_dyn_object_str*/(const cell *str) -> void*
	{
		return new dyn_object(str);
	},
	+[]/*delete_dyn_object*/(void *obj) -> void
	{
		delete static_cast<dyn_object*>(obj);
	},
	+[]/*assign_dyn_object_copy*/(void *self, const void *obj) -> void
	{
		*static_cast<dyn_object*>(self) = *static_cast<const dyn_object*>(obj);
	},
	+[]/*assign_dyn_object_move*/(void *self, void *obj) -> void
	{
		*static_cast<dyn_object*>(self) = std::move(*static_cast<dyn_object*>(obj));
	},
	+[]/*dyn_object_get_tag*/(const void *obj) -> const void*
	{
		return static_cast<const dyn_object*>(obj)->get_tag();
	},
	+[]/*dyn_object_get_size*/(const void *obj) -> cell
	{
		return static_cast<const dyn_object*>(obj)->get_size();
	},
	+[]/*dyn_object_is_array*/(const void *obj) -> cell
	{
		return static_cast<const dyn_object*>(obj)->is_array();
	},
	+[]/*dyn_object_get_cell*/(const void *obj, cell index) -> cell
	{
		cell val;
		if(static_cast<const dyn_object*>(obj)->get_cell(index, val))
		{
			return val;
		}
		return 0;
	},
	+[]/*dyn_object_get_array*/(const void *obj, cell *arr, cell maxsize) -> cell
	{
		return static_cast<const dyn_object*>(obj)->get_array(arr, maxsize);
	},
	+[]/*dyn_object_acquire*/(void *obj) -> void
	{
		static_cast<const dyn_object*>(obj)->acquire();
	},
	+[]/*dyn_object_release*/(void *obj) -> void
	{
		static_cast<const dyn_object*>(obj)->release();
	},
	+[]/*dyn_object_get_rank*/(const void *obj) -> cell
	{
		return static_cast<const dyn_object*>(obj)->get_rank();
	},
	nullptr
};

static func_ptr list_functions[] = {
	+[]/*new_list*/() -> void*
	{
		return &*list_pool.add();
	},
	+[]/*delete_list*/(void *list) -> void
	{
		list_pool.remove(static_cast<list_t*>(list));
	},
	+[]/*list_get_id*/(void *list) -> cell
	{
		return list_pool.get_id(static_cast<list_t*>(list));
	},
	+[]/*list_from_id*/(cell id) -> void*
	{
		list_t *ptr;
		if(list_pool.get_by_id(id, ptr))
		{
			return ptr;
		}
		return nullptr;
	},
	+[]/*list_get_size*/(const void *list) -> cell
	{
		return static_cast<const list_t*>(list)->size();
	},
	+[]/*list_get*/(void *list, cell index) -> void*
	{
		return &((*static_cast<list_t*>(list))[index]);
	},
	+[]/*list_add_copy*/(void *list, const void *obj) -> void
	{
		static_cast<list_t*>(list)->push_back(*static_cast<const dyn_object*>(obj));
	},
	+[]/*list_add_move*/(void *list, void *obj) -> void
	{
		static_cast<list_t*>(list)->push_back(std::move(*static_cast<dyn_object*>(obj)));
	},
	+[]/*list_clear*/(void *list) -> void
	{
		static_cast<list_t*>(list)->clear();
	},
	nullptr
};

static func_ptr linked_list_functions[] = {
	+[]/*new_linked_list*/() -> void*
	{
		return &*linked_list_pool.add();
	},
	+[]/*delete_linked_list*/(void *linked_list) -> void
	{
		linked_list_pool.remove(static_cast<linked_list_t*>(linked_list));
	},
	+[]/*linked_list_get_id*/(void *linked_list) -> cell
	{
		return linked_list_pool.get_id(static_cast<linked_list_t*>(linked_list));
	},
	+[]/*linked_list_from_id*/(cell id) -> void*
	{
		linked_list_t *ptr;
		if(linked_list_pool.get_by_id(id, ptr))
		{
			return ptr;
		}
		return nullptr;
	},
	+[]/*linked_list_get_size*/(const void *linked_list) -> cell
	{
		return static_cast<const linked_list_t*>(linked_list)->size();
	},
	+[]/*linked_list_get*/(void *linked_list, cell index) -> void*
	{
		return &((*static_cast<linked_list_t*>(linked_list))[index]);
	},
	+[]/*linked_list_add_copy*/(void *linked_list, const void *obj) -> void
	{
		static_cast<linked_list_t*>(linked_list)->push_back(*static_cast<const dyn_object*>(obj));
	},
	+[]/*linked_list_add_move*/(void *linked_list, void *obj) -> void
	{
		static_cast<linked_list_t*>(linked_list)->push_back(std::move(*static_cast<dyn_object*>(obj)));
	},
	+[]/*linked_list_clear*/(void *linked_list) -> void
	{
		static_cast<linked_list_t*>(linked_list)->clear();
	},
	nullptr
};

static func_ptr map_functions[] = {
	+[]/*new_map*/() -> void*
	{
		return &*map_pool.add();
	},
	+[]/*delete_map*/(void *map) -> void
	{
		map_pool.remove(static_cast<map_t*>(map));
	},
	+[]/*map_get_id*/(void *map) -> cell
	{
		return map_pool.get_id(static_cast<map_t*>(map));
	},
	+[]/*map_from_id*/(cell id) -> void*
	{
		map_t *ptr;
		if(map_pool.get_by_id(id, ptr))
		{
			return ptr;
		}
		return nullptr;
	},
	+[]/*map_get_size*/(const void *map) -> cell
	{
		return static_cast<const map_t*>(map)->size();
	},
	+[]/*map_get*/(void *map, const void *key) -> void*
	{
		return &((*static_cast<map_t*>(map))[*static_cast<const dyn_object*>(key)]);
	},
	+[]/*map_find*/(void *map, const void *key) -> void*
	{
		map_t &mapobj = *static_cast<map_t*>(map);
		auto it = mapobj.find(*static_cast<const dyn_object*>(key));
		if(it != mapobj.end())
		{
			return &it->second;
		}
		return nullptr;
	},
	+[]/*map_add_copy*/(void *map, const void *key, const void *value) -> cell
	{
		return static_cast<map_t*>(map)->insert(*static_cast<const dyn_object*>(key), *static_cast<const dyn_object*>(value)).second;
	},
	+[]/*map_add_move*/(void *map, const void *key, void *value) -> cell
	{
		return static_cast<map_t*>(map)->insert(*static_cast<const dyn_object*>(key), std::move(*static_cast<dyn_object*>(value))).second;
	},
	+[]/*map_clear*/(void *map) -> void
	{
		static_cast<map_t*>(map)->clear();
	},
	+[]/*map_is_ordered*/(void *map) -> cell
	{
		return static_cast<map_t*>(map)->ordered();
	},
	+[]/*map_set_ordered*/(void *map, cell ordered) -> void
	{
		static_cast<map_t*>(map)->set_ordered(ordered);
	},
	nullptr
};

using string_ptr = typename std::remove_reference<typename object_pool<strings::cell_string>::object_ptr>::type*;
using const_string_ptr = typename std::remove_reference<typename object_pool<strings::cell_string>::const_object_ptr>::type*;

static func_ptr string_functions[] = {
	+[]/*new_string*/() -> void*
	{
		return &strings::pool.add();
	},
	+[]/*delete_string*/(void *str) -> void
	{
		strings::pool.remove(*static_cast<string_ptr>(str));
	},
	+[]/*string_get_id*/(void *str) -> cell
	{
		return strings::pool.get_id(*static_cast<string_ptr>(str));
	},
	+[]/*string_from_id*/(cell id) -> void*
	{
		string_ptr ptr;
		if(strings::pool.get_by_id(id, ptr))
		{
			return ptr;
		}
		return nullptr;
	},
	+[]/*string_acquire*/(void *str) -> cell
	{
		return static_cast<string_ptr>(str)->acquire();
	},
	+[]/*string_release*/(void *str) -> cell
	{
		return static_cast<string_ptr>(str)->release();
	},
	+[]/*string_get_size*/(const void *str) -> cell
	{
		return (*static_cast<const_string_ptr>(str))->size();
	},
	+[]/*string_get_data*/(void *str) -> cell*
	{
		return &(**static_cast<string_ptr>(str))[0];
	},
	+[]/*string_create*/(const char *c, cell len) -> cell
	{
		std::string str(c, len);
		return strings::create(str);
	},
	+[]/*string_append*/(void *str, const cell *data, cell len) -> void
	{
		(*static_cast<string_ptr>(str))->append(data, len);
	},
	nullptr
};

using variant_ptr = typename std::remove_reference<typename object_pool<dyn_object>::object_ptr>::type*;
using const_variant_ptr = typename std::remove_reference<typename object_pool<dyn_object>::const_object_ptr>::type*;

static func_ptr variant_functions[] = {
	+[]/*new_variant*/() -> void*
	{
		return &variants::pool.add();
	},
	+[]/*delete_variant*/(void *var) -> void
	{
		variants::pool.remove(*static_cast<variant_ptr>(var));
	},
	+[]/*variant_get_id*/(void *var) -> cell
	{
		return variants::pool.get_id(*static_cast<variant_ptr>(var));
	},
	+[]/*variant_from_id*/(cell id) -> void*
	{
		variant_ptr ptr;
		if(variants::pool.get_by_id(id, ptr))
		{
			return ptr;
		}
		return nullptr;
	},
	+[]/*variant_acquire*/(void *var) -> cell
	{
		return static_cast<variant_ptr>(var)->acquire();
	},
	+[]/*variant_release*/(void *var) -> cell
	{
		return static_cast<variant_ptr>(var)->release();
	},
	+[]/*variant_get_size*/(const void *var) -> cell
	{
		return (*static_cast<const_variant_ptr>(var))->get_size();
	},
	+[]/*variant_get_object*/(void *var) -> void*
	{
		return &*static_cast<variant_ptr>(var);
	},
	nullptr
};

static func_ptr task_functions[] = {
	+[]/*new_task*/() -> void*
	{
		return &*tasks::add();
	},
	+[]/*delete_task*/(void *task) -> void
	{
		tasks::remove(static_cast<tasks::task*>(task));
	},
	+[]/*task_get_id*/(void *task) -> cell
	{
		return tasks::get_id(static_cast<tasks::task*>(task));
	},
	+[]/*task_from_id*/(cell id) -> void*
	{
		tasks::task *ptr;
		if(tasks::get_by_id(id, ptr))
		{
			return ptr;
		}
		return nullptr;
	},
	+[]/*task_state*/(const void *task) -> cell
	{
		return static_cast<const tasks::task*>(task)->state();
	},
	+[]/*task_completed*/(const void *task) -> cell
	{
		return static_cast<const tasks::task*>(task)->completed();
	},
	+[]/*task_faulted*/(const void *task) -> cell
	{
		return static_cast<const tasks::task*>(task)->faulted();
	},
	+[]/*task_result*/(const void *task) -> void*
	{
		dyn_object result = static_cast<const tasks::task*>(task)->result();
		if(result.is_null()) return nullptr;
		return new dyn_object(std::move(result));
	},
	+[]/*task_error*/(const void *task) -> cell
	{
		return static_cast<const tasks::task*>(task)->error();
	},
	+[]/*task_reset*/(void *task) -> void
	{
		static_cast<tasks::task*>(task)->reset();
	},
	+[]/*task_keep*/(void *task, cell keep) -> void
	{
		static_cast<tasks::task*>(task)->keep(keep);
	},
	+[]/*task_set_completed*/(void *task, void *result) -> cell
	{
		return static_cast<tasks::task*>(task)->set_completed(std::move(*static_cast<dyn_object*>(result)));
	},
	+[]/*task_set_faulted*/(void *task, cell error) -> cell
	{
		return static_cast<tasks::task*>(task)->set_faulted(error);
	},
	+[]/*task_register_handler*/(void *task, cell (*handler)(void *task, void *cookie), void *cookie) -> void
	{
		static_cast<tasks::task*>(task)->register_handler([=](tasks::task &task)
		{
			return handler(&task, cookie);
		});
	},
	+[]/*task_register_handler_iter*/(void *task, cell(*handler)(void *task, void *cookie), void *cookie) -> void*
	{
		auto it = static_cast<tasks::task*>(task)->register_handler([=](tasks::task &task)
		{
			return handler(&task, cookie);
		});
		return new typename std::list<std::unique_ptr<tasks::handler>>::iterator(std::move(it));
	},
	+[]/*task_free_iter*/(void *iter) -> void
	{
		delete static_cast<typename std::list<std::unique_ptr<tasks::handler>>::iterator*>(iter);
	},
	+[]/*task_unregister_handler*/(void *task, void *iter) -> void
	{
		auto it = static_cast<typename std::list<std::unique_ptr<tasks::handler>>::iterator*>(iter);
		static_cast<tasks::task*>(task)->unregister_handler(*it);
		delete it;
	},
	nullptr
};

static void *func_table[] = {
	&main_functions,
	&tag_functions,
	&dyn_object_functions,
	&list_functions,
	&linked_list_functions,
	&map_functions,
	&string_functions,
	&variant_functions,
	&task_functions,
	nullptr
};

void ***get_api_table()
{
	return reinterpret_cast<void***>(&func_table);
}
