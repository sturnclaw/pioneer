// Copyright © 2013-14 Meteoric Games Ltd
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ShipCockpit.h"

#include "Easing.h"
#include "Game.h"
#include "Pi.h"
#include "Player.h"
#include "Space.h"
#include "WorldView.h"
#include "graphics/Renderer.h"
#include "graphics/Texture.h"
#include "imgui/examples/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui_internal.h"
#include "ship/CameraController.h"

ShipCockpit::ShipCockpit(const std::string &modelName) :
	m_shipDir(0.0),
	m_shipYaw(0.0),
	m_dir(0.0),
	m_yaw(0.0),
	m_rotInterp(0.f),
	m_transInterp(0.f),
	m_gForce(0.f),
	m_offset(0.f),
	m_shipVel(0.f),
	m_translate(0.0),
	m_transform(matrix4x4d::Identity())
{
	assert(!modelName.empty());
	SetModel(modelName.c_str());
	assert(GetModel());
	SetColliding(false);
	m_icc = nullptr;

	m_drawList.reset(new ImDrawList(ImGui::GetDrawListSharedData()));
}

ShipCockpit::~ShipCockpit()
{
}

void ShipCockpit::Render(Graphics::Renderer *renderer, const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform)
{
	PROFILE_SCOPED()
	RenderModel(renderer, camera, viewCoords, viewTransform);
}

inline void ShipCockpit::resetInternalCameraController()
{
	m_icc = static_cast<InternalCameraController *>(Pi::game->GetWorldView()->shipView->GetCameraController());
}

