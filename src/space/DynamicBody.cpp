
#include "space/DynamicBody.h"
#include "core/Delegate.h"
#include "gameconsts.h"
#include "space/BasicComponents.h"
#include "space/Manager.h"
#include "vector3.h"

using namespace PhysicsSpace;

struct LastFrameCache {
	ForceCache cache;
};

double CalcAtmosphericDrag(const PlanetData &pd, vector3d pos, double velSqr, double area, double coeff)
{
	double pressure, density;
	GetAtmosphericState(pd, pos.Length(), &pressure, &density);

	// Simplified calculation of atmospheric drag/lift force.
	return 0.5 * density * velSqr * area * coeff;
}

class PhysicsSpace::ClearForces : public ISystem {
	void Update(World *world, float dt) override
	{
		world->view<ForceCache>().each([=](Entity ent, ForceCache &fc) {
			world->pack<LastFrameCache>(ent, { fc });

			fc.force = vector3d(0.0);
			fc.torque = vector3d(0.0);
			fc.externalForce = vector3d(0.0);
			fc.atmoForce = vector3d(0.0);
		});
	}
};

class PhysicsSpace::CalcDynamicForce : public ISystem {
public:
	void Update(World *w, float dt) override
	{
		world = w;
		world->view<Transform, DynamicBody, BasicDragData, ForceCache>()
			.each(make_delegate(&CalcDynamicForce::CalcAtmosphericForce, *this));

		world->view<Transform, DynamicBody, ForceCache>()
			.each(make_delegate(&CalcDynamicForce::CalcExternalForce, *this));
	}

private:
	void CalcAtmosphericForce(const Transform &tr, const DynamicBody &b, const BasicDragData &d, ForceCache &fc)
	{
		const Frame *f = &world->unpack<Frame>(tr.frame);
		vector3d atmos_force = vector3d(0.0);

		if (f->IsRotFrame() && world->contains<PlanetData>(f->bodyId)) {
			vector3d dragDir = -b.velocity.NormalizedSafe();
			const PlanetData &pd = world->unpack<PlanetData>(f->bodyId);
			// We assume the object is a perfect sphere in the size of the clip radius.
			// Most things are /not/ using the default DynamicBody code, but this is still better than before.
			atmos_force = CalcAtmosphericDrag(pd, tr.position, b.velocity.LengthSqr(), d.area, d.dragCoeff) * dragDir;
		}

		fc.atmoForce = atmos_force;
	}

	void CalcExternalForce(const Transform &tr, const DynamicBody &b, ForceCache &fc)
	{
		// gravity
		const Frame *f = &world->unpack<Frame>(tr.frame);
		if (!f) return; // no external force if not in a frame

		if (f->bodyId) {
			const DynamicBody &ob = world->unpack<DynamicBody>(f->bodyId);
			vector3d b1b2 = tr.position;
			double m1m2 = b.mass * ob.mass;
			double invrsqr = 1.0 / b1b2.LengthSqr();
			double force = G * m1m2 * invrsqr;
			fc.externalForce += -b1b2 * sqrt(invrsqr) * force;
		}

		if (f->IsRotFrame()) {
			vector3d angRot(0, f->GetAngSpeed(), 0);
			fc.externalForce -= b.mass * angRot.Cross(angRot.Cross(tr.position)); // centrifugal
			fc.externalForce -= 2 * b.mass * angRot.Cross(b.velocity);			  // coriolis
		}
	}

	World *world;
};

class PhysicsSpace::DynamicUpdateSystem : public ISystem {
public:
	void Update(World *w, float dt) override
	{
		world = w;
		timeStep = dt;
		world->view<Transform, DynamicBody, ForceCache>().each(make_delegate(&DynamicUpdateSystem::UpdateDynamic, *this));
	}

private:
	void UpdateDynamic(Entity ent, Transform &tr, DynamicBody &b, ForceCache &fc)
	{
		// atmospheric drag
		if (fc.atmoForce.LengthSqr() > 0.0001) {
			// make this a bit less daft at high time accel
			// only allow atmoForce to increase by .1g per frame
			// TODO: clamp fc.atmoForce instead.
			const ForceCache &oc = world->unpack<LastFrameCache>(ent).cache;
			vector3d f1g = oc.atmoForce + fc.atmoForce.NormalizedSafe() * b.mass;
			fc.externalForce += (fc.atmoForce.LengthSqr() > f1g.LengthSqr()) ? f1g : fc.atmoForce;
		}

		fc.force += fc.externalForce;

		b.velocity += timeStep * fc.force * (1.0 / b.mass);
		b.angVelocity += timeStep * fc.torque * (1.0 / b.angInertia);

		double len = b.angVelocity.Length();
		if (len > 1e-16) {
			vector3d axis = b.angVelocity * (1.0 / len);
			matrix3x3d r = matrix3x3d::Rotate(len * timeStep, axis);
			tr.orient = r * tr.orient;
		}

		tr.position += b.velocity * timeStep;
		// no need to store last values in this struct, ClearForces saves the copy of last frame's values.
	}

	World *world;
	float timeStep;
};
