// This file is part of the Trinity Engine, Copyright (c) 2020 Web eWorks, LTD.
//
// This file is a (heavily) altered version of TwoECS, released under the terms
// of the Zlib License:
//
// Copyright (c) 2020 stillwwater
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#pragma once

/* clang-format off */

#include <array>
#include <type_traits>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <cstdint>

// Enable assertions.
#ifndef NDEBUG
#	include <cassert>
#	define ASSERT(exp) assert(exp)
#else // NDEBUG
#	define ASSERT(exp) {}
#endif // NDEBUG

#define ASSERT_MSG(exp, msg) ASSERT(exp && msg)
#define ASSERT_ENTITY(entity) ASSERT(entity != entity_traits::null)

#ifndef LIKELY
#    if defined(__clang__) || defined(__GNUC__) || defined(__INTEL_COMPILER)
#        define LIKELY(x) __builtin_expect(!!(x), 1)
#        define UNLIKELY(x) __builtin_expect(!!(x), 0)
#    else
#        define LIKELY(x) (x)
#        define UNLIKELY(x) (x)
#    endif
#endif

// Allows C++ 17 like folding of template parameter packs in expressions
#define ECS_TEMPLATE_FOLD(exp)               \
	using Expand_ = char[];                  \
	(void)Expand_{((void)(exp), char(0))...}

namespace Trinity {

// A unique identifier representing each entity in the world.
// By default entity IDs are 32 bits, which should be sufficient for most games.
// If you need more than a million active entities, use sparse sets directly; this
// file will likely have too much overhead for your use case.
using Entity = uint32_t;
static_assert(std::is_unsigned<Entity>::value && std::is_integral<Entity>::value,
	"Entity ID must be an unsigned integer");

namespace entity_traits {
	using type = uint32_t;
	static constexpr type index_bits = 20;
	static constexpr type version_bits = 12;
	static constexpr type index_mask = (1 << index_bits) - 1;
	static constexpr type version_mask = ((1 << version_bits) - 1) << index_bits;
	static constexpr Entity null = Entity(0);
	static constexpr Entity max_entity = ~Entity(0);
} // namespace entity_traits

// Compile time id for a given type.
using type_id_t = size_t;
static_assert(std::is_same<type_id_t, std::uintptr_t>::value,
	"Type ID must be the same size as pointer type!");

namespace internal {
	template <typename T>
	struct TypeIdInfo {
		static const type_id_t *const value;
	};

	template <typename T>
	const type_id_t *const TypeIdInfo<T>::value = nullptr;

	// Type erased `unique_ptr`, this adds some memory overhead since it needs
	// a custom deleter.
	using unique_void_ptr_t = std::unique_ptr<void, void (*)(void *)>;

	template <typename T>
	unique_void_ptr_t unique_void_ptr(T *p) {
		return unique_void_ptr_t(p, [](void *data) {
			delete static_cast<T *>(data);
		});
	}

	// Type list support borrowed from EnTT with love.
	// This code is licensed under the The MIT License (MIT)
	// Copyright (c) 2017-2020 Michele Caini

	// A class to use to push around lists of types, nothing more.
	template<typename...>
	struct type_list {};

	// Primary template isn't defined on purpose.
	template<typename>
	struct type_list_size;

	// Compile-time number of elements in a type list.
	template<typename... Type>
	struct type_list_size<type_list<Type...>>
			: std::integral_constant<std::size_t, sizeof...(Type)>
	{};

	// Primary template isn't defined on purpose.
	template<typename...>
	struct type_list_cat;

	// Helper to concatenate empty type lists.
	template<>
	struct type_list_cat<> {
		/*! @brief A type list composed by the types of all the type lists. */
		using type = type_list<>;
	};

	// Concatenate multiple (2+) type lists
	template<typename... Type, typename... Other, typename... List>
	struct type_list_cat<type_list<Type...>, type_list<Other...>, List...> {
		using type = typename type_list_cat<type_list<Type..., Other...>, List...>::type;
	};

	// Helper to concatenate multiple type lists
	template<typename... Type>
	struct type_list_cat<type_list<Type...>> {
		using type = type_list<Type...>;
	};

	// End of type list code.

	// Workaround for no std::get<Type>(std::tuple<Types...>) in C++11.
#if _cplusplus < 201402L
	// From https://stackoverflow.com/questions/16594002/
	template<int Index, class Search, class First, class... Types>
	struct get_internal
	{
		typedef typename get_internal<Index + 1, Search, Types...>::type type;
			static constexpr int index = Index;
	};

	template<int Index, class Search, class... Types>
	struct get_internal<Index, Search, Search, Types...>
	{
		typedef get_internal type;
		static constexpr int index = Index;
	};

	template<class T, class... Types>
	T get(std::tuple<Types...> tuple)
	{
		return std::get<get_internal<0,T,Types...>::type::index>(tuple);
	}
#else
	using std::get;
#endif

} // namespace internal

// Compile time typeid.
// TODO: this version will not work across boundaries. Evaluate the use of e.g. PRETTY_FUNCTION.
template <typename T>
constexpr type_id_t type_id() {
	using actual_type_t = typename std::remove_pointer<typename std::decay<T>::type>::type;
	return reinterpret_cast<std::uintptr_t>(static_cast<const void *>(&internal::TypeIdInfo<actual_type_t>::value));
}

// A simple optional type in order to support C++ 11.
template <typename T>
class Optional {
public:
	bool has_value;

