#include "natives.h"
#include "modules/containers.h"
#include "modules/variants.h"
#include <iterator>

template <size_t... KeyIndices>
class key_at
{
	using key_ftype = typename dyn_factory<KeyIndices...>::type;

public:
	template <size_t... ValueIndices>
	class value_at
	{
		using value_ftype = typename dyn_factory<ValueIndices...>::type;
		using result_ftype = typename dyn_result<ValueIndices...>::type;

	public:
		// native bool:map_add(Map:map, key, value, ...);
		template <key_ftype KeyFactory, value_ftype ValueFactory>
		static cell AMX_NATIVE_CALL map_add(AMX *amx, cell *params)
		{
			map_t *ptr;
			if(!map_pool.get_by_id(params[1], ptr)) return -1;
			auto ret = ptr->insert(std::make_pair(KeyFactory(amx, params[KeyIndices]...), ValueFactory(amx, params[ValueIndices]...)));
			return static_cast<cell>(ret.second);
		}

		// native bool:map_set(Map:map, key, value, ...);
		template <key_ftype KeyFactory, value_ftype ValueFactory>
		static cell AMX_NATIVE_CALL map_set(AMX *amx, cell *params)
		{
			map_t *ptr;
			if(!map_pool.get_by_id(params[1], ptr)) return 0;
			(*ptr)[KeyFactory(amx, params[KeyIndices]...)] = ValueFactory(amx, params[ValueIndices]...);
			return 1;
		}

		// native map_get(Map:map, key, ...);
		template <key_ftype KeyFactory, result_ftype ValueFactory>
		static cell AMX_NATIVE_CALL map_get(AMX *amx, cell *params)
		{
			map_t *ptr;
			if(!map_pool.get_by_id(params[1], ptr)) return 0;
			auto it = ptr->find(KeyFactory(amx, params[KeyIndices]...));
			if(it != ptr->end())
			{
				return ValueFactory(amx, it->second, params[ValueIndices]...);
			}
			return 0;
		}
	};

	// native bool:map_remove(Map:map, key, ...);
	template <key_ftype KeyFactory>
	static cell AMX_NATIVE_CALL map_remove(AMX *amx, cell *params)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto it = ptr->find(KeyFactory(amx, params[KeyIndices]...));
		if(it != ptr->end())
		{
			ptr->erase(it);
			return 1;
		}
		return 0;
	}

	// native bool:map_has_key(Map:map, key, ...);
	template <key_ftype KeyFactory>
	static cell AMX_NATIVE_CALL map_has_key(AMX *amx, cell *params)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto it = ptr->find(KeyFactory(amx, params[KeyIndices]...));
		if(it != ptr->end())
		{
			return 1;
		}
		return 0;
	}
	
	// native bool:map_set_cell(Map:map, key, offset, AnyTag:value, ...);
	template <key_ftype KeyFactory, size_t TagIndex = 0>
	static cell AMX_NATIVE_CALL map_set_cell(AMX *amx, cell *params)
	{
		if(params[3] < 0) return 0;
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto it = ptr->find(KeyFactory(amx, params[KeyIndices]...));
		if(it != ptr->end())
		{
			auto &obj = it->second;
			if(TagIndex && !obj.tag_assignable(amx, params[TagIndex])) return 0;
			return obj.set_cell(params[3], params[4]);
		}
		return 0;
	}

	// native map_tagof(Map:map, key, ...);
	template <key_ftype KeyFactory>
	static cell AMX_NATIVE_CALL map_tagof(AMX *amx, cell *params)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto it = ptr->find(KeyFactory(amx, params[KeyIndices]...));
		if(it != ptr->end())
		{
			return it->second.get_tag(amx);
		}
		return 0;
	}

	// native map_sizeof(Map:map, key, ...);
	template <key_ftype KeyFactory>
	static cell AMX_NATIVE_CALL map_sizeof(AMX *amx, cell *params)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto it = ptr->find(KeyFactory(amx, params[KeyIndices]...));
		if(it != ptr->end())
		{
			return it->second.get_size();
		}
		return 0;
	}
};

template <size_t... ValueIndices>
class value_at
{
	using result_ftype = typename dyn_result<ValueIndices...>::type;

public:
	// native map_key_at(Map:map, index, ...);
	template <result_ftype ValueFactory>
	static cell AMX_NATIVE_CALL map_key_at(AMX *amx, cell *params)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto it = ptr->begin();
		std::advance(it, params[2]);
		if(it != ptr->end())
		{
			return ValueFactory(amx, it->first, params[ValueIndices]...);
		}
		return 0;
	}

	// native map_value_at(Map:map, index, ...);
	template <result_ftype ValueFactory>
	static cell AMX_NATIVE_CALL map_value_at(AMX *amx, cell *params)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto it = ptr->begin();
		std::advance(it, params[2]);
		if(it != ptr->end())
		{
			return ValueFactory(amx, it->second, params[ValueIndices]...);
		}
		return 0;
	}
};

namespace Natives
{
	// native Map:map_new();
	AMX_DEFINE_NATIVE(map_new, 0)
	{
		return map_pool.get_id(map_pool.add());
	}

