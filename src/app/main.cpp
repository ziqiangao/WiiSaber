#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ogc/conf.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>

#include "WireApplication.h"
#include "WireCuller.h"
#include "WireCamera.h"
#include "WireRenderer.h"
#include "WireNode.h"
class PhysicsWorld;
#include "Importer.h"
#include "AnimationCallbacks.h"
#include "DefaultEnvironment.h"
#include "BeatmapLights.h"
#include "scene.h"

using namespace Wire;

class WiiSaber : public WIREAPPLICATION {
	WIRE_DECLARE_INITIALIZE;
	typedef WIREAPPLICATION Parent;

public:
	WiiSaber()
		: WIREAPPLICATION(ColorRGBA(0.015F, 0.02F, 0.06F, 1.0F),
			"WiiSaber", 0, 0, 640, 480, true, true), mpAnimationCallbacks(NULL) {}

	virtual Bool OnInitialize();
	virtual void OnIdle();
	virtual void OnTerminate();

private:
	NodePtr mspScene;
	CameraPtr mspCamera;
	Culler mCuller;
	Double mStartTime;
	Double mLastCameraUpdate;
	static const Float kBeatmapBpm;
	static const Char *kBeatmapPath;
	BeatScene::DefaultEnvironmentAnimationCallbacks *mpAnimationCallbacks;
};

WIRE_APPLICATION(WiiSaber);
WIRE_REGISTER_INITIALIZE(WiiSaber);

const Float WiiSaber::kBeatmapBpm = 166.0F;
const Char *WiiSaber::kBeatmapPath = "sd:/apps/wiisaber/maps/AngelVoices/ExpertStandard.dat";

static Float GetDisplayAspectRatio() {
	return CONF_GetAspectRatio() == CONF_ASPECT_16_9
		? (16.0F / 9.0F) : (4.0F / 3.0F);
}

Bool WiiSaber::OnInitialize() {
	// Send stdout/stderr (including the startup diagnostics) to Dolphin's
	// OSReport log so printf output is visible while running under Dolphin.
	SYS_STDIO_Report(true);
	if (!Parent::OnInitialize()) return false;

	const Char *environmentDirectory = "sd:/apps/wiisaber/environments/DefaultEnvironment/";
	GetRenderer()->SetClearColor(ColorRGBA(0.015F, 0.02F, 0.06F, 1.0F));
	Importer importer(environmentDirectory);
	FILE *sceneFile = fopen("sd:/apps/wiisaber/environments/DefaultEnvironment/DefaultEnvironment.xml", "rb");
	if (sceneFile) {
		fclose(sceneFile);
		mspScene = importer.LoadSceneFromXml("DefaultEnvironment.xml");
		printf("[WiiSaber] scene import: %s\n", mspScene ? "succeeded" : "failed");
	} else printf("[WiiSaber] default environment scene missing\n");
	if (!mspScene) {
		printf("[WiiSaber] using built-in demo scene\n");
		mspScene = BeatScene::CreateDefaultEnvironment();
	}
	mpAnimationCallbacks = new BeatScene::DefaultEnvironmentAnimationCallbacks;
	mpAnimationCallbacks->Bind(mspScene, GetRenderer());
	mpAnimationCallbacks->Reset();
	BeatScene::LoadBeatmapCallbacks(kBeatmapPath,
		*mpAnimationCallbacks,
		BeatScene::DefaultEnvironmentAnimationCallbacks::PrimaryColor(),
		BeatScene::DefaultEnvironmentAnimationCallbacks::SecondaryColor(),
		BeatScene::DefaultEnvironmentAnimationCallbacks::WhiteColor(),
		BeatScene::DefaultEnvironmentAnimationCallbacks::BoostPrimaryColor(),
		BeatScene::DefaultEnvironmentAnimationCallbacks::BoostSecondaryColor());
	BeatScene::StartBeatmapCallbacks();

	mspScene->UpdateRS();
	mspScene->Bind(GetRenderer());
	mspScene->WarmUpRendering(GetRenderer());

	Vector3F location(0.0F, 2.5F, 0.0F);
	Vector3F direction(0.0F, 0.0F, 1.0F);
	direction.Normalize();
	Vector3F up(0.0F, 1.0F, 0.0F);
	mspCamera = WIRE_NEW Camera(location, direction, up);
	const Float displayAspect = GetDisplayAspectRatio();
	printf("[WiiSaber] display aspect compensation: %.3f\n", displayAspect);
	mspCamera->SetFrustum(55.0F, displayAspect, 0.1F,
		1000.0F);
	mCuller.SetCamera(mspCamera);
	mStartTime = System::GetTime();
	mLastCameraUpdate = mStartTime;
	return true;
}

