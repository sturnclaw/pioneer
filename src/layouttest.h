
#pragma once

#include "core/GuiApplication.h"

class TestLifecycle;

class LayoutTestApp : public GuiApplication {
public:
	LayoutTestApp();

	void Startup() override;
	void Shutdown() override;

protected:
	void PreUpdate() override;
	void PostUpdate() override;

private:
	friend class TestLifecycle;
	std::shared_ptr<TestLifecycle> m_lifecycle;
};