	Optional(const T &value) : has_value{true}, val{value} {}
	Optional(T &&value) : has_value{true}, val{std::move(value)} {}
	Optional() : has_value{false} {}

	const T &value() const &;
	T &value() &;

	const T &&value() const &&;
	T &&value() &&;

	T value_or(T &&u) { has_value ? std::move(val) : u; };
	T value_or(const T &u) const { has_value ? val : u; };

private:
	T val;
};

// An event channel handles events for a single event type.
template <typename Event>
class EventChannel {
public:
	using EventHandler = std::function<bool (const Event &)>;

	// Adds a function as an event handler
	void bind(EventHandler &&fn);

	// Emits an event to all event handlers
	void emit(const Event &event) const;

private:
	std::vector<EventHandler> handlers;
};

namespace ECS {

// Used to represent an entity that has no value. The `NullEntity` exists
// in the world but has no components.
constexpr Entity NullEntity = entity_traits::null;

// Creates an Entity id from an index and version
inline constexpr Entity entity_id(entity_traits::type i, entity_traits::type version) {
	return i | (version << entity_traits::index_bits);
}

// Returns the index part of an entity id.
inline constexpr entity_traits::type entity_index(Entity entity) {
	return entity & entity_traits::index_mask;
}

// Returns the version part of an entity id.
inline constexpr entity_traits::type entity_version(Entity entity) {
	return entity >> entity_traits::index_bits;
}

// An empty component that is used to indicate whether the entity it is
// attached to is currently active.
struct ActiveTag {};

class World;

// Base class for all systems. The lifetime of systems is managed by a SystemManager.
class ISystem {
public:
	virtual ~ISystem() = default;
	virtual void Init(World *world) {};
	virtual void Update(World *world, float dt) {};
	virtual void Uninit(World *world) {};
};

class SystemManager {
public:
	SystemManager() = delete;
	SystemManager(World *world);

	// Creates and adds a system to the world. This function calls
	// `System::load` to initialize the system.
	//
	// > Systems are not unique. 'Duplicate' systems, that is different
	// instances of the same system type, are allowed in the world.
	template <class T, typename... Args>
	T *make_system(Args &&...args);

	// Adds a system to the world. This function calls `System::load` to
	// initialize the system.
	//
	// > You may add multiple different instances of the same system type,
	// but passing a system pointer that points to an instance that is
	// already in the world is not allowed and will result in a debug assertion.
	template <class T>
	T *make_system(T *system);

	// Adds a system to the world before another system if it exists.
	// `Before` is an existing system.
	// `T` is the system to be added before an existing system.
	template <class Before, class T, typename... Args>
	T *make_system_before(Args &&...args);

	// Returns the first system that matches the given type.
	// System returned will be `nullptr` if it is not found.
	template <class T>
	T *get_system();

	// Returns all system that matches the given type.
	// Systems returned will not be null.
	template <class T>
	void get_all_systems(std::vector<T *> *systems);

	// Iterates through all systems registered with this manager and
	// calls their Update methods. The caller is welcome to use systems()
	// and handle more fine-grained updates as needed.
	void update(float dt);

	// System must not be null. Do not destroy a system while the main update
	// loop is running as it could invalidate the system iterator, consider
	// checking if the system should run or not in the system update loop
	// instead.
	void destroy_system(ISystem *system);

	// Destroy all systems in the world.
	void destroy_systems();

	// Systems returned will not be null.
	inline const std::vector<ISystem *> &systems() { return active_systems; }

	// Get the owning world of this manager.
	inline World *get_world() const { return world; }

private:
	World *world;

	// Systems' lifetimes are controlled by their manager.
	std::vector<ISystem *> active_systems;

	// Kept separate since most of the time we just want to iterate through
	// all the systems and do not need to know their types.
	std::vector<type_id_t> active_system_types;
};

// Maintains a "sparse" random-access lookup table that maps Entity IDs
// to indexes in a separate packed array.
// Component storage should be built on top of this structure.
class SparseSet {
public:
	using PackedType = entity_traits::type;
	static_assert(std::is_integral<PackedType>::value,
		"PackedType must be an integral type!");
	// by default, the null entry is equal to std::numeric_limits<T>::max();
	static constexpr PackedType null = ~(PackedType{});

	static constexpr int page_size = 4096;
	static_assert((page_size & (page_size - 1)) == 0,
		"SparseSet::page_size must be a power of two!");
	using page_t = std::unique_ptr<PackedType[]>;
	using iterator = typename std::vector<Entity>::reverse_iterator;
	using const_iterator = typename std::vector<Entity>::const_reverse_iterator;

	size_t size() const { return packed.size(); }
	Entity *data() { return packed.data(); }
	std::vector<Entity> &get_packed() { return packed; }

