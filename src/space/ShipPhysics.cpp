
#include "space/ShipPhysics.h"
#include "core/Delegate.h"
#include "space/BasicComponents.h"
#include "space/DynamicBody.h"
#include "space/Manager.h"

using namespace PhysicsSpace;

static const double DEFAULT_DRAG_COEFF = 0.1; // 'smooth sphere'

double CalcAtmosphericDrag(const PlanetData &pd, vector3d pos, double velSqr, double area, double coeff)
{
	double pressure, density;
	GetAtmosphericState(pd, pos.Length(), &pressure, &density);

	// Simplified calculation of atmospheric drag/lift force.
	return 0.5 * density * velSqr * area * coeff;
}

template <typename T>
inline int sign(T num)
{
	return (num >= 0) - (num < 0);
}

class PhysicsSpace::ShipAtmoForceSystem : public ISystem {
public:
	static ShipAtmoForceSystem *Create(Trinity::ECS::SystemManager *manager)
	{
		return manager->make_system_before<DynamicUpdateSystem, ShipAtmoForceSystem>();
	}

	void Update(World *w, float dt) override
	{
		world = w;
		world->view<Transform, DynamicBody, ShipDragData, ForceCache, Trinity::ECS::ActiveTag>()
			.each(make_delegate(&ShipAtmoForceSystem::CalcAtmosphericForce, *this));
	}

private:
	World *world;
	double invTimeAccelRate;

	void CalcAtmosphericForce(const Transform &tr, const DynamicBody &b, const ShipDragData &d, ForceCache &fc)
	{
		const Frame *f = &world->unpack<Frame>(tr.frame);

		if (!f->IsRotFrame() || !world->contains<PlanetData>(f->bodyId)) {
			fc.atmoForce = vector3d(0.0);
			return;
		}

		// TODO: vary drag coefficient based on Reynolds number, specifically by
		// atmospheric composition (viscosity) and airspeed (mach number).
		const PlanetData &pd = world->unpack<PlanetData>(f->bodyId);

		// By converting the velocity into local space, we can apply the drag individually to each component.
		vector3d m_localVel = b.velocity * tr.orient;
		double m_lVSqr = m_localVel.LengthSqr();

		// The drag forces applied to the craft, in local space.
		// TODO: verify dimensional accuracy and that we're not generating more drag than physically possible.
		// TODO: use a different drag constant for each side (front, back, etc).
		// This also handles (most of) the lift due to wing deflection.
		vector3d fAtmosDrag = vector3d(
			CalcAtmosphericDrag(pd, tr.position, m_lVSqr, d.crossSection.x, d.dragCoeff.x),
			CalcAtmosphericDrag(pd, tr.position, m_lVSqr, d.crossSection.y, d.dragCoeff.y),
			CalcAtmosphericDrag(pd, tr.position, m_lVSqr, d.crossSection.z, d.dragCoeff.z));

		// The direction vector of the velocity also serves to scale and sign the generated drag.
		fAtmosDrag = fAtmosDrag * -m_localVel.NormalizedSafe();

		// The amount of lift produced by air pressure differential across the top and bottom of the lifting surfaces.
		vector3d fAtmosLift = vector3d(0.0);

		double m_AoAMultiplier = m_localVel.NormalizedSafe().y;

		// There's no lift produced once the wing hits the stall angle.
		if (std::abs(m_AoAMultiplier) < 0.61) {
			// Pioneer simulates non-cambered wings, with equal air displacement on either side of AoA.

			// Generated lift peaks at around 20 degrees here, and falls off fully at 35-ish.
			// TODO: handle AoA better / more gracefully with an actual angle- and curve-based implementation.
			m_AoAMultiplier = cos((std::abs(m_AoAMultiplier) - 0.31) * 5.0) * sign(m_AoAMultiplier);

			// TODO: verify dimensional accuracy and that we're not generating more lift than physically possible.
			// We scale down the lift contribution because fAtmosDrag handles deflection-based lift.
			fAtmosLift.y = CalcAtmosphericDrag(pd, tr.position, pow(m_localVel.z, 2), d.crossSection.y, d.liftCoeff) * -m_AoAMultiplier * 0.2;
		}

		fc.atmoForce = tr.orient * (fAtmosDrag + fAtmosLift);

		//calculates torque to force the spacecraft go nose-first in atmosphere
		vector3d m_vel = b.velocity.NormalizedSafe();
		vector3d m_torqueDir = -m_vel.Cross(-tr.orient.VectorZ()); // <--- This is correct

		if (b.velocity.Length() > 100) { //don't apply torque at minimal speeds
			// TODO: evaluate this function and properly implement based upon ship cross-section.
			double m_drag = CalcAtmosphericDrag(pd, tr.position, b.velocity.LengthSqr(), d.crossSection.y, DEFAULT_DRAG_COEFF);
			fc.torque += m_drag * m_torqueDir * ((d.crossSection.y + d.crossSection.x) / (d.crossSection.z * 4)) * 0.3 * d.atmoStability * invTimeAccelRate;
		}
	}
};

/*
void Ship::TimeStepUpdate(const float timeStep)
{
	// If docked, station is responsible for updating position/orient of ship
	// but we call this crap anyway and hope it doesn't do anything bad

	const vector3d thrust = GetPropulsion()->GetActualLinThrust();
	AddRelForce(thrust);
	AddRelTorque(GetPropulsion()->GetActualAngThrust());

	//apply extra atmospheric flight forces
	AddTorque(CalcAtmoTorque());

	if (m_landingGearAnimation)
		m_landingGearAnimation->SetProgress(m_wheelState);
	m_dragCoeff = DynamicBody::DEFAULT_DRAG_COEFF * (1.0 + 0.25 * m_wheelState);
	DynamicBody::TimeStepUpdate(timeStep);

	// fuel use decreases mass, so do this as the last thing in the frame
	UpdateFuel(timeStep);

	m_navLights->SetEnabled(m_wheelState > 0.01f);
	m_navLights->Update(timeStep);
	if (m_sensors.get()) m_sensors->Update(timeStep);
}
*/
