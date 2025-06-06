// Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _GASGIANT_H
#define _GASGIANT_H

#include "BaseSphere.h"
#include "GasGiantJobs.h"
#include "JobQueue.h"
#include "vector3.h"

#include <deque>

namespace Graphics {
	class Renderer;
	class RenderTarget;
	class Texture;
} // namespace Graphics

class SystemBody;
class GasGiant;
class GasPatch;
class GasPatchContext;
class Camera;

namespace {
	class STextureFaceResult;
	class SGPUGenResult;
} // namespace

#define NUM_PATCHES 6

class GasGiant : public BaseSphere {
public:
	GasGiant(const SystemBody *body);
	virtual ~GasGiant();

	void Update() override;
	void Render(Graphics::Renderer *renderer, const matrix4x4d &modelView, vector3d campos, const float radius, const std::vector<Camera::Shadow> &shadows) override;

	double GetHeight(const vector3d &p) const final { return 0.0; }

	// in sbody radii
	double GetMaxFeatureHeight() const override { return 0.0; }

	void Reset() override;

	static bool OnAddTextureFaceResult(const SystemPath &path, GasGiantJobs::STextureFaceResult *res);
	static bool OnAddGPUGenResult(const SystemPath &path, GasGiantJobs::SGPUGenResult *res);
	static void InitGasGiant();
	static void UninitGasGiant();
	static void UpdateAllGasGiants();
	static void OnChangeGasGiantsDetailLevel();

	static void CreateRenderTarget(const Uint16 width, const Uint16 height);
	static void SetRenderTargetCubemap(const Uint32, Graphics::Texture *, const bool unBind = true);
	static Graphics::RenderTarget *GetRenderTarget();

private:
	void BuildFirstPatches();
	void GenerateTexture();
	bool AddTextureFaceResult(GasGiantJobs::STextureFaceResult *res);
	bool AddGPUGenResult(GasGiantJobs::SGPUGenResult *res);

	static RefCountedPtr<GasPatchContext> s_patchContext;

	static Graphics::RenderTarget *s_renderTarget;

	std::unique_ptr<GasPatch> m_patches[NUM_PATCHES];

	bool m_hasTempCampos;
	vector3d m_tempCampos;

	void SetUpMaterials() override;
	RefCountedPtr<Graphics::Texture> m_surfaceTextureSmall;
	RefCountedPtr<Graphics::Texture> m_surfaceTexture;
	RefCountedPtr<Graphics::Texture> m_builtTexture;

	std::unique_ptr<Color[]> m_jobColorBuffers[NUM_PATCHES];
	Job::Handle m_job[NUM_PATCHES];
	bool m_hasJobRequest[NUM_PATCHES];

	Job::Handle m_gpuJob;
	bool m_hasGpuJobRequest;

	float m_timeDelay;
};

#endif /* _GASGIANT_H */