	// Use reverse iterator semantics to avoid invalidating references by pushing to the array.
	// TODO: evaluate whether we need to use a different iterator to avoid invalidation on resize.
	iterator begin() { return packed.rbegin(); }
	iterator end() { return packed.rend(); }

	const_iterator cbegin() { return packed.crbegin(); }
	const_iterator cend() { return packed.crend(); }

	void clear() { sparse.clear(); packed.clear(); }
	void shrink_to_fit() {
		if (packed.empty()) sparse.clear();
		packed.shrink_to_fit();
	}

	bool contains(Entity id) const {
		return page(id) < sparse.size() && sparse[page(id)] && sparse[page(id)][offset(id)] != null;
	}

	void emplace(Entity new_ent) {
		(*this)[new_ent] = packed.size();
		packed.push_back(new_ent);
	}

	// Remove an entry from the packed list by swapping it with the last
	// entry in the packed list.
	void remove(Entity to_remove) {
		size_t last = packed.size() - 1;
		// Overwrite the old slot in the packed array with the entity that "owns" the component we just moved.
		Entity moved_entity = packed[last];
		packed[to_remove] = packed[last];
		packed.pop_back();

		// Update the entities in the sparse array.
		(*this)[moved_entity] = to_remove;
		(*this)[to_remove] = null;
	}

	// Return a reference to the value slot in this sparse array,
	// creating a new page as necessary.
	PackedType &operator[](Entity id) { return assure(page(id))[offset(id)]; }

	// Returns a reference to the value slot in this sparse array,
	// or returns a const reference to an invalid value;
	const PackedType &operator[](Entity id) const {
		if (contains(id)) return sparse[page(id)][offset(id)];
		else return null;
	}

private:
	page_t &assure(size_t pos) {
		if (pos >= sparse.size())
			sparse.resize(pos + 1);

		auto &val = sparse[pos];
		if (UNLIKELY(!val)) {
			val.reset(new PackedType[page_size]);
			std::fill(val.get(), val.get() + page_size, null);
		}
		return val;
	}

	constexpr size_t page(Entity id) const { return entity_index(id) / page_size; }
	constexpr size_t offset(Entity id) const { return entity_index(id) & (page_size - 1); }

	std::vector<page_t> sparse;
	std::vector<Entity> packed;
};

class IComponentArray {
public:
	// Entity int type is used to ensure we can address the maximum
	// number of entities.
	virtual ~IComponentArray() = default;
	virtual SparseSet &get_sparse() = 0;
	virtual bool remove(Entity entity) = 0;
	virtual void copy(Entity dst, Entity src) = 0;
};

// Manages all instances of a component type and keeps track of which
// entity a component is attached to.
template <typename T, typename Allocator = std::allocator<T>, typename = void>
class ComponentArray final : public IComponentArray {
public:
	static_assert(std::is_copy_assignable<T>(),
				  "Component type must be copy assignable");

	static_assert(std::is_copy_constructible<T>(),
				  "Component type must be copy constructible");

	ComponentArray() {
		// Reduce the number of allocations by reserving room for the first 32 components.
		// 32 is a magic number chosen for reducing excessive STL container allocations.
		instances.reserve(32);
		sparse.get_packed().reserve(32);
	}

	// Returns a component of type T given an Entity.
	// Note: References returned by this function are only guaranteed to be
	// valid until the next time the container is sorted. Don't hold reference.
	inline T &read(Entity entity) {
		ASSERT_MSG(contains(entity), "Missing component on Entity.");
		return instances[sparse[entity]];
	}

	// Set a component in the packed array and associate an entity with the
	// component.
	T &write(Entity entity, const T &component) {
		auto &pos = sparse[entity];
		if (pos != SparseSet::null) {
			// Replace component
			return instances[pos] = component;
		}
		ASSERT(sparse.size() < entity_traits::max_entity);

		sparse.emplace(entity);
		instances.push_back(component);

		return instances[pos];
	}

	// Create a component for the given entity from its constructor arguments.
	// It is an error to call this function if the given entity already contains
	// a component of this type.
	template<typename ...Args>
	T &emplace(const Entity entity, Args &&... args) {
		instances.emplace_back(std::forward<Args>(args)...);

		// sparse array update goes after instance creation so constructor
		// errors don't leave the array in an unsafe state.
		sparse.emplace(entity);
		return instances.back();
	}

	// Invalidate this component type for an entity. Returns true if the
	// component was removed.
	//
	// This function will always succeed, even if the entity does not
	// contain a component of this type.
	//
	// > References returned by `read` may become invalid after remove is
	// called.
	bool remove(Entity entity) override {
		auto removed = sparse[entity];
		if (removed == SparseSet::null) {
			// This is a no-op since calling this as a virtual member function
			// means there is no way for the caller to check if the entity
			// contains a component. `contains` is not virtual as it needs to
			// be fast.
			return false;
		}

		// Move the last component into the empty slot to keep the array packed
		// use std::swap here to avoid needing copy-construction
		std::swap(instances[removed], instances[instances.size() - 1]);
		instances.pop_back();
		sparse.remove(entity);

		return true;
	}

