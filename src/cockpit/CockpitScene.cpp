// Copyright © 2008-2022 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "CockpitScene.h"

#include "Camera.h"
#include "JsonUtils.h"
#include "InteractionScene.h"
#include "Pi.h"
#include "Prop.h"
#include "Ship.h"
#include "ShipType.h"

#include "graphics/Drawables.h"
#include "graphics/Graphics.h"
#include "graphics/Types.h"
#include "lua/LuaObject.h"
#include "matrix3x3.h"
#include "profiler/Profiler.h"
#include "scenegraph/Model.h"

using namespace Cockpit;

PropDB *CockpitScene::m_propDB = nullptr;

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
	m_props.clear();
	m_interactionScene.reset();
}

void CockpitScene::InitForShipType(const ShipType *shipType)
{
	PROFILE_SCOPED()

	Clear();

	m_shipType = shipType;

	if (!m_propDB) {
		m_propDB = new PropDB();
		m_propDB->LoadPropCtx();
		m_propDB->LoadProps("cockpits/props/switches.json");
	}

	Json cockpitInfo = nullptr;
	std::string_view cockpitName = shipType->cockpitName;
	if (!cockpitName.empty()) {
		cockpitInfo = JsonUtils::LoadJsonDataFile(fmt::format("cockpits/{}/cockpit.json", cockpitName));
	}

	if (cockpitInfo.is_null()) {
		cockpitInfo = JsonUtils::LoadJsonDataFile("cockpits/default_cockpit/cockpit.json");
	}

	if (!cockpitInfo.is_object())
		return;

	Load(cockpitName, cockpitInfo);
}

void CockpitScene::Load(std::string_view cockpitPath, const Json &cockpitInfo)
{
	PROFILE_SCOPED()

	std::string cockpitModel = cockpitInfo["model"];
	SceneGraph::Model *model = Pi::FindModel(cockpitModel, false);
	if (!model)
		model = Pi::FindModel("default_cockpit");

	m_model.reset(model->MakeInstance());

	m_interactionScene.reset(new InteractionScene());

	/*
	m_interactionScene->AddSphereTrigger(0, vector3f(0.01, -0.01, -0.5), 0.1);
	m_interactionScene->AddBoxTrigger(1,
		vector3f(-0.05, 0.0, -0.5),
		matrix3x3f::RotateX(M_PI_4),
		vector3f(0.05, 0.05, 0.1));
	*/

	auto iter = cockpitInfo.find("props");
	if (iter != cockpitInfo.end() && iter->is_array())
		LoadProps(*iter);
}

void CockpitScene::Clear()
{
	// m_displayContexts.clear();
	m_props.clear();

	m_interactionScene.reset();
	m_model.reset();
}

void CockpitScene::LoadProps(const Json &node)
{
	PROFILE_SCOPED()

	for (const auto &entry : node) {
		std::string_view id = entry["id"];
		PropInfo *propInfo = m_propDB->GetProp(id);
		if (propInfo == nullptr) {
			Log::Warning("Could not find prop {}\n", id);
			continue;
		}

		vector3f position;
		matrix3x3f orient = matrix3x3fIdentity;

		const Json &posNode = entry["position"];
		if (posNode.is_array()) {
			position.x = posNode[0];
			position.y = posNode[1];
			position.z = posNode[2];
		}

		const Json &orientNode = entry["orient"];
		if (orientNode.is_array()) {
			orient[0] = orientNode[0];
			orient[1] = orientNode[1];
			orient[2] = orientNode[2];
			orient[3] = orientNode[3];
			orient[4] = orientNode[4];
			orient[5] = orientNode[5];
			orient[6] = orientNode[6];
			orient[7] = orientNode[7];
			orient[8] = orientNode[8];
		}

		Prop *prop = new Prop(propInfo, this, m_props.size(), m_propDB->GetEnvTable());

		prop->SetPosition(position);
		prop->SetOrient(orient);

		prop->UpdateTriggers();

		m_props.emplace_back(prop);
	}
}

void CockpitScene::SetShip(Ship *ship)
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

	// Set this ship in the cockpit environment table
	LuaRef &envTable = m_propDB->GetEnvTable();
	envTable.PushCopyToStack();
	LuaObject<Ship>::PushToLua(m_ship);
	lua_setfield(envTable.GetLua(), -2, "ship");
	lua_pop(envTable.GetLua(), 1);

	for (auto &prop : m_props) {
		prop->Update(Pi::GetFrameTime());
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

	uint32_t propId = id >> 8;
	uint32_t triggerId = id & 0xff;

	if (propId < m_props.size() && Pi::input->IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
		m_props[propId]->TriggerAction(triggerId);
	}
}

void CockpitScene::Render(Graphics::Renderer *r, Camera *camera, const matrix4x4f &viewTransform)
{
	PROFILE_SCOPED()

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

	// r->SetAmbientColor(Color4f(ambient, ambient, ambient));
	r->SetAmbientColor(Color4f(1, 1, 1));
	r->SetLightIntensity(intensities.size(), intensities.data());

	r->ClearDepthBuffer();

	m_model->Render(viewTransform);

	for (auto &prop : m_props) {
		prop->Render(r, viewTransform);
	}

	m_interactionScene->DrawDebug(r, m_debugMat.get(), viewTransform);

	// Draw debug indicator for line trace normal
	// matrix4x4f trans = matrix4x4f(m_camOrient, m_camPosition);
	// r->SetTransform(viewTransform * trans * matrix4x4f::Translation(m_lastTrace));
	// Graphics::Drawables::GetAxes3DDrawable(r)->Draw(r);

	r->SetAmbientColor(oldAmbient);
	r->SetLightIntensity(oldIntensity.size(), oldIntensity.data());
}