void ShipCockpit::Update(const Player *player, float timeStep)
{
	m_transform = matrix4x4d::Identity();

	if (m_icc == nullptr) {
		// I don't know where to put this
		resetInternalCameraController();
	}

	double rotX;
	double rotY;
	m_icc->getRots(rotX, rotY);
	m_transform.RotateX(rotX);
	m_transform.RotateY(rotY);

	vector3d cur_dir = player->GetOrient().VectorZ().Normalized();
	if (cur_dir.Dot(m_shipDir) < 1.0f) {
		m_rotInterp = 0.0f;
		m_shipDir = cur_dir;
	}

	//---------------------------------------- Acceleration
	float cur_vel = CalculateSignedForwardVelocity(-cur_dir, player->GetVelocity()); // Forward is -Z
	float gforce = Clamp(floorf(((fabs(cur_vel) - m_shipVel) / timeStep) / 9.8f), -COCKPIT_MAX_GFORCE, COCKPIT_MAX_GFORCE);
	if (fabs(cur_vel) > 500000.0f ||	   // Limit gforce measurement so we don't get astronomical fluctuations
		fabs(gforce - m_gForce) > 100.0) { // Smooth out gforce one frame spikes, sometimes happens when hitting max speed due to the thrust limiters
		gforce = 0.0f;
	}
	if (fabs(gforce - m_gForce) > 100.0) {
		gforce = 0.0f;
	}
	if (fabs(m_translate.z - m_offset) < 0.001f) {
		m_transInterp = 0.0f;
	}
	float offset = (gforce > 14.0f ? -1.0f : (gforce < -14.0f ? 1.0f : 0.0f)) * COCKPIT_ACCEL_OFFSET;
	m_transInterp += timeStep * COCKPIT_ACCEL_INTERP_MULTIPLIER;
	if (m_transInterp > 1.0f) {
		m_transInterp = 1.0f;
		m_translate.z = offset;
	}
	m_translate.z = Easing::Quad::EaseIn(double(m_transInterp), m_translate.z, offset - m_translate.z, 1.0);
	m_gForce = gforce;
	m_offset = offset;
	m_shipVel = cur_vel;

	//------------------------------------------ Rotation
	// For yaw/pitch
	vector3d rot_axis = cur_dir.Cross(m_dir).Normalized();
	vector3d yaw_axis = player->GetOrient().VectorY().Normalized();
	vector3d pitch_axis = player->GetOrient().VectorX().Normalized();
	float dot = cur_dir.Dot(m_dir);
	float angle = acos(dot);
	// For roll
	if (yaw_axis.Dot(m_shipYaw) < 1.0f) {
		m_rotInterp = 0.0f;
		m_shipYaw = yaw_axis;
	}
	vector3d rot_yaw_axis = yaw_axis.Cross(m_yaw).Normalized();
	float dot_yaw = yaw_axis.Dot(m_yaw);
	float angle_yaw = acos(dot_yaw);

	if (dot < 1.0f || dot_yaw < 1.0f) {
		// Lag/Recovery interpolation
		m_rotInterp += timeStep * COCKPIT_ROTATION_INTERP_MULTIPLIER;
		if (m_rotInterp > 1.0f) {
			m_rotInterp = 1.0f;
		}

		// Yaw and Pitch
		if (dot < 1.0f) {
			if (angle > DEG2RAD(COCKPIT_LAG_MAX_ANGLE)) {
				angle = DEG2RAD(COCKPIT_LAG_MAX_ANGLE);
			}
			angle = Easing::Quad::EaseOut(m_rotInterp, angle, -angle, 1.0f);
			m_dir = cur_dir;
			if (angle >= 0.0f) {
				m_dir.ArbRotate(rot_axis, angle);
				// Apply pitch
				vector3d yz_proj = (m_dir - (m_dir.Dot(pitch_axis) * pitch_axis)).Normalized();
				float pitch_cos = yz_proj.Dot(cur_dir);
				float pitch_angle = 0.0f;
				if (pitch_cos < 1.0f) {
					pitch_angle = acos(pitch_cos);
					if (rot_axis.Dot(pitch_axis) < 0) {
						pitch_angle = -pitch_angle;
					}
					m_transform.RotateX(-pitch_angle);
				}
				// Apply yaw
				vector3d xz_proj = (m_dir - (m_dir.Dot(yaw_axis) * yaw_axis)).Normalized();
				float yaw_cos = xz_proj.Dot(cur_dir);
				float yaw_angle = 0.0f;
				if (yaw_cos < 1.0f) {
					yaw_angle = acos(yaw_cos);
					if (rot_axis.Dot(yaw_axis) < 0) {
						yaw_angle = -yaw_angle;
					}
					m_transform.RotateY(-yaw_angle);
				}
			} else {
				angle = 0.0f;
			}
		}

		// Roll
		if (dot_yaw < 1.0f) {
			if (angle_yaw > DEG2RAD(COCKPIT_LAG_MAX_ANGLE)) {
				angle_yaw = DEG2RAD(COCKPIT_LAG_MAX_ANGLE);
			}
			if (dot_yaw < 1.0f) {
				angle_yaw = Easing::Quad::EaseOut(m_rotInterp, angle_yaw, -angle_yaw, 1.0f);
			}
			m_yaw = yaw_axis;
			if (angle_yaw >= 0.0f) {
				m_yaw.ArbRotate(rot_yaw_axis, angle_yaw);
				// Apply roll
				vector3d xy_proj = (m_yaw - (m_yaw.Dot(cur_dir) * cur_dir)).Normalized();
				float roll_cos = xy_proj.Dot(yaw_axis);
				float roll_angle = 0.0f;
				if (roll_cos < 1.0f) {
					roll_angle = acos(roll_cos);
					if (rot_yaw_axis.Dot(cur_dir) < 0) {
						roll_angle = -roll_angle;
					}
					m_transform.RotateZ(-roll_angle);
				}
			} else {
				angle_yaw = 0.0f;
			}
		}
	} else {
		m_rotInterp = 0.0f;
	}

	// setup thruster levels
	if (GetModel()) {
		Propulsion *prop = player->GetComponent<Propulsion>();
		vector3f linthrust{ prop->GetLinThrusterState() };
		vector3f angthrust{ prop->GetAngThrusterState() };
		GetModel()->SetThrust(linthrust, -angthrust);
	}
}