	// Copy component to `dst` from `src`.
	void copy(Entity dst, Entity src) override { write(dst, read(src)); }

	SparseSet &get_sparse() override { return sparse; }

	// Returns true if the entity has a component of type T.
	inline bool contains(Entity entity) const { return sparse.contains(entity); }

	// Returns the number of valid components in the packed array.
	inline size_t size() const { return sparse.size(); }

	// Request that all arrays have their capacity shrunk to no greater than the
	// number of elements currently stored in the array. This request is non-binding.
	// Please don't do this if you intend to add further elements to the array.
	void shrink_to_fit() {
		sparse.shrink_to_fit();
		instances.shrink_to_fit();
	}

private:
	// Maps an Entity id to an index in the packed+instance array.
	SparseSet sparse;
	// All instances of component type T are stored in a contiguous vector.
	std::vector<T, Allocator> instances;
};

// Template specialization of ComponentArray for zero-size objects (e.g. tags)
// Avoids all overhead with needing a packed array and simply stores a boolean value.
template<typename T>
class ComponentArray<T, std::allocator<T>, typename std::enable_if<std::is_empty<T>::value>::type> final : public IComponentArray {
public:

	inline bool contains(const Entity entity) const { return sparse.contains(entity); }
	inline size_t size() const { return contained_size; }

	SparseSet &get_sparse() override { return sparse; }

	T &read(const Entity entity) {
		return null_instance;
	}

	T &write(const Entity entity, const T &copy_from) {
		if (sparse.contains(entity)) return null_instance;
		++contained_size;
		sparse.emplace(entity);
		return null_instance;
	}

	template<typename ...Args>
	T &emplace(const Entity entity, Args &&... args) {
		[[maybe_unused]]T throwaway{std::forward<Args>(args)...};
		sparse.emplace(entity);
		++contained_size;
		return null_instance;
	}

	bool remove(Entity entity) override {
		if (!sparse.contains(entity))
			return false;

		sparse.remove(entity);
		--contained_size;
		return true;
	}

	void copy(Entity dst, Entity src) override {
		if (!sparse.contains(dst)) ++contained_size;
		sparse[dst] = sparse[src];
	}

private:
	SparseSet sparse;
	T null_instance = {};
	size_t contained_size = 0;
};

// A view provides a non-owning interface to iterate a collection of componeents.
template<typename ...Components>
class ComponentView {
	friend class World;
	ComponentView(World *p);

	// convenience typedef to contain the list of secondary component pools.
	using other_type = std::array<SparseSet *, (sizeof...(Components) - 1)>;

	template <typename T>
	using ViewFunc = typename std::common_type<std::function<T>>::type;

	// Multi-component view iterator.
	template<typename It>
	class view_iterator final {
		friend class ComponentView;
		view_iterator(It first, It last, It current, other_type &other) :
			first(first), last(last),
			it(current),
			other(other)
		{
			if (it != last && !valid()) ++(*this);
		}

		bool valid() const noexcept {
			Entity ent = *it;
			return sizeof...(Components) == 1 || std::all_of(other.cbegin(), other.cend(), [=](SparseSet *const set){ return set->contains(ent); });
		}

	public:
		using difference_type = typename std::iterator_traits<It>::difference_type;
        using value_type = typename std::iterator_traits<It>::value_type;
        using pointer = typename std::iterator_traits<It>::pointer;
        using reference = typename std::iterator_traits<It>::reference;
        using iterator_category = std::bidirectional_iterator_tag;

		view_iterator &operator++() {
			while (++it != last && !valid()) {}
			return *this;
		}

		view_iterator operator++(int) {
			view_iterator orig = *this;
			return ++(*this), orig;
		}

		view_iterator & operator--() noexcept {
            while(--it != first && !valid());
            return *this;
        }

        view_iterator operator--(int) noexcept {
            view_iterator orig = *this;
            return operator--(), orig;
        }

        bool operator==(const view_iterator &other) const noexcept {
            return other.it == it;
        }

        bool operator!=(const view_iterator &other) const noexcept {
            return !(*this == other);
        }

        pointer operator->() const {
            return &*it;
        }

        reference operator*() const {
            return *operator->();
        }

	private:
		It first;
		It last;
		It it;
		other_type &other;
	};

	// Internal specialization for [](Entity, ComponentA &, ... ComponentN &)
	template<typename ...Types>
	void traverse(ViewFunc<void (Entity, Types &...)> &&func, internal::type_list<Types...>);

	// Internal specialization for [](ComponentA &, ... ComponentN &)
	template<typename ...Types>
	void traverse(ViewFunc<void (Types &...)> &&func, internal::type_list<Types...>);

public:

	using iterator = view_iterator<typename SparseSet::iterator>;
	using const_iterator = view_iterator<typename SparseSet::const_iterator>;

	iterator begin() {
		return { view->begin(), view->end(), view->begin(), other_views };
	}
	iterator end() {
		return { view->begin(), view->end(), view->end(), other_views };
	}