void WiiSaber::OnIdle() {
	const Double time = System::GetTime();
	Float deltaTime = static_cast<Float>(time - mLastCameraUpdate);
	if (deltaTime > 0.1F) deltaTime = 0.1F;
	mLastCameraUpdate = time;

	// D-pad moves relative to the camera; 1/2 turn; A/B rise and descend.
	const u32 buttons = WPAD_ButtonsHeld(WPAD_CHAN_0);
	const Float moveSpeed = 5.0F * deltaTime;
	const Float turnSpeed = 1.8F * deltaTime;
	Vector3F location = mspCamera->GetLocation();
	Vector3F direction = mspCamera->GetDVector();
	Vector3F up = mspCamera->GetUVector();
	Vector3F right = mspCamera->GetRVector();

	if (buttons & WPAD_BUTTON_UP) location += direction * moveSpeed;
	if (buttons & WPAD_BUTTON_DOWN) location -= direction * moveSpeed;
	if (buttons & WPAD_BUTTON_RIGHT) location += right * moveSpeed;
	if (buttons & WPAD_BUTTON_LEFT) location -= right * moveSpeed;
	if (buttons & WPAD_BUTTON_A) location += Vector3F::UNIT_Y * moveSpeed;
	if (buttons & WPAD_BUTTON_B) location -= Vector3F::UNIT_Y * moveSpeed;

	Float turn = 0.0F;
	if (buttons & WPAD_BUTTON_2) turn += turnSpeed;
	if (buttons & WPAD_BUTTON_1) turn -= turnSpeed;
	if (turn != 0.0F) {
		const Float cosine = cosf(turn);
		const Float sine = sinf(turn);
		const Float directionX = direction.X();
		const Float directionZ = direction.Z();
		const Float rightX = right.X();
		const Float rightZ = right.Z();
		direction.X() = cosine * directionX + sine * directionZ;
		direction.Z() = -sine * directionX + cosine * directionZ;
		right.X() = cosine * rightX + sine * rightZ;
		right.Z() = -sine * rightX + cosine * rightZ;
		mspCamera->SetFrame(location, direction, up, right);
	} else {
		mspCamera->SetLocation(location);
	}

	const Float elapsed = static_cast<Float>(time - mStartTime);
	BeatScene::UpdateEnvironment(mspScene, elapsed);
	BeatScene::UpdateBeatmapCallbacks(*mpAnimationCallbacks, elapsed, kBeatmapBpm);
	mpAnimationCallbacks->AnimationUpdate();
	mspScene->UpdateGS(time);
	mCuller.ComputeVisibleSet(mspScene);

	GetRenderer()->ClearBuffers();
	GetRenderer()->PreDraw(mspCamera);
	GetRenderer()->Draw(mCuller.GetVisibleSets());
	GetRenderer()->PostDraw();
	GetRenderer()->DisplayBackBuffer();
}

void WiiSaber::OnTerminate() {
	if (mspScene) mspScene->Unbind(GetRenderer());
	delete mpAnimationCallbacks;
	mpAnimationCallbacks = NULL;
	mspCamera = NULL;
	mspScene = NULL;
}
