#ifndef DYN_OBJECT_H_INCLUDED
#define DYN_OBJECT_H_INCLUDED

#include "sdk/amx/amx.h"
#include <memory>
#include <string>
#include <cstring>

struct dyn_object
{
	bool is_array;
	union{
		cell cell_value;
		size_t array_size;
	};
	std::unique_ptr<cell[]> array_value;
	std::string tag_name;
	std::string find_tag(AMX *amx, cell tag_id);

	dyn_object(AMX *amx, cell value, cell tag_id);
	dyn_object(AMX *amx, cell *arr, size_t size, cell tag_id);
	bool check_tag(AMX *amx, cell tag_id);
};

#endif