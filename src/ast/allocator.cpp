#include "allocator.h"
#include <cstdlib>

namespace ast
{

#ifndef BOZON_NO_ARENA

constexpr size_t default_node_capacity =  1024 * 1024;

struct node_t
{
	size_t size;
	size_t capacity;
	node_t *prev_node;
	node_t *next_node;
	alignas(arena_allocator::min_alignment) uint8_t data[];
};

struct arena_t
{
	node_t *first_node;
	node_t *last_node;

	~arena_t(void)
	{
		// size_t size = 0;
		// size_t capacity = 0;
		for (auto node = this->first_node; node != nullptr;)
		{
			auto const next_node = node->next_node;
			// size += node->size;
			// capacity += node->capacity;
			std::free(node);
			node = next_node;
		}
		// bz::log("Memory allocated by arena_allocator: {}/{}\n", size, capacity);
	}
};

static arena_t arena = { nullptr, nullptr };

void *arena_allocator::sized_allocate(size_t size)
{
	size = size % min_alignment == 0 ? size : size + min_alignment - size % min_alignment;
	if (size > default_node_capacity)
	{
		auto const new_node_memory = std::malloc(sizeof (node_t) + size);
		if (new_node_memory == nullptr) bz_unlikely
		{
			bz::log("Failed to allocate memory for {} bytes", size);
			bz_unreachable;
		}
		auto const new_node = new(new_node_memory) node_t{};
		// insert the new node to the beginning of the arena
		new_node->size = size;
		new_node->capacity = size;
		new_node->prev_node = nullptr;
		new_node->next_node = arena.first_node;
		arena.first_node = new_node;
		if (new_node->next_node != nullptr)
		{
			new_node->next_node->prev_node = new_node;
		}
		else
		{
			arena.last_node = new_node;
		}
		return new_node->data;
	}
	else if (arena.last_node == nullptr)
	{
		auto const new_node_memory = std::malloc(sizeof (node_t) + default_node_capacity);
		if (new_node_memory == nullptr) bz_unlikely
		{
			bz::log("Failed to allocate memory for {} bytes", size);
			bz_unreachable;
		}
		auto const new_node = new(new_node_memory) node_t{};
		new_node->capacity = default_node_capacity;
		new_node->prev_node = nullptr;
		new_node->next_node = nullptr;
		new_node->size = size;
		arena.first_node = new_node;
		arena.last_node = new_node;
		return new_node->data;
	}
	else if (arena.last_node->size + size <= arena.last_node->capacity)
	{
		auto const result = arena.last_node->data + arena.last_node->size;
		arena.last_node->size += size;
		return result;
	}
	else if (arena.last_node->next_node != nullptr)
	{
		arena.last_node = arena.last_node->next_node;
		arena.last_node->size = size;
		return arena.last_node->data;
	}
	else
	{
		auto const new_node_memory = std::malloc(sizeof (node_t) + default_node_capacity);
		if (new_node_memory == nullptr) bz_unlikely
		{
			bz::log("Failed to allocate memory for {} bytes", size);
			bz_unreachable;
		}
		auto const new_node = new(new_node_memory) node_t{};
		new_node->capacity = default_node_capacity;
		new_node->size = size;
		new_node->prev_node = arena.last_node;
		new_node->next_node = nullptr;
		arena.last_node->next_node = new_node;
		arena.last_node = new_node;
		return new_node->data;
	}
}

void arena_allocator::sized_free(void *p, size_t size)
{
	if (arena.last_node->data + arena.last_node->size - size == p)
	{
		arena.last_node->size -= size;
	}
}

void arena_allocator::unsized_free([[maybe_unused]] void *p)
{
	// nothing
}

#else

void *arena_allocator::sized_allocate(size_t size)
{
	return std::malloc(size);
}

void arena_allocator::sized_free(void *p, [[maybe_unused]] size_t size)
{
	std::free(p);
}

void arena_allocator::unsized_free(void *p)
{
	std::free(p);
}

#endif // BOZON_NO_ARENA

} // namespace ast
