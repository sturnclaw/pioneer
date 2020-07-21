
#pragma once

#include "BasicComponents.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

namespace PhysicsSpace {

using EntityId = uint32_t;

// FIXME: here for mocking purposes only
template<typename T>
struct typeId {
	static size_t type() {
		return 0;
	}
};

// FIXME: support cloning pools for ping-pong behavior, etc.
// Mostly here for mocking purposes
class Manager {
	struct PoolBase {
		virtual ~PoolBase();
	};

	template<typename C>
	struct Pool : public PoolBase {
		C *get(EntityId e);
	};

public:
	template<typename C>
	bool HasComponent(EntityId e) {
		return pool<C>()->get(e) != nullptr;
	}

	template<typename C>
	C &GetComponent(EntityId e) {
		return pool<C>()->get(e);
	}

	// FIXME: here to mock, actually implement with ability to poke into last frame's cached data
	template<typename C>
	const C &GetLast(EntityId e) {
		return pool<C>()->get(e);
	}

	Frame *GetFrame(EntityId f) {
		return pool<Frame>()->get(f);
	}

private:
	template<typename C>
	Pool<C> *pool() {
		auto &pb = poolMap[typeId<C>::type()];
		if (pb == nullptr)
			pb.reset(new Pool<C>());

		return reinterpret_cast<Pool<C> *>(pb.get());
	}

	std::map<size_t, std::unique_ptr<PoolBase> > poolMap;
};

struct IterState {
	Manager *manager;
	EntityId entity;
	double timeStep;
};

} // namespace PhyiscsSpace