#define RT_SIZE 300
#define SCAN_RANGE 40000
void ShipCockpit::RenderCockpit(Graphics::Renderer *renderer, const Camera *camera, FrameId frameId)
{
	PROFILE_SCOPED()
	if (!m_screenRT) {
		Graphics::RenderTargetDesc rtDesc(RT_SIZE, RT_SIZE,
			Graphics::TextureFormat::TEXTURE_RGBA_8888,
			Graphics::TextureFormat::TEXTURE_NONE);
		m_screenRT.reset(renderer->CreateRenderTarget(rtDesc));
		auto *mat = GetModel()->GetMaterialByName("screen_scanner").Get();

		mat->SetTexture(Graphics::Renderer::GetName("texture0"),
			m_screenRT->GetColorTexture());
		mat->SetTexture(Graphics::Renderer::GetName("texture2"),
			m_screenRT->GetColorTexture());
	}

	m_drawList->_ResetForNewFrame();
	m_drawList->PushClipRectFullScreen();
	ImFont *font = Pi::pigui->GetFont("orbiteer", 12);
	if (!font) font = ImGui::GetFont();

	m_drawList->PushTextureID(font->ContainerAtlas->TexID);

	ImVec2 p1 = { 10, ceil(RT_SIZE * 0.13f) };
	ImVec2 p2 = { RT_SIZE - 10, RT_SIZE - 10 };
	ImVec2 quart = { (p2.x - p1.x) / 4.0f, (p2.y - p1.y) / 4.0f };
	ImVec2 xwidth = { p2.x - p1.x, 0 };
	ImVec2 yheight = { 0, p2.y - p1.y };

	ImVec2 x31 = { p1.x, p1.y + quart.y * 1.f };
	ImVec2 x32 = { p1.x, p1.y + quart.y * 2.f };
	ImVec2 x33 = { p1.x, p1.y + quart.y * 3.f };
	ImVec2 y31 = { p1.x + quart.x * 1.f, p1.y };
	ImVec2 y32 = { p1.x + quart.x * 2.f, p1.y };
	ImVec2 y33 = { p1.x + quart.x * 3.f, p1.y };

	constexpr ImU32 lineCol = IM_COL32(170, 180, 240, 255);
	constexpr ImU32 scanCol = IM_COL32(240, 220, 180, 255);
	constexpr ImU32 contactCol = IM_COL32(240, 180, 160, 255);

	m_drawList->AddRect(p1, p2, lineCol, 8.0f, ImDrawCornerFlags_All, 5.0f);
	m_drawList->AddLine(x31, x31 + xwidth, lineCol, 3);
	m_drawList->AddLine(x32, x32 + xwidth, lineCol, 3);
	m_drawList->AddLine(x33, x33 + xwidth, lineCol, 3);
	m_drawList->AddLine(y31, y31 + yheight, lineCol, 3);
	m_drawList->AddLine(y32, y32 + yheight, lineCol, 3);
	m_drawList->AddLine(y33, y33 + yheight, lineCol, 3);

	Space::BodyNearList nearby = Pi::game->GetSpace()->GetBodiesMaybeNear(Pi::player, SCAN_RANGE);

	for (auto *body : nearby) {
		if (!body->IsType(ObjectType::SHIP))
			continue;

		auto pos = body->GetPositionRelTo(Pi::player);
		if (pos.Dot(-Pi::player->GetOrient().VectorZ()) <= 0 || pos.LengthSqr() > SCAN_RANGE * SCAN_RANGE)
			continue;

		auto frameUp = Pi::player->GetPosition().NormalizedSafe();
		auto playerDir = -Pi::player->GetOrient().VectorZ();
		playerDir = (playerDir - playerDir.Dot(frameUp) * frameUp).NormalizedSafe();
		auto orient = matrix3x3d::FromVectors(playerDir.Cross(frameUp), frameUp, -playerDir);

		pos = pos * orient;
		float w = Clamp(pos.x * 2.0 / SCAN_RANGE, -1.0, 1.0);
		float h = Clamp(pos.z / SCAN_RANGE, -1.0, 0.0);

		ImVec2 selfPos{ p1.x + xwidth.x * 0.5f, p2.y };
		ImVec2 contactPos = selfPos + ImVec2{ xwidth.x * w * 0.5f, h * yheight.y };

		std::string test = fmt::format("{}", int(ceil(pos.y / 1000)));
		m_drawList->AddCircle(contactPos, 4, contactCol, 12, 2);
		m_drawList->AddText(font, font->FontSize, contactPos + ImVec2{ 0.f, -18.f }, contactCol, test.c_str());
		if (body == Pi::player->GetCombatTarget())
			m_drawList->AddCircle(contactPos, 2, IM_COL32(200, 100, 100, 255), 12, 3);
	}

	float interp = abs(sin(Pi::game->GetTime()));
	ImVec2 startPos = p1 + yheight * 0.05f + xwidth * 0.5f;
	ImVec2 interpWidth{ xwidth.x * 0.5f * interp - 10.f, 0.f };
	m_drawList->AddLine(startPos - interpWidth, startPos + interpWidth, scanCol, 10.0f);

	Pi::pigui->RenderToTexture(m_screenRT.get(), { m_drawList.get() });

	renderer->ClearDepthBuffer();

	Body::SetFrame(frameId);
	Render(renderer, camera, m_translate, m_transform);
	Body::SetFrame(FrameId::Invalid);
}

void ShipCockpit::OnActivated(const Player *player)
{
	assert(player);
	m_dir = player->GetOrient().VectorZ().Normalized();
	m_yaw = player->GetOrient().VectorY().Normalized();
	m_shipDir = m_dir;
	m_shipYaw = m_yaw;
	m_shipVel = CalculateSignedForwardVelocity(-m_shipDir, player->GetVelocity());
}

float ShipCockpit::CalculateSignedForwardVelocity(const vector3d &normalized_forward, const vector3d &velocity)
{
	float velz_cos = velocity.Dot(normalized_forward);
	return (velz_cos * normalized_forward).Length() * (velz_cos < 0.0 ? -1.0 : 1.0);
}