	// native Map:map_new_args(key_tag_id=tagof(arg0), value_tag_id=tagof(arg1), AnyTag:arg0, AnyTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_new_args, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell) - 2;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_args: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[3 + arg], value = params[3 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, *key_addr, params[1]), dyn_object(amx, *value_addr, params[2])));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_args_str(key_tag_id=tagof(arg0), AnyTag:arg0, arg1[], AnyTag:...);
	AMX_DEFINE_NATIVE(map_new_args_str, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
			}else{
				amx_GetAddr(amx, key, &key_addr);
			}
			amx_GetAddr(amx, value, &value_addr);
			ptr->insert(std::make_pair(dyn_object(amx, *key_addr, params[1]), dyn_object(amx, value_addr)));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_args_var(key_tag_id=tagof(arg0), AnyTag:arg0, VariantTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_new_args_var, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, *key_addr, params[1]), variants::get(*value_addr)));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_str_args(value_tag_id=tagof(arg1), arg0[], AnyTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_new_str_args, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_str_args: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			amx_GetAddr(amx, key, &key_addr);
			if(arg == 0)
			{
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, key_addr), dyn_object(amx, *value_addr, params[1])));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_str_args_str(arg0[], arg1[], ...);
	AMX_DEFINE_NATIVE(map_new_str_args_str, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell);
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_str_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[1 + arg], value = params[1 + arg + 1], *key_addr, *value_addr;
			amx_GetAddr(amx, key, &key_addr);
			amx_GetAddr(amx, value, &value_addr);
			ptr->insert(std::make_pair(dyn_object(amx, key_addr), dyn_object(amx, value_addr)));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_str_args_var(arg0[], VariantTag:arg1, {_,VariantTags}:...);
	AMX_DEFINE_NATIVE(map_new_str_args_var, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell);
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_str_args_var: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[1 + arg], value = params[1 + arg + 1], *key_addr, *value_addr;
			amx_GetAddr(amx, key, &key_addr);
			if(arg == 0)
			{
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, key_addr), variants::get(*value_addr)));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_var_args(value_tag_id=tagof(arg1), VariantTag:arg0, AnyTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_new_var_args, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_var_args: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(variants::get(*key_addr), dyn_object(amx, *value_addr, params[1])));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_var_args_str(VariantTag:arg0, arg1[], {_,VariantTags}:...);
	AMX_DEFINE_NATIVE(map_new_var_args_str, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell);
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_var_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[1 + arg], value = params[1 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
			}else{
				amx_GetAddr(amx, key, &key_addr);
			}
			amx_GetAddr(amx, value, &value_addr);
			ptr->insert(std::make_pair(variants::get(*key_addr), dyn_object(amx, value_addr)));
		}
		return map_pool.get_id(ptr);
	}

	// native Map:map_new_var_args_var(VariantTag:arg0, VariantTag:arg1, VariantTag:...);
	AMX_DEFINE_NATIVE(map_new_var_args_var, 0)
	{
		auto ptr = map_pool.add();
		cell numargs = params[0] / sizeof(cell);
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_new_var_args_var: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[1 + arg], value = params[1 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(variants::get(*key_addr), variants::get(*value_addr)));
		}
		return map_pool.get_id(ptr);
	}

	// native bool:map_valid(Map:map);
	AMX_DEFINE_NATIVE(map_valid, 1)
	{
		map_t *ptr;
		return map_pool.get_by_id(params[1], ptr);
	}

	// native bool:map_delete(Map:map);
	AMX_DEFINE_NATIVE(map_delete, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		return map_pool.remove(ptr);
	}

	// native bool:map_delete_deep(Map:map);
	AMX_DEFINE_NATIVE(map_delete_deep, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		for(auto &pair : *ptr)
		{
			pair.first.release();
			pair.second.release();
		}
		return map_pool.remove(ptr);
	}

	// native Map:map_clone(Map:map);
	AMX_DEFINE_NATIVE(map_clone, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		auto m = map_pool.add();
		for(auto &&pair : *ptr)
		{
			m->insert(std::make_pair(pair.first.clone(), pair.second.clone()));
		}
		return map_pool.get_id(m);
	}

	// native map_size(Map:map);
	AMX_DEFINE_NATIVE(map_size, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return -1;
		return static_cast<cell>(ptr->size());
	}

	// native map_clear(Map:map);
	AMX_DEFINE_NATIVE(map_clear, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		ptr->clear();
		return 1;
	}

	// native map_set_ordered(Map:map, bool:ordered);
	AMX_DEFINE_NATIVE(map_set_ordered, 2)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		ptr->set_ordered(params[2]);
		return 1;
	}

	// native bool:map_is_ordered(Map:map);
	AMX_DEFINE_NATIVE(map_is_ordered, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return 0;
		return ptr->ordered();
	}

	// native bool:map_add(Map:map, AnyTag:key, AnyTag:value, key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_add, 5)
	{
		return key_at<2, 4>::value_at<3, 5>::map_add<dyn_func, dyn_func>(amx, params);
	}

	// native bool:map_add_arr(Map:map, AnyTag:key, const AnyTag:value[], value_size=sizeof(value), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_add_arr, 6)
	{
		return key_at<2, 5>::value_at<3, 4, 6>::map_add<dyn_func, dyn_func_arr>(amx, params);
	}

	// native bool:map_add_str(Map:map, AnyTag:key, const value[], key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_add_str, 4)
	{
		return key_at<2, 4>::value_at<3>::map_add<dyn_func, dyn_func_str>(amx, params);
	}

	// native bool:map_add_var(Map:map, AnyTag:key, VariantTag:value, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_add_var, 4)
	{
		return key_at<2, 4>::value_at<3>::map_add<dyn_func, dyn_func_var>(amx, params);
	}

	// native bool:map_arr_add(Map:map, const AnyTag:key[], AnyTag:value, key_size=sizeof(key), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_add, 6)
	{
		return key_at<2, 4, 5>::value_at<3, 6>::map_add<dyn_func_arr, dyn_func>(amx, params);
	}

	// native bool:map_arr_add_arr(Map:map, const AnyTag:key[], const AnyTag:value[], key_size=sizeof(key), value_size=sizeof(value), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_add_arr, 7)
	{
		return key_at<2, 4, 6>::value_at<3, 5, 7>::map_add<dyn_func_arr, dyn_func_arr>(amx, params);
	}

	// native bool:map_arr_add_str(Map:map, const AnyTag:key[], const value[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_add_str, 5)
	{
		return key_at<2, 4, 5>::value_at<3>::map_add<dyn_func_arr, dyn_func_str>(amx, params);
	}

	// native bool:map_arr_add_var(Map:map, const AnyTag:key[], VariantTag:value, key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_add_var, 5)
	{
		return key_at<2, 4, 5>::value_at<3>::map_add<dyn_func_arr, dyn_func_var>(amx, params);
	}

	// native bool:map_str_add(Map:map, const key[], AnyTag:value, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_str_add, 4)
	{
		return key_at<2>::value_at<3, 4>::map_add<dyn_func_str, dyn_func>(amx, params);
	}

	// native bool:map_str_add_arr(Map:map, const key[], const AnyTag:value[], value_size=sizeof(value), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_str_add_arr, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_add<dyn_func_str, dyn_func_arr>(amx, params);
	}

	// native bool:map_str_add_str(Map:map, const key[], const value[]);
	AMX_DEFINE_NATIVE(map_str_add_str, 3)
	{
		return key_at<2>::value_at<3>::map_add<dyn_func_str, dyn_func_str>(amx, params);
	}

	// native bool:map_str_add_var(Map:map, const key[], VariantTag:value);
	AMX_DEFINE_NATIVE(map_str_add_var, 3)
	{
		return key_at<2>::value_at<3>::map_add<dyn_func_str, dyn_func_var>(amx, params);
	}

	// native bool:map_var_add(Map:map, VariantTag:key, AnyTag:value, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_var_add, 4)
	{
		return key_at<2>::value_at<3, 4>::map_add<dyn_func_var, dyn_func>(amx, params);
	}

	// native bool:map_var_add_arr(Map:map, VariantTag:key, const AnyTag:value[], value_size=sizeof(value), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_var_add_arr, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_add<dyn_func_var, dyn_func_arr>(amx, params);
	}

	// native bool:map_var_add_str(Map:map, VariantTag:key, const value[]);
	AMX_DEFINE_NATIVE(map_var_add_str, 3)
	{
		return key_at<2>::value_at<3>::map_add<dyn_func_var, dyn_func_str>(amx, params);
	}

	// native bool:map_str_add_var(Map:map, VariantTag:key, VariantTag:value);
	AMX_DEFINE_NATIVE(map_var_add_var, 3)
	{
		return key_at<2>::value_at<3>::map_add<dyn_func_var, dyn_func_var>(amx, params);
	}

	// native map_add_map(Map:map, Map:other, bool:overwrite);
	AMX_DEFINE_NATIVE(map_add_map, 3)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return -1;
		map_t *ptr2;
		if(!map_pool.get_by_id(params[1], ptr2)) return -1;
		if(params[3])
		{
			for(auto &pair : *ptr2)
			{
				(*ptr)[pair.first] = pair.second;
			}
		}else{
			ptr->insert(ptr2->begin(), ptr2->end());
		}
		return ptr->size() - ptr2->size();
	}

	// native map_add_args(key_tag_id=tagof(arg0), value_tag_id=tagof(arg1), Map:map, AnyTag:arg0, AnyTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_add_args, 3)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[3], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 3;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_args: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[4 + arg], value = params[4 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, *key_addr, params[1]), dyn_object(amx, *value_addr, params[2])));
		}
		return numargs / 2;
	}

	// native map_add_args_str(key_tag_id=tagof(arg0), Map:map, AnyTag:arg0, arg1[], AnyTag:...);
	AMX_DEFINE_NATIVE(map_add_args_str, 2)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[2], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 2;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[3 + arg], value = params[3 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
			}else{
				amx_GetAddr(amx, key, &key_addr);
			}
			amx_GetAddr(amx, value, &value_addr);
			ptr->insert(std::make_pair(dyn_object(amx, *key_addr, params[1]), dyn_object(amx, value_addr)));
		}
		return numargs / 2;
	}

	// native map_add_args_var(key_tag_id=tagof(arg0), Map:map, AnyTag:arg0, VariantTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_add_args_var, 2)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[2], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 2;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[3 + arg], value = params[3 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, *key_addr, params[1]), variants::get(*value_addr)));
		}
		return numargs / 2;
	}

	// native map_add_str_args(value_tag_id=tagof(arg1), Map:map, arg0[], AnyTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_add_str_args, 2)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[2], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 2;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_str_args: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[3 + arg], value = params[3 + arg + 1], *key_addr, *value_addr;
			amx_GetAddr(amx, key, &key_addr);
			if(arg == 0)
			{
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, key_addr), dyn_object(amx, *value_addr, params[1])));
		}
		return numargs / 2;
	}

	// native map_add_str_args_str(Map:map, arg0[], arg1[], ...);
	AMX_DEFINE_NATIVE(map_add_str_args_str, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_str_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			amx_GetAddr(amx, key, &key_addr);
			amx_GetAddr(amx, value, &value_addr);
			ptr->insert(std::make_pair(dyn_object(amx, key_addr), dyn_object(amx, value_addr)));
		}
		return numargs / 2;
	}

	// native map_add_str_args_var(Map:map, arg0[], VariantTag:arg1, {_,VariantTags}:...);
	AMX_DEFINE_NATIVE(map_add_str_args_var, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_str_args_var: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			amx_GetAddr(amx, key, &key_addr);
			if(arg == 0)
			{
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(dyn_object(amx, key_addr), variants::get(*value_addr)));
		}
		return numargs / 2;
	}

	// native map_add_var_args(value_tag_id=tagof(arg1), Map:map, VariantTag:arg0, AnyTag:arg1, AnyTag:...);
	AMX_DEFINE_NATIVE(map_add_var_args, 2)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[2], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 2;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_var_args: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[3 + arg], value = params[3 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(variants::get(*key_addr), dyn_object(amx, *value_addr, params[1])));
		}
		return numargs / 2;
	}

	// native map_add_var_args_str(Map:map, VariantTag:arg0, arg1[], {_,VariantTags}:...);
	AMX_DEFINE_NATIVE(map_add_var_args_str, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_var_args_str: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
			}else{
				amx_GetAddr(amx, key, &key_addr);
			}
			amx_GetAddr(amx, value, &value_addr);
			ptr->insert(std::make_pair(variants::get(*key_addr), dyn_object(amx, value_addr)));
		}
		return numargs / 2;
	}

	// native Map:map_add_var_args_var(Map:map, VariantTag:arg0, VariantTag:arg1, VariantTag:...);
	AMX_DEFINE_NATIVE(map_add_var_args_var, 1)
	{
		map_t *ptr;
		if(!map_pool.get_by_id(params[1], ptr)) return -1;
		cell numargs = params[0] / sizeof(cell) - 1;
		if(numargs % 2 != 0)
		{
			logerror(amx, "[PP] map_add_var_args_var: bad variable argument count %d (must be even).", numargs);
			return 0;
		}
		for(cell arg = 0; arg < numargs; arg += 2)
		{
			cell key = params[2 + arg], value = params[2 + arg + 1], *key_addr, *value_addr;
			if(arg == 0)
			{
				key_addr = &key;
				value_addr = &value;
			}else{
				amx_GetAddr(amx, key, &key_addr);
				amx_GetAddr(amx, key, &value_addr);
			}
			ptr->insert(std::make_pair(variants::get(*key_addr), variants::get(*value_addr)));
		}
		return numargs / 2;
	}

	// native bool:map_remove(Map:map, AnyTag:key, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_remove, 3)
	{
		return key_at<2, 3>::map_remove<dyn_func>(amx, params);
	}

	// native bool:map_arr_remove(Map:map, const AnyTag:key[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_remove, 4)
	{
		return key_at<2, 3, 4>::map_remove<dyn_func_arr>(amx, params);
	}

	// native bool:map_str_remove(Map:map, const key[]);
	AMX_DEFINE_NATIVE(map_str_remove, 2)
	{
		return key_at<2>::map_remove<dyn_func_str>(amx, params);
	}

	// native bool:map_str_remove(Map:map, VariantTag:key);
	AMX_DEFINE_NATIVE(map_var_remove, 2)
	{
		return key_at<2>::map_remove<dyn_func_var>(amx, params);
	}

	// native bool:map_has_key(Map:map, AnyTag:key, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_has_key, 3)
	{
		return key_at<2, 3>::map_has_key<dyn_func>(amx, params);
	}

	// native bool:map_has_arr_key(Map:map, const AnyTag:key[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_has_arr_key, 4)
	{
		return key_at<2, 3, 4>::map_has_key<dyn_func_arr>(amx, params);
	}

	// native bool:map_has_str_key(Map:map, const key[]);
	AMX_DEFINE_NATIVE(map_has_str_key, 2)
	{
		return key_at<2>::map_has_key<dyn_func_str>(amx, params);
	}

	// native bool:map_has_var_key(Map:map, VariantTag:key);
	AMX_DEFINE_NATIVE(map_has_var_key, 2)
	{
		return key_at<2>::map_has_key<dyn_func_var>(amx, params);
	}

	// native map_get(Map:map, AnyTag:key, offset=0, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_get, 4)
	{
		return key_at<2, 4>::value_at<3>::map_get<dyn_func, dyn_func>(amx, params);
	}

	// native map_get_arr(Map:map, AnyTag:key, AnyTag:value[], value_size=sizeof(value), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_get_arr, 5)
	{
		return key_at<2, 5>::value_at<3, 4>::map_get<dyn_func, dyn_func_arr>(amx, params);
	}

	// native Variant:map_get_var(Map:map, AnyTag:key, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_get_var, 3)
	{
		return key_at<2, 3>::value_at<>::map_get<dyn_func, dyn_func_var>(amx, params);
	}

	// native bool:map_get_safe(Map:map, AnyTag:key, &AnyTag:value, offset=0, key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_get_safe, 6)
	{
		return key_at<2, 5>::value_at<3, 4, 6>::map_get<dyn_func, dyn_func>(amx, params);
	}

	// native map_get_arr_safe(Map:map, AnyTag:key, AnyTag:value[], value_size=sizeof(value), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_get_arr_safe, 6)
	{
		return key_at<2, 5>::value_at<3, 4, 6>::map_get<dyn_func, dyn_func_arr>(amx, params);
	}

	// native map_arr_get(Map:map, const AnyTag:key[], offset=0, key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_get, 5)
	{
		return key_at<2, 4, 5>::value_at<3>::map_get<dyn_func_arr, dyn_func>(amx, params);
	}

	// native map_arr_get_arr(Map:map, const AnyTag:key[], AnyTag:value[], value_size=sizeof(value), key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_get_arr, 6)
	{
		return key_at<2, 5, 6>::value_at<3, 4>::map_get<dyn_func_arr, dyn_func_arr>(amx, params);
	}

	// native Variant:map_arr_get_var(Map:map, const AnyTag:key[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_get_var, 4)
	{
		return key_at<2, 3, 4>::value_at<>::map_get<dyn_func_arr, dyn_func_var>(amx, params);
	}

	// native bool:map_arr_get_safe(Map:map, const AnyTag:key[], &AnyTag:value, offset=0, key_size=sizeof(key), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_get_safe, 7)
	{
		return key_at<2, 5, 6>::value_at<3, 4, 7>::map_get<dyn_func_arr, dyn_func>(amx, params);
	}

	// native map_arr_get_arr_safe(Map:map, const AnyTag:key[], AnyTag:value[], value_size=sizeof(value), key_size=sizeof(key), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_get_arr_safe, 7)
	{
		return key_at<2, 5, 6>::value_at<3, 4, 7>::map_get<dyn_func_arr, dyn_func_arr>(amx, params);
	}

	// native map_str_get(Map:map, const key[], offset=0);
	AMX_DEFINE_NATIVE(map_str_get, 3)
	{
		return key_at<2>::value_at<3>::map_get<dyn_func_str, dyn_func>(amx, params);
	}

	// native map_str_get_arr(Map:map, const key[], AnyTag:value[], value_size=sizeof(value));
	AMX_DEFINE_NATIVE(map_str_get_arr, 4)
	{
		return key_at<2>::value_at<3, 4>::map_get<dyn_func_str, dyn_func_arr>(amx, params);
	}

	// native Variant:map_str_get_var(Map:map, const key[]);
	AMX_DEFINE_NATIVE(map_str_get_var, 2)
	{
		return key_at<2>::value_at<>::map_get<dyn_func_str, dyn_func_var>(amx, params);
	}

	// native bool:map_str_get_safe(Map:map, const key[], &AnyTag:value, offset=0, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_str_get_safe, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_get<dyn_func_str, dyn_func>(amx, params);
	}

	// native map_str_get_arr_safe(Map:map, const key[], AnyTag:value[], value_size=sizeof(value), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_str_get_arr_safe, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_get<dyn_func_str, dyn_func_arr>(amx, params);
	}

	// native map_var_get(Map:map, VariantTag:key, offset=0);
	AMX_DEFINE_NATIVE(map_var_get, 3)
	{
		return key_at<2>::value_at<3>::map_get<dyn_func_var, dyn_func>(amx, params);
	}

	// native map_var_get_arr(Map:map, VariantTag:key, AnyTag:value[], value_size=sizeof(value));
	AMX_DEFINE_NATIVE(map_var_get_arr, 4)
	{
		return key_at<2>::value_at<3, 4>::map_get<dyn_func_var, dyn_func_arr>(amx, params);
	}

	// native Variant:map_var_get_var(Map:map, VariantTag:key);
	AMX_DEFINE_NATIVE(map_var_get_var, 2)
	{
		return key_at<2>::value_at<>::map_get<dyn_func_var, dyn_func_var>(amx, params);
	}

	// native bool:map_var_get_safe(Map:map, VariantTag:key, &AnyTag:value, offset=0, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_var_get_safe, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_get<dyn_func_var, dyn_func>(amx, params);
	}

	// native map_var_get_arr_safe(Map:map, VariantTag:key, AnyTag:value[], value_size=sizeof(value), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_var_get_arr_safe, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_get<dyn_func_var, dyn_func_arr>(amx, params);
	}

	// native bool:map_set(Map:map, AnyTag:key, AnyTag:value, key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_set, 5)
	{
		return key_at<2, 4>::value_at<3, 5>::map_set<dyn_func, dyn_func>(amx, params);
	}

	// native bool:map_set_arr(Map:map, AnyTag:key, const AnyTag:value[], value_size=sizeof(value), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_set_arr, 6)
	{
		return key_at<2, 5>::value_at<3, 4, 6>::map_set<dyn_func, dyn_func_arr>(amx, params);
	}

	// native bool:map_set_str(Map:map, AnyTag:key, const value[], key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_set_str, 4)
	{
		return key_at<2, 4>::value_at<3>::map_set<dyn_func, dyn_func_str>(amx, params);
	}

	// native bool:map_set_var(Map:map, AnyTag:key, VariantTag:value, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_set_var, 4)
	{
		return key_at<2, 4>::value_at<3>::map_set<dyn_func, dyn_func_var>(amx, params);
	}

	// native bool:map_set_cell(Map:map, AnyTag:key, offset, AnyTag:value, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_set_cell, 5)
	{
		return key_at<2, 5>::map_set_cell<dyn_func>(amx, params);
	}

	// native bool:map_set_cell_safe(Map:map, AnyTag:key, offset, AnyTag:value, key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_set_cell_safe, 5)
	{
		return key_at<2, 5>::map_set_cell<dyn_func, 6>(amx, params);
	}

	// native bool:map_arr_set(Map:map, const AnyTag:key[], AnyTag:value, key_size=sizeof(key), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_set, 5)
	{
		return key_at<2, 4, 5>::value_at<3, 6>::map_set<dyn_func_arr, dyn_func>(amx, params);
	}

	// native bool:map_arr_set_arr(Map:map, const AnyTag:key[], const AnyTag:value[], value_size=sizeof(value), key_size=sizeof(key), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_set_arr, 7)
	{
		return key_at<2, 5, 6>::value_at<3, 4, 7>::map_set<dyn_func_arr, dyn_func_arr>(amx, params);
	}

	// native bool:map_arr_set_str(Map:map, const AnyTag:key[], const value[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_set_str, 5)
	{
		return key_at<2, 4, 5>::value_at<3>::map_set<dyn_func_arr, dyn_func_str>(amx, params);
	}

	// native bool:map_arr_set_var(Map:map, const AnyTag:key[], VariantTags:value, key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_set_var, 5)
	{
		return key_at<2, 4, 5>::value_at<3>::map_set<dyn_func_arr, dyn_func_var>(amx, params);
	}

	// native bool:map_arr_set_cell(Map:map, const AnyTag:key[], offset, AnyTag:value, key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_set_cell, 6)
	{
		return key_at<2, 5, 6>::map_set_cell<dyn_func_arr>(amx, params);
	}

	// native bool:map_arr_set_cell_safe(Map:map, const AnyTag:key[], offset, AnyTag:value, key_size=sizeof(key), key_tag_id=tagof(key), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_set_cell_safe, 6)
	{
		return key_at<2, 5, 6>::map_set_cell<dyn_func_arr, 7>(amx, params);
	}

	// native bool:map_str_set(Map:map, const key[], AnyTag:value, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_str_set, 4)
	{
		return key_at<2>::value_at<3, 4>::map_set<dyn_func_str, dyn_func>(amx, params);
	}

	// native bool:map_str_set_arr(Map:map, const key[], const AnyTag:value[], value_size=sizeof(value), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_str_set_arr, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_set<dyn_func_str, dyn_func_arr>(amx, params);
	}

	// native bool:map_str_set_str(Map:map, const key[], const value[]);
	AMX_DEFINE_NATIVE(map_str_set_str, 3)
	{
		return key_at<2>::value_at<3>::map_set<dyn_func_str, dyn_func_str>(amx, params);
	}

	// native bool:map_str_set_var(Map:map, const key[], VariantTag:value);
	AMX_DEFINE_NATIVE(map_str_set_var, 3)
	{
		return key_at<2>::value_at<3>::map_set<dyn_func_str, dyn_func_var>(amx, params);
	}

	// native bool:map_str_set_cell(Map:map, const key[], offset, AnyTag:value);
	AMX_DEFINE_NATIVE(map_str_set_cell, 4)
	{
		return key_at<2>::map_set_cell<dyn_func_str>(amx, params);
	}

	// native bool:map_str_set_cell_safe(Map:map, const key[], offset, AnyTag:value, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_str_set_cell_safe, 5)
	{
		return key_at<2>::map_set_cell<dyn_func_str, 5>(amx, params);
	}

	// native bool:map_var_set(Map:map, VariantTag:key, AnyTag:value, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_var_set, 4)
	{
		return key_at<2>::value_at<3, 4>::map_set<dyn_func_var, dyn_func>(amx, params);
	}

	// native bool:map_var_set_arr(Map:map, VariantTag:key, const AnyTag:value[], value_size=sizeof(value), value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_var_set_arr, 5)
	{
		return key_at<2>::value_at<3, 4, 5>::map_set<dyn_func_var, dyn_func_arr>(amx, params);
	}

	// native bool:map_var_set_str(Map:map, VariantTag:key, const value[]);
	AMX_DEFINE_NATIVE(map_var_set_str, 3)
	{
		return key_at<2>::value_at<3>::map_set<dyn_func_var, dyn_func_str>(amx, params);
	}

	// native bool:map_var_set_var(Map:map, VariantTag:key, VariantTag:value);
	AMX_DEFINE_NATIVE(map_var_set_var, 3)
	{
		return key_at<2>::value_at<3>::map_set<dyn_func_var, dyn_func_var>(amx, params);
	}

	// native bool:map_var_set_cell(Map:map, VariantTag:key, offset, AnyTag:value);
	AMX_DEFINE_NATIVE(map_var_set_cell, 4)
	{
		return key_at<2>::map_set_cell<dyn_func_var>(amx, params);
	}

	// native bool:map_var_set_cell_safe(Map:map, VariantTag:key, offset, AnyTag:value, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_var_set_cell_safe, 5)
	{
		return key_at<2>::map_set_cell<dyn_func_var, 5>(amx, params);
	}

	// native map_key_at(Map:map, index, offset=0);
	AMX_DEFINE_NATIVE(map_key_at, 3)
	{
		return value_at<3>::map_key_at<dyn_func>(amx, params);
	}

	// native map_arr_key_at(Map:map, index, AnyTag:key[], key_size=sizeof(key));
	AMX_DEFINE_NATIVE(map_arr_key_at, 4)
	{
		return value_at<3, 4>::map_key_at<dyn_func_arr>(amx, params);
	}

	// native Variant:map_var_key_at(Map:map, index);
	AMX_DEFINE_NATIVE(map_var_key_at, 2)
	{
		return value_at<>::map_key_at<dyn_func_var>(amx, params);
	}

	// native bool:map_key_at_safe(Map:map, index, &AnyTag:key, offset=0, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_key_at_safe, 5)
	{
		return value_at<3, 4, 5>::map_key_at<dyn_func>(amx, params);
	}

	// native map_arr_key_at_safe(Map:map, index, AnyTag:key[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_key_at_safe, 5)
	{
		return value_at<3, 4, 5>::map_key_at<dyn_func_arr>(amx, params);
	}

	// native map_value_at(Map:map, index, offset=0);
	AMX_DEFINE_NATIVE(map_value_at, 3)
	{
		return value_at<3>::map_value_at<dyn_func>(amx, params);
	}

	// native map_arr_value_at(Map:map, index, AnyTag:value[], value_size=sizeof(value));
	AMX_DEFINE_NATIVE(map_arr_value_at, 4)
	{
		return value_at<3, 4>::map_value_at<dyn_func_arr>(amx, params);
	}

	// native Variant:map_var_value_at(Map:map, index);
	AMX_DEFINE_NATIVE(map_var_value_at, 2)
	{
		return value_at<>::map_value_at<dyn_func_var>(amx, params);
	}

	// native bool:map_value_at_safe(Map:map, index, &AnyTag:value, offset=0, value_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_value_at_safe, 5)
	{
		return value_at<3, 4, 5>::map_value_at<dyn_func>(amx, params);
	}

	// native map_arr_value_at_safe(Map:map, index, AnyTag:value[], value_size=sizeof(value), key_tag_id=tagof(value));
	AMX_DEFINE_NATIVE(map_arr_value_at_safe, 5)
	{
		return value_at<3, 4, 5>::map_value_at<dyn_func_arr>(amx, params);
	}

	// native map_tagof(Map:map, AnyTag:key, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_tagof, 3)
	{
		return key_at<2, 3>::map_tagof<dyn_func>(amx, params);
	}

	// native map_sizeof(Map:map, AnyTag:key, key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_sizeof, 3)
	{
		return key_at<2, 3>::map_sizeof<dyn_func>(amx, params);
	}

	// native map_arr_tagof(Map:map, const AnyTag:key[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_tagof, 4)
	{
		return key_at<2, 3, 4>::map_tagof<dyn_func_arr>(amx, params);
	}

	// native map_arr_sizeof(Map:map, const AnyTag:key[], key_size=sizeof(key), key_tag_id=tagof(key));
	AMX_DEFINE_NATIVE(map_arr_sizeof, 4)
	{
		return key_at<2, 3, 4>::map_sizeof<dyn_func_arr>(amx, params);
	}

	// native map_str_tagof(Map:map, const key[]);
	AMX_DEFINE_NATIVE(map_str_tagof, 2)
	{
		return key_at<2>::map_tagof<dyn_func_str>(amx, params);
	}

	// native map_str_sizeof(Map:map, const key[]);
	AMX_DEFINE_NATIVE(map_str_sizeof, 2)
	{
		return key_at<2>::map_sizeof<dyn_func_str>(amx, params);
	}

	// native map_var_tagof(Map:map, VariantTag:key);
	AMX_DEFINE_NATIVE(map_var_tagof, 2)
	{
		return key_at<2>::map_tagof<dyn_func_var>(amx, params);
	}

	// native map_var_sizeof(Map:map, VariantTag:key);
	AMX_DEFINE_NATIVE(map_var_sizeof, 2)
	{
		return key_at<2>::map_sizeof<dyn_func_var>(amx, params);
	}
}