	const_iterator cbegin() {
		return { view->cbegin(), view->cend(), view->cbegin(), other_views };
	}
	const_iterator cend() {
		return { view->cbegin(), view->cend(), view->cend(), other_views };
	}

	// Calls `fn` with a reference to each unpacked component for every entity
	// with all requested components.
	// NOTE: Empty components are omitted from the function arguments for ease
	// of use. If this function is causing template errors, you likely have an
	// incorrect set of parameters in the functor passed to this function.
	//
	// Proper usage:
	//     world.view<A, B, C, EmptyD>.each([](Entity e, A &a, B &b, C &c) {
	//         // ...
	//     });
	// OR without entity ID:
	//     world.view<A, B, C, EmptyD>.each([](A &a, B &b, C &c) {
	//         // ...
	//     });
	template<typename Func>
	inline void each(Func fn);

private:

	World *parent;
	std::tuple<ComponentArray<Components> *...> components;

	other_type other_views;
	SparseSet *view;
};

// A registry holds a collection of systems, components and entities.
// It is effectively a world and will be referred to as such.
class World {
public:
	template <typename T>
	using ViewFunc = typename std::common_type<std::function<T>>::type;

	World();

	World(const World &) = delete;
	World &operator=(const World &) = delete;

	World(World &&) = default;
	World &operator=(World &&) = default;

	virtual ~World() = default;

	// Returns the default SystemManager associated with this world.
	SystemManager &get_manager();

	// Creates a new entity in the world with an Active component.
	Entity make_entity();

	// Creates a new entity and copies components from another.
	Entity make_entity(Entity archetype);

	// Creates a new inactive entity in the world. The entity will need
	// to have active set before it can be used by systems.
	// Useful to create entities without initializing them.
	// > Note: Inactive entities still exist in the world and can have
	// components added to them.
	Entity make_inactive_entity();

	// Copy components from entity `src` to entity `dst`.
	void copy_entity(Entity dst, Entity src);

	// Destroys an entity and all of its components.
	void destroy_entity(Entity entity);

	// Adds or replaces a component and associates an entity with the
	// component.
	//
	// Adding components will invalidate the cache. The number of cached views
	// is *usually* approximately equal to the number of systems, so this
	// operation is not that expensive but you should avoid doing it every
	// frame. This does not apply to 're-packing' components (see note below).
	//
	// > Note: If the entity already has a component of the same type, the old
	// component will be replaced. Replacing a component with a new instance
	// is *much* faster than calling `remove` and then `pack` which
	// would result in the cache being rebuilt twice. Replacing a component
	// does not invalidate the cache and is cheap operation.
	template <typename Component>
	Component &pack(Entity entity, const Component &component);

	// Shortcut to pack multiple components to an entity, equivalent to
	// calling `pack(entity, component)` for each component.
	//
	// Unlike the original `pack` function, this function does not return a
	// reference to the component that was just packed.
	template <typename C0, typename... Cn>
	void pack(Entity entity, const C0 &component, const Cn &...components);

	// Returns a component of the given type associated with an entity.
	// This function will only check if the component does not exist for an
	// entity if assertions are enabled, otherwise it is unchecked.
	// Use contains if you need to verify the existence of a component
	// before removing it. This is a cheap operation.
	//
	// This function returns a reference to a component in the packed array.
	// The reference may become invalid after `remove` is called since `remove`
	// may move components in memory.
	//
	//     auto &a = world.unpack<A>(entity1);
	//     a.x = 5; // a is a valid reference and x will be updated.
	//
	//     auto b = world.unpack<B>(entity1); // copy B
	//     world.remove<B>(entity2);
	//     b.x = 5;
	//     world.pack(entity1, b); // Ensures b will be updated in the array
	//
	//     auto &c = world.unpack<C>(entity1);
	//     world.remove<C>(entity2);
	//     c.x = 5; // may not update c.x in the array
	//
	// If you plan on holding the reference it is better to copy the
	// component and then pack it again if you have modified the component.
	// Re-packing a component is a cheap operation and will not invalidate.
	// the cache.
	//
	// > Do not store this reference between frames such as in a member
	// variable, store the entity instead and call unpack each frame. This
	// operation is designed to be called multiple times per frame so it
	// is very fast, there is no need to `cache` a component reference in
	// a member variable.
	template <typename Component>
	inline Component &unpack(Entity entity);

	// Returns true if a component of the given type is associated with an
	// entity. This is a cheap operation.
	template <typename Component>
	inline bool contains(Entity entity) const;

	// Returns true if an entity contains all given components.
	template <typename C0, typename... Cn,
		typename Enable = typename std::enable_if<(sizeof...(Cn) > 0)>::type>
	inline bool contains(Entity entity) const;

	// Removes a component from an entity. Removing components invalidates
	// the cache.
	//
	// If the entity does not have the component being removed this function
	// is a noop and will succeed.
	template <typename Component>
	void remove(Entity entity);

	// Adds or removes an Active component.
	//
	// > When calling `set_active(entity, true)` with an entity that is already
	// active this function won't do anything. The same is true when calling
	// `set_active(entity, false)` with an entity that is already inactive.
	inline void set_active(Entity entity, bool active);

