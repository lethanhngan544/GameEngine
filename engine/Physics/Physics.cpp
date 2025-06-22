#include <Physics.h>

#include <imgui.h>

#include <Data.h>
#include <optional>

#include <imgui.h>

// Jolt includes

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

using namespace JPH;
using namespace JPH::literals;
using namespace std;

namespace eg::Physics
{
	class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
	{
	public:
		virtual bool					ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
		{
			switch (inObject1)
			{
			case Layers::NON_MOVING:
				return inObject2 == Layers::MOVING; // Non moving only collides with moving
			case Layers::MOVING:
				return true; // Moving collides with everything
			default:
				JPH_ASSERT(false);
				return false;
			}
		}
	};



	class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			// Create a mapping table from object to broad phase layer
			mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		virtual uint					GetNumBroadPhaseLayers() const final
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		virtual BroadPhaseLayer			GetBroadPhaseLayer(ObjectLayer inLayer) const final
		{
			return mObjectToBroadPhase[inLayer];
		}
	private:
		BroadPhaseLayer					mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
		{
			switch (inLayer1)
			{
			case Layers::NON_MOVING:
				return inLayer2 == BroadPhaseLayers::MOVING;
			case Layers::MOVING:
				return true;
			default:
				JPH_ASSERT(false);
				return false;
			}
		}
	};


	std::optional<TempAllocatorImpl> gTempAllocator;
	std::optional<JobSystemThreadPool> gJobSystem;
	std::optional<BPLayerInterfaceImpl> gBroadPhaseLayerInterface;
	std::optional< ObjectVsBroadPhaseLayerFilterImpl> gObjectVsBroadPhaseLayerFilter;
	std::optional< ObjectLayerPairFilterImpl> gObjectVsObjectLayerFilter;
	std::optional< PhysicsSystem> gPhysicsSystem;
	BodyInterface* gBodyInterface = nullptr;


	const uint cMaxBodies = 65536;
	const uint cNumBodyMutexes = 0;
	const uint cMaxBodyPairs = 65536;
	const uint cMaxContactConstraints = 10240;

	void create()
	{
		RegisterDefaultAllocator();
		Factory::sInstance = new Factory();
		RegisterTypes();

		gTempAllocator.emplace(10 * 1024 * 1024);
		gJobSystem.emplace(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
		gBroadPhaseLayerInterface.emplace();
		gObjectVsBroadPhaseLayerFilter.emplace();
		gObjectVsObjectLayerFilter.emplace();

		// Now we can create the actual physics system.
		gPhysicsSystem.emplace();
		
		gPhysicsSystem->Init(cMaxBodies,
			cNumBodyMutexes,
			cMaxBodyPairs,
			cMaxContactConstraints,
			gBroadPhaseLayerInterface.value(),
			gObjectVsBroadPhaseLayerFilter.value(),
			gObjectVsObjectLayerFilter.value());

		gBodyInterface = &gPhysicsSystem->GetBodyInterface();
	}



	void update(float delta)
	{
		gPhysicsSystem->Update(delta, 1, &gTempAllocator.value(), &gJobSystem.value());
	}
	void reset()
	{
		gPhysicsSystem.reset();
		gBodyInterface = nullptr;

		gPhysicsSystem.emplace();

		gPhysicsSystem->Init(cMaxBodies,
			cNumBodyMutexes,
			cMaxBodyPairs,
			cMaxContactConstraints,
			gBroadPhaseLayerInterface.value(),
			gObjectVsBroadPhaseLayerFilter.value(),
			gObjectVsObjectLayerFilter.value());

		gBodyInterface = &gPhysicsSystem->GetBodyInterface();
	}
	void destroy()
	{
		gPhysicsSystem.reset();
		gBodyInterface = nullptr;

		// Unregisters all types with the factory and cleans up the default material
		UnregisterTypes();

		// Destroy the factory
		delete Factory::sInstance;
		Factory::sInstance = nullptr;
	}

	JPH::PhysicsSystem& getPhysicsSystem()
	{
		return *gPhysicsSystem;
	}
	JPH::BodyInterface* getBodyInterface()
	{
		return gBodyInterface;
	}
}