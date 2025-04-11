#include <Physics.h>

#include <imgui.h>

#include <Data.h>
#include <optional>

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


	class DebugRenderer : public JPH::DebugRendererSimple
	{
	public:
		virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
		{
			Data::DebugRenderer::recordLine(glm::vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()),
				glm::vec3(inTo.GetX(), inTo.GetY(), inTo.GetZ()),
				glm::vec4(inColor.r, inColor.g, inColor.b, inColor.a));
		}
		virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override
		{

		}

	};

	std::optional<TempAllocatorImpl> gTempAllocator;
	std::optional<JobSystemThreadPool> gJobSystem;
	std::optional<BPLayerInterfaceImpl> gBroadPhaseLayerInterface;
	std::optional< ObjectVsBroadPhaseLayerFilterImpl> gObjectVsBroadPhaseLayerFilter;
	std::optional< ObjectLayerPairFilterImpl> gObjectVsObjectLayerFilter;
	std::optional< PhysicsSystem> gPhysicsSystem;
	BodyInterface* gBodyInterface = nullptr;

	BodyID gFloorBody;
	std::optional<DebugRenderer> gDebugRenderer;

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

		BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));
		floor_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed when its reference count goes to 0.
		BodyCreationSettings floor_settings(&floor_shape_settings, RVec3(0.0_r, -1.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);

		// Create the actual rigid body
		gFloorBody = gBodyInterface->CreateAndAddBody(floor_settings, EActivation::DontActivate); // Note that if we run out of bodies this can return nullptr
		
		gDebugRenderer.emplace();


	}

	void render()
	{
		static JPH::BodyManager::DrawSettings settings;
		//use imgui to controll
		ImGui::Begin("Physics");
		ImGui::Checkbox("Draw Velocity", &settings.mDrawVelocity);
		ImGui::Checkbox("Draw Bounding Box", &settings.mDrawBoundingBox);
		ImGui::Checkbox("Draw Shape", &settings.mDrawShape);
		
		ImGui::End();
		
		gPhysicsSystem->DrawBodies(settings, &gDebugRenderer.value(), nullptr);
	}

	void update(float delta)
	{

		// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
		const int cCollisionSteps = 1;
		// Step the world
		gPhysicsSystem->Update(delta, cCollisionSteps, &gTempAllocator.value(), &gJobSystem.value());
	}

	void destroy()
	{
		// Remove and destroy the floor
		gBodyInterface->RemoveBody(gFloorBody);
		gBodyInterface->DestroyBody(gFloorBody);

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