// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "CockpitScene.h"

#include "Camera.h"
#include "JsonUtils.h"
#include "InteractionScene.h"
#include "Pi.h"
#include "Ship.h"
#include "ShipType.h"

#include "graphics/Drawables.h"
#include "graphics/Graphics.h"
#include "graphics/Types.h"
#include "profiler/Profiler.h"
#include "scenegraph/Model.h"

using namespace Cockpit;

CockpitScene::CockpitScene(Graphics::Renderer *r) :
	m_camPosition(),
	m_camOrient(),
	m_ship(nullptr),
	m_shipType(nullptr),
	m_lastTrace()
{
	Graphics::RenderStateDesc rsd = {};
	rsd.primitiveType = Graphics::PrimitiveType::LINE_SINGLE;
	rsd.depthWrite = false;

	m_debugMat.reset(r->CreateMaterial("vtxColor", {}, rsd));
}

CockpitScene::~CockpitScene()
{

}

void CockpitScene::InitForShipType(const ShipType *shipType)
{
	Clear();

	std::string cockpitName;

	if (!shipType->cockpitName.empty()) {
		if (Pi::FindModel(shipType->cockpitName, false))
			cockpitName = shipType->cockpitName;
	}

	if (cockpitName.empty()) {
		if (Pi::FindModel("default_cockpit", false))
			cockpitName = shipType->cockpitName;
		else
			return;
	}

	Load(cockpitName);
}

void CockpitScene::Load(const std::string &cockpitPath)
{
	const Json &cockpitInfo = JsonUtils::LoadJsonDataFile("cockpits/" + cockpitPath + "/cockpit.json");

	m_model.reset(Pi::FindModel(cockpitPath)->MakeInstance());
	m_interactionScene.reset(new InteractionScene());
	m_interactionScene->AddSphereTrigger(0, vector3f(0.01, -0.01, -0.5), 0.1);

	auto iter = cockpitInfo.find("props");
	if (iter != cockpitInfo.end() && iter->is_array())
		LoadProps(*iter);
}

void CockpitScene::Clear()
{
	// m_displayContexts.clear();
	// m_props.clear();
	m_actionMap.clear();

	m_interactionScene.reset();
	m_model.reset();
}

void CockpitScene::LoadProps(const Json &node)
{
}

void CockpitScene::SetShip(const Ship *ship)
{
	m_ship = ship;
}

void CockpitScene::Update(matrix3x3d viewOrient, vector3d viewOffset)
{
	PROFILE_SCOPED()

	m_camOrient = matrix3x3f(viewOrient);
	m_camPosition = vector3f(viewOffset);

	if (m_model && m_ship) {
		Propulsion *prop = m_ship->GetComponent<Propulsion>();
		vector3f linthrust{ prop->GetLinThrusterState() };
		vector3f angthrust{ prop->GetAngThrusterState() };
		m_model->SetThrust(linthrust, -angthrust);
	}

	if (Pi::input->IsCapturingMouse())
		return;

	int mousePos[2];
	Pi::input->GetMousePosition(mousePos);

	vector2f pos = vector2f(
		float(mousePos[0]) / Graphics::GetScreenWidth(),
		float(mousePos[1]) / Graphics::GetScreenHeight());

	pos = pos * 2.0f - vector2f(1.0f);

	float aspectRatio = float(Graphics::GetScreenWidth()) / float(Graphics::GetScreenHeight());
	float screenHeightWorld = tan(DEG2RAD(Graphics::GetFov()) / 2.0);

	vector3f traceRay = vector3f(
		pos.x * screenHeightWorld * aspectRatio,
		-pos.y * screenHeightWorld,
		-1.0).Normalized();

	m_lastTrace = traceRay;

	size_t id = m_interactionScene->TraceRay(m_camPosition, m_camOrient * traceRay);
}

void CockpitScene::Render(Graphics::Renderer *r, Camera *camera, const matrix4x4f &viewTransform)
{
	if (!m_model || !m_ship)
		return;

	double ambient, direct;
	camera->CalcLighting(m_ship, ambient, direct);

	Color oldAmbient = r->GetAmbientColor();
	std::vector<float> oldIntensity;
	std::vector<float> intensities;
	for (int i = 0; i < camera->GetNumLightSources(); i++) {
		intensities.push_back(direct * camera->ShadowedIntensity(i, m_ship));
		oldIntensity.push_back(r->GetLight(i).GetIntensity());
	}

	r->SetAmbientColor(Color4f(ambient, ambient, ambient));
	r->SetLightIntensity(intensities.size(), intensities.data());

	r->ClearDepthBuffer();

	m_model->Render(viewTransform);

	// TODO: render props

	m_interactionScene->DrawDebug(r, m_debugMat.get(), viewTransform);

	// Draw debug indicator for line trace normal
	// matrix4x4f trans = matrix4x4f(m_camOrient, m_camPosition);
	// r->SetTransform(viewTransform * trans * matrix4x4f::Translation(m_lastTrace));
	// Graphics::Drawables::GetAxes3DDrawable(r)->Draw(r);

	r->SetAmbientColor(oldAmbient);
	r->SetLightIntensity(oldIntensity.size(), oldIntensity.data());
}