static AMX_NATIVE_INFO native_list[] =
{
	AMX_DECLARE_NATIVE(map_new),
	AMX_DECLARE_NATIVE(map_new_args),
	AMX_DECLARE_NATIVE(map_new_args_str),
	AMX_DECLARE_NATIVE(map_new_args_var),
	AMX_DECLARE_NATIVE(map_new_str_args),
	AMX_DECLARE_NATIVE(map_new_str_args_str),
	AMX_DECLARE_NATIVE(map_new_str_args_var),
	AMX_DECLARE_NATIVE(map_new_var_args),
	AMX_DECLARE_NATIVE(map_new_var_args_str),
	AMX_DECLARE_NATIVE(map_new_var_args_var),
	AMX_DECLARE_NATIVE(map_valid),
	AMX_DECLARE_NATIVE(map_delete),
	AMX_DECLARE_NATIVE(map_delete_deep),
	AMX_DECLARE_NATIVE(map_clone),
	AMX_DECLARE_NATIVE(map_size),
	AMX_DECLARE_NATIVE(map_clear),
	AMX_DECLARE_NATIVE(map_set_ordered),
	AMX_DECLARE_NATIVE(map_is_ordered),
	AMX_DECLARE_NATIVE(map_add),
	AMX_DECLARE_NATIVE(map_add_arr),
	AMX_DECLARE_NATIVE(map_add_str),
	AMX_DECLARE_NATIVE(map_add_var),
	AMX_DECLARE_NATIVE(map_arr_add),
	AMX_DECLARE_NATIVE(map_arr_add_arr),
	AMX_DECLARE_NATIVE(map_arr_add_str),
	AMX_DECLARE_NATIVE(map_arr_add_var),
	AMX_DECLARE_NATIVE(map_str_add),
	AMX_DECLARE_NATIVE(map_str_add_arr),
	AMX_DECLARE_NATIVE(map_str_add_str),
	AMX_DECLARE_NATIVE(map_str_add_var),
	AMX_DECLARE_NATIVE(map_var_add),
	AMX_DECLARE_NATIVE(map_var_add_arr),
	AMX_DECLARE_NATIVE(map_var_add_str),
	AMX_DECLARE_NATIVE(map_var_add_var),
	AMX_DECLARE_NATIVE(map_add_map),
	AMX_DECLARE_NATIVE(map_add_args),
	AMX_DECLARE_NATIVE(map_add_args_str),
	AMX_DECLARE_NATIVE(map_add_args_var),
	AMX_DECLARE_NATIVE(map_add_str_args),
	AMX_DECLARE_NATIVE(map_add_str_args_str),
	AMX_DECLARE_NATIVE(map_add_str_args_var),
	AMX_DECLARE_NATIVE(map_add_var_args),
	AMX_DECLARE_NATIVE(map_add_var_args_str),
	AMX_DECLARE_NATIVE(map_add_var_args_var),
	AMX_DECLARE_NATIVE(map_remove),
	AMX_DECLARE_NATIVE(map_arr_remove),
	AMX_DECLARE_NATIVE(map_str_remove),
	AMX_DECLARE_NATIVE(map_var_remove),
	AMX_DECLARE_NATIVE(map_has_key),
	AMX_DECLARE_NATIVE(map_has_arr_key),
	AMX_DECLARE_NATIVE(map_has_str_key),
	AMX_DECLARE_NATIVE(map_has_var_key),
	AMX_DECLARE_NATIVE(map_get),
	AMX_DECLARE_NATIVE(map_get_arr),
	AMX_DECLARE_NATIVE(map_get_var),
	AMX_DECLARE_NATIVE(map_get_safe),
	AMX_DECLARE_NATIVE(map_get_arr_safe),
	AMX_DECLARE_NATIVE(map_arr_get),
	AMX_DECLARE_NATIVE(map_arr_get_arr),
	AMX_DECLARE_NATIVE(map_arr_get_var),
	AMX_DECLARE_NATIVE(map_arr_get_safe),
	AMX_DECLARE_NATIVE(map_arr_get_arr_safe),
	AMX_DECLARE_NATIVE(map_str_get),
	AMX_DECLARE_NATIVE(map_str_get_arr),
	AMX_DECLARE_NATIVE(map_str_get_var),
	AMX_DECLARE_NATIVE(map_str_get_safe),
	AMX_DECLARE_NATIVE(map_str_get_arr_safe),
	AMX_DECLARE_NATIVE(map_var_get),
	AMX_DECLARE_NATIVE(map_var_get_arr),
	AMX_DECLARE_NATIVE(map_var_get_var),
	AMX_DECLARE_NATIVE(map_var_get_safe),
	AMX_DECLARE_NATIVE(map_var_get_arr_safe),
	AMX_DECLARE_NATIVE(map_set),
	AMX_DECLARE_NATIVE(map_set_arr),
	AMX_DECLARE_NATIVE(map_set_str),
	AMX_DECLARE_NATIVE(map_set_var),
	AMX_DECLARE_NATIVE(map_set_cell),
	AMX_DECLARE_NATIVE(map_set_cell_safe),
	AMX_DECLARE_NATIVE(map_arr_set),
	AMX_DECLARE_NATIVE(map_arr_set_arr),
	AMX_DECLARE_NATIVE(map_arr_set_str),
	AMX_DECLARE_NATIVE(map_arr_set_var),
	AMX_DECLARE_NATIVE(map_arr_set_cell),
	AMX_DECLARE_NATIVE(map_arr_set_cell_safe),
	AMX_DECLARE_NATIVE(map_str_set),
	AMX_DECLARE_NATIVE(map_str_set_arr),
	AMX_DECLARE_NATIVE(map_str_set_str),
	AMX_DECLARE_NATIVE(map_str_set_var),
	AMX_DECLARE_NATIVE(map_str_set_cell),
	AMX_DECLARE_NATIVE(map_str_set_cell_safe),
	AMX_DECLARE_NATIVE(map_var_set),
	AMX_DECLARE_NATIVE(map_var_set_arr),
	AMX_DECLARE_NATIVE(map_var_set_str),
	AMX_DECLARE_NATIVE(map_var_set_var),
	AMX_DECLARE_NATIVE(map_var_set_cell),
	AMX_DECLARE_NATIVE(map_var_set_cell_safe),
	AMX_DECLARE_NATIVE(map_key_at),
	AMX_DECLARE_NATIVE(map_arr_key_at),
	AMX_DECLARE_NATIVE(map_var_key_at),
	AMX_DECLARE_NATIVE(map_key_at_safe),
	AMX_DECLARE_NATIVE(map_arr_key_at_safe),
	AMX_DECLARE_NATIVE(map_value_at),
	AMX_DECLARE_NATIVE(map_arr_value_at),
	AMX_DECLARE_NATIVE(map_var_value_at),
	AMX_DECLARE_NATIVE(map_value_at_safe),
	AMX_DECLARE_NATIVE(map_arr_value_at_safe),
	AMX_DECLARE_NATIVE(map_tagof),
	AMX_DECLARE_NATIVE(map_sizeof),
	AMX_DECLARE_NATIVE(map_arr_tagof),
	AMX_DECLARE_NATIVE(map_arr_sizeof),
	AMX_DECLARE_NATIVE(map_str_tagof),
	AMX_DECLARE_NATIVE(map_str_sizeof),
	AMX_DECLARE_NATIVE(map_var_tagof),
	AMX_DECLARE_NATIVE(map_var_sizeof),
};

int RegisterMapNatives(AMX *amx)
{
	return amx_Register(amx, native_list, sizeof(native_list) / sizeof(*native_list));
}