	// Returns all entities that have all requested components. If you would
	// like to only match active entities, you will manually need to include
	// ECS::ActiveTag in the list of components.
	//
	//     for (auto entity : view<A, B, C>()) {
	//         auto &a = unpack<A>(entity);
	//         // ...
	//     }
	//
	template <typename... Components>
	ComponentView<Components...> view();

	// Returns an owning group of a certain set of components.
	// A group takes ownership of the underlying array and sorts component
	// data to make iteration as fast as possible.
	//
	// The first call to this function will build a cache with the entities
	// that contain the requested components, subsequent calls will return
	// the cached data as long as the data is still valid. If a cache is no
	// longer valid, this function will rebuild the cache by applying all
	// necessary diffs to make the cache valid.
	//
	// The cost of rebuilding the cache depends on how many diff operations
	// are needed. Any operation that changes whether an entity should be
	// in this cache (does the entity have all requested components?) counts
	// as a single Add or Remove diff. Functions like `remove`,
	// `make_entity`, `pack`, `destroy_entity` may cause a cache to be
	// invalidated. Functions that may invalidate the cache are documented.
	// TODO: implement this functionality.
	// ComponentGroup<Components...> group(bool include_inactive = false);

	// Returns all entities in the world. Entities returned may be inactive.
	// > Note: Calling `destroy_entity()` will invalidate the iterator, use
	// `view<>(true)` to get all entities without having `destroy_entity()`
	// invalidate the iterator.
	inline const std::vector<Entity> &unsafe_view_all() { return entities; }

	// Creates a single instance of the requested component and returns the
	// the component. This is a convenience function for storing state in
	// the World instead of in System objects.
	//
	//     auto &camera = world.add_singleton<CameraData>();
	//
	template<typename Component>
	Component &add_singleton(const Component &initial = {});

	// Finds a single instance of the requested component and unpacks
	// the component requested. This is a convenience function for storing
	// state in the World.
	//
	//     auto &camera = world.get_singleton<CameraData>();
	//
	template<typename Component>
	Component &get_singleton();

	// Adds a function to receive events of type T
	template <typename Event>
	void bind(typename EventChannel<Event>::EventHandler &&fn);

	// Same as `bind(fn)` but allows a member function to be used
	// as an event handler.
	//
	//     bind<KeyDownEvent>(&World::keydown, this);
	template <typename Event, typename Func, class T>
	void bind(Func &&fn, T this_ptr);

	// Emits an event to all event handlers. If a handler function in the
	// chain returns true then the event is considered handled and will
	// not propagate to other listeners.
	template <typename Event>
	void emit(const Event &event) const;

	// Removes all event handlers, you'll unlikely need to call this since
	// events are cleared when the world is destroyed.
	inline void clear_event_channels() { channels.clear(); };

	// Registers a component type if it does not exist and returns it.
	// Components are registered automatically so there is usually no reason
	// to call this function.
	template<typename Component>
	inline ComponentArray<Component> &assure();

private:
	size_t alive_count = 0;

	template<typename Component>
	inline ComponentArray<Component> *get_array() const;

	SystemManager manager;

	// Contains available entity ids. When creating entities check if this
	// is not empty, otherwise use alive_count + 1 as the new id.
	std::vector<Entity> recycled_list;

	// All alive (but not necessarily active) entities.
	std::vector<Entity> entities;

	std::unordered_map<type_id_t, std::unique_ptr<IComponentArray>> component_pools;
	std::unordered_map<type_id_t, internal::unique_void_ptr_t> singleton_components;

	// Event channels.
	std::unordered_map<type_id_t, internal::unique_void_ptr_t> channels;
};

World::World() :
	manager(this)
{}

SystemManager &World::get_manager() {
	return manager;
}

template<typename Component>
inline ComponentArray<Component> &World::assure() {
	// Component may not have been regisered yet
	type_id_t type = type_id<Component>();

	if (UNLIKELY(component_pools[type].get() == nullptr))
		component_pools[type].reset(new ComponentArray<Component>());

	return *static_cast<ComponentArray<Component> *>(component_pools[type].get());
}

template<typename Component>
inline ComponentArray<Component> *World::get_array() const {
	// This function should not be used without first ensuring the requested component has been registered.
	return static_cast<ComponentArray<Component> *>(component_pools.at(type_id<Component>()).get());
}

template <typename Component>
Component &World::pack(Entity entity, const Component &component) {
	ASSERT_ENTITY(entity);
	auto &array = assure<Component>();
	if (!array.contains(entity))
		return array.emplace(entity, component);

	// TODO: invalidate any group caches here.

	return array.write(entity, component);
}

template <typename C0, typename... Cn>
void World::pack(Entity entity, const C0 &component, const Cn &...components) {
	pack(entity, component);
	pack(entity, components...);
}

template <typename Component>
inline Component &World::unpack(Entity entity) {
	// in general, unpack should not be called except from code that's already done bounds-checking
	return assure<Component>().read(entity);
}

template <typename Component>
inline bool World::contains(Entity entity) const {
	// This function must work if a component has never been registered,
	// since it's reasonable to check if an entity has a component when
	// a component type has never been added to any entity.
	return component_pools.count(type_id<Component>()) && get_array<Component>()->contains(entity);
}

template <typename C0, typename... Cn, typename Enable>
inline bool World::contains(Entity entity) const {
	return contains<C0>(entity) && contains<Cn...>(entity);
}

template <typename Component>
void World::remove(Entity entity) {
	// Assume component was registered when it was packed
	ASSERT(component_pools.count(type_id<Component>()));

	assure<Component>().remove(entity);
	// TODO: invalidate any group caches here
}

inline void World::set_active(Entity entity, bool active) {
	ASSERT_ENTITY(entity);
	// If active is constant this conditional should be removed when
	// the function is inlined
	if (active)
		assure<ActiveTag>().emplace(entity);
	else
		assure<ActiveTag>().remove(entity);
}

template <typename... Components>
ComponentView<Components ...> World::view() {
	// Component may not have been registered
	ECS_TEMPLATE_FOLD(assure<Components>());

	return ComponentView<Components...>(this);
}

inline Entity World::make_entity() {
	auto entity = make_inactive_entity();
	assure<ActiveTag>().emplace(entity);
	return entity;
}

inline Entity World::make_entity(Entity archetype) {
	ASSERT_ENTITY(archetype);
	auto entity = make_entity();
	copy_entity(entity, archetype);
	return entity;
}

inline Entity World::make_inactive_entity() {
	Entity entity;
	if (recycled_list.empty()) {
		ASSERT_MSG(alive_count < entity_traits::max_entity, "Too many entities");
		entity = alive_count++;

		// Create a nullentity that will never have any components
		// This is useful when we need to store entities in an array and
		// need a way to define entities that are not valid.
		if (entity == NullEntity) {
			entities.push_back(NullEntity);
			++entity;
			++alive_count;
		}
	} else {
		entity = recycled_list.back();
		recycled_list.pop_back();
		auto version = entity_version(entity);
		auto index = entity_index(entity);
		entity = entity_id(index, version + 1);
	}
	entities.push_back(entity);
	return entity;
}

inline void World::copy_entity(Entity dst, Entity src) {
	ASSERT_ENTITY(dst);
	for (const auto &pair : component_pools) {
		if (pair.second->get_sparse().contains(src))
			pair.second->copy(dst, src);
	}

	// TODO: update group caches here.
}

inline void World::destroy_entity(Entity entity) {
	ASSERT_ENTITY(entity);
	for (auto &pair : component_pools)
		pair.second->remove(entity);

	// TODO: update caches here.

	recycled_list.push_back(entity);
	entities[entity] = SparseSet::null;
}

template<typename Component>
Component &World::add_singleton(const Component &initial) {
	static_assert(std::is_copy_constructible<Component>::value,
		"Singleton Components must be copy constructible!");
	internal::unique_void_ptr_t &ptr = singleton_components[type_id<Component>()];
	ptr.reset(new Component(initial));
	return *static_cast<Component *>(ptr.get());
}

template<typename Component>
Component &World::get_singleton() {
	internal::unique_void_ptr_t &singleton_ptr = singleton_components[type_id<Component>()];
	ASSERT(singleton_ptr.get());
	return *static_cast<Component *>(singleton_ptr.get());
}

template <typename Event>
void World::bind(typename EventChannel<Event>::EventHandler &&fn) {
	constexpr auto type = type_id<Event>();
	if (channels.find(type) == channels.end()) {
		auto c = internal::unique_void_ptr<EventChannel<Event>>(new EventChannel<Event>);
		channels.emplace(std::make_pair(type, std::move(c)));
	}
	static_cast<EventChannel<Event> *>(channels.at(type).get())
		->bind(std::move(fn));
}

template <typename Event, typename Func, class T>
void World::bind(Func &&fn, T this_ptr) {
	bind<Event>(std::bind(fn, this_ptr, std::placeholders::_1));
}

template <typename Event>
void World::emit(const Event &event) const {
	auto chan_it = channels.find(type_id<Event>());
	if (chan_it == channels.end()) {
		return;
	}
	static_cast<EventChannel<Event> *>(chan_it->second.get())->emit(event);
}

template<typename ...Components>
ComponentView<Components...>::ComponentView(World *p) :
	parent(p)
{
	// Find the smallest list of components to speed up iteration.
	std::array<SparseSet *, sizeof...(Components)> pools = {
		&parent->assure<Components>().get_sparse()...
	};

	components = std::make_tuple(&parent->assure<Components>()...);

	size_t min = ~size_t(0);
	for (int i = 0; i < sizeof...(Components); i++) {
		if (pools[i]->size() < min) {
			min = pools[i]->size();
			view = pools[i];
		}
	}

	// build the list of other sparse sets
	other_type other;
	size_t pos = 0;
	ECS_TEMPLATE_FOLD(&parent->assure<Components>().get_sparse() == view ?
		nullptr : (other_views[pos] = &parent->assure<Components>().get_sparse(), other[pos++]));
}

template <typename... Components>
template <typename ...Types>
void ComponentView<Components...>::traverse(ViewFunc<void (Types &...)> &&fn, internal::type_list<Types...>) {
	for (const auto entity : *this) {
		fn(internal::get<ComponentArray<Types> *>(components)->read(entity)...);
	}
}

template <typename... Components>
template <typename ...Types>
void ComponentView<Components...>::traverse(ViewFunc<void (Entity, Types &...)> &&fn, internal::type_list<Types...>) {
	for (const auto entity : *this) {
		ASSERT_ENTITY(entity);
		fn(entity, internal::get<ComponentArray<Types> *>(components)->read(entity)...);
	}
}

template <typename... Components>
template <typename Func>
inline void ComponentView<Components...>::each(Func fn)
{
	using non_empty_type = typename internal::type_list_cat<typename std::conditional<
		std::is_empty<Components>::value,
		internal::type_list<>,
		internal::type_list<Components>
	>::type...>::type;
	traverse(std::move(fn), non_empty_type{});
}

SystemManager::SystemManager(World *world) :
	world(world)
{}

template <class T, typename... Args>
T *SystemManager::make_system(Args &&...args) {
	return make_system(new T(std::forward<Args>(args)...));
}

template <class T>
T *SystemManager::make_system(T *system) {
	static_assert(std::is_convertible<T *, ISystem *>(),
				  "Type cannot be converted to a System");
	ASSERT_MSG(std::find(active_systems.begin(),
 		active_systems.end(), system) == active_systems.end(),
		"Duplicate systems in the world");

	active_systems.push_back(system);
	active_system_types.push_back(type_id<T>());
	system->Init(world);
	return system;
}

template <class Before, class T, typename... Args>
T *SystemManager::make_system_before(Args &&...args) {
	static_assert(std::is_convertible<T *, ISystem *>(),
		"Type cannot be converted to a System");
	ASSERT(active_systems.size() == active_system_types.size());

	auto pos = std::find(active_system_types.begin(),
		active_system_types.end(), type_id<Before>());

	auto *system = new T(std::forward<Args>(args)...);
	if (pos == active_system_types.end()) {
		return system;
	}
	size_t index = std::distance(pos, active_system_types.end()) - 1;
	active_systems.insert(active_systems.begin() + index, system);
	active_system_types.insert(pos, type_id<T>());
	return system;
}

template <class T>
T *SystemManager::get_system() {
	static_assert(std::is_convertible<T *, ISystem *>(),
				  "Type cannot be converted to a System");
	ASSERT(active_systems.size() == active_system_types.size());

	auto pos = std::find(active_system_types.begin(),
		active_system_types.end(), type_id<T>());

	if (pos == active_system_types.end()) {
		return nullptr;
	}
	auto index = std::distance(active_system_types.begin(), pos);
	return static_cast<T *>(active_systems[index]);
}

template <class T>
void SystemManager::get_all_systems(std::vector<T *> *systems) {
	static_assert(std::is_convertible<T *, ISystem *>(),
				  "Type cannot be converted to a System");
	ASSERT(active_systems.size() == active_system_types.size());
	ASSERT(systems != nullptr);

	for (size_t i = 0; i < active_systems.size(); ++i) {
		if (active_system_types[i] != type_id<T>()) {
			continue;
		}
		systems->push_back(static_cast<T *>(active_systems[i]));
	}
}

void SystemManager::update(float dt) {
	for (auto system : active_systems) {
		system->Update(world, dt);
	}
}

inline void SystemManager::destroy_system(ISystem *system) {
	ASSERT(active_systems.size() == active_system_types.size());
	ASSERT(system != nullptr);

	auto pos = std::find(active_systems.begin(), active_systems.end(), system);
	if (pos == active_systems.end()) {
		return;
	}
	system->Uninit(world);
	delete system;

	auto index = std::distance(active_systems.begin(), pos);
	active_systems.erase(pos);
	active_system_types.erase(active_system_types.begin() + index);
}

inline void SystemManager::destroy_systems() {
	for (auto *system : active_systems) {
		system->Uninit(world);
		delete system;
	}
	active_systems.clear();
	active_system_types.clear();
}

} // namespace ECS

template <typename Event>
void EventChannel<Event>::bind(EventHandler &&fn) {
	handlers.push_back(std::move(fn));
}

template <typename Event>
void EventChannel<Event>::emit(const Event &event) const {
	for (const auto &fn : handlers) {
		if (fn(event)) break;
	}
}

template <typename T>
const T &Optional<T>::value() const & {
	ASSERT(has_value);
	return val;
}

template <typename T>
T &Optional<T>::value() & {
	ASSERT(has_value);
	return val;
}

template <typename T>
const T &&Optional<T>::value() const && {
	ASSERT(has_value);
	return std::move(val);
}

template <typename T>
T &&Optional<T>::value() && {
	ASSERT(has_value);
	return std::move(val);
}

} // namespace Trinity

#undef ECS_TEMPLATE_FOLD
#undef ASSERT_ENTITY
#undef ASSERT_MSG
#undef ASSERT

/* clang-format on */
