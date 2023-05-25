#include "pasting.gj.h"
#include "skcrypt.h"
#include "d3d9_x.h"
#include "xor.hpp"
#include <dwmapi.h>
#include <vector>
#include <corecrt_math.h>
#include "offsets.h"
#include "includesfn.h"
#include "driver.h"
#include <xstring>
#include "encrypt.h"
#include "string.h"

int centerx;
int centery;

DWORD processID;
DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR PlayerState;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Persistentlevel;
Vector3 localactorpos;

ImFont* pArial = nullptr;
ImFont* pVerdana = nullptr;
ImFont* pRobot = nullptr;
ImFont* pSegoe = nullptr;

RECT GameRect = { NULL };
D3DPRESENT_PARAMETERS d3dpp;

static void xCreateWindow();
static void xInitD3d();
static void xMainLoop();
static void xShutdown();
static LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND Window = NULL;
IDirect3D9Ex* p_Object = NULL;
static LPDIRECT3DDEVICE9 D3dDevice = NULL;
static LPDIRECT3DVERTEXBUFFER9 TriBuf = NULL;

FTransform GetBoneIndex(DWORD_PTR mesh, int index)
{
	DWORD_PTR bonearray;
	bonearray = read<DWORD_PTR>(mesh + OFFSETS::BoneArray);

	if (bonearray == NULL)
	{
		bonearray = read<DWORD_PTR>(mesh + OFFSETS::BoneArray + 0x10);
	}
	return  read<FTransform>(bonearray + (index * 0x60));
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id)
{
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(mesh + OFFSETS::ComponetToWorld);

	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

D3DXMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

double __fastcall Atan2(double a1, double a2)
{
	double result; // xmm0_8

	result = 0.0;
	if (a2 != 0.0 || a1 != 0.0)
		return atan2(a1, a2);
	return result;
}
double __fastcall FMod(double a1, double a2)
{
	if (fabs(a2) > 0.00000001)
		return fmod(a1, a2);
	else
		return 0.0;
}

double ClampAxis(double Angle)
{
	// returns Angle in the range (-360,360)
	Angle = FMod(Angle, (double)360.0);

	if (Angle < (double)0.0)
	{
		// shift to [0,360) range
		Angle += (double)360.0;
	}

	return Angle;
}

double NormalizeAxis(double Angle)
{
	// returns Angle in the range [0,360)
	Angle = ClampAxis(Angle);

	if (Angle > (double)180.0)
	{
		// shift to (-180,180]
		Angle -= (double)360.0;
	}

	return Angle;
}

class FRotator
{
public:
	FRotator() : Pitch(0.f), Yaw(0.f), Roll(0.f)
	{

	}

	FRotator(double _Pitch, double _Yaw, double _Roll) : Pitch(_Pitch), Yaw(_Yaw), Roll(_Roll)
	{

	}
	~FRotator()
	{

	}

	double Pitch;
	double Yaw;
	double Roll;
	inline FRotator get() {
		return FRotator(Pitch, Yaw, Roll);
	}
	inline void set(double _Pitch, double _Yaw, double _Roll) {
		Pitch = _Pitch;
		Yaw = _Yaw;
		Roll = _Roll;
	}

	inline FRotator Clamp() {
		FRotator result = get();
		if (result.Pitch > 180)
			result.Pitch -= 360;
		else if (result.Pitch < -180)
			result.Pitch += 360;
		if (result.Yaw > 180)
			result.Yaw -= 360;
		else if (result.Yaw < -180)
			result.Yaw += 360;
		if (result.Pitch < -89)
			result.Pitch = -89;
		if (result.Pitch > 89)
			result.Pitch = 89;
		while (result.Yaw < -180.0f)
			result.Yaw += 360.0f;
		while (result.Yaw > 180.0f)
			result.Yaw -= 360.0f;

		result.Roll = 0;
		return result;

	}
	double Length() {
		return sqrt(Pitch * Pitch + Yaw * Yaw + Roll * Roll);
	}

	FRotator operator+(FRotator angB) { return FRotator(Pitch + angB.Pitch, Yaw + angB.Yaw, Roll + angB.Roll); }
	FRotator operator-(FRotator angB) { return FRotator(Pitch - angB.Pitch, Yaw - angB.Yaw, Roll - angB.Roll); }
	FRotator operator/(float flNum) { return FRotator(Pitch / flNum, Yaw / flNum, Roll / flNum); }
	FRotator operator*(float flNum) { return FRotator(Pitch * flNum, Yaw * flNum, Roll * flNum); }
	bool operator==(FRotator angB) { return Pitch == angB.Pitch && Yaw == angB.Yaw && Yaw == angB.Yaw; }
	bool operator!=(FRotator angB) { return Pitch != angB.Pitch || Yaw != angB.Yaw || Yaw != angB.Yaw; }

};


FRotator Rotator(FQuat* F)
{
	const double SingularityTest = F->z * F->x - F->w * F->y;
	const double YawY = 2.f * (F->w * F->z + F->x * F->y);
	const double YawX = (1.f - 2.f * ((F->y * F->y) + (F->z * F->z)));

	const double SINGULARITY_THRESHOLD = 0.4999995f;
	const double RAD_TO_DEG = 57.295776;
	double Pitch, Yaw, Roll;

	if (SingularityTest < -SINGULARITY_THRESHOLD)
	{
		Pitch = -90.f;
		Yaw = (Atan2(YawY, YawX) * RAD_TO_DEG);
		Roll = NormalizeAxis(-Yaw - (2.f * Atan2(F->x, F->w) * RAD_TO_DEG));
	}
	else if (SingularityTest > SINGULARITY_THRESHOLD)
	{
		Pitch = 90.f;
		Yaw = (Atan2(YawY, YawX) * RAD_TO_DEG);
		Roll = NormalizeAxis(Yaw - (2.f * Atan2(F->x, F->w) * RAD_TO_DEG));
	}
	else
	{
		Pitch = (asin(2.f * SingularityTest) * RAD_TO_DEG);
		Yaw = (Atan2(YawY, YawX) * RAD_TO_DEG);
		Roll = (Atan2(-2.f * (F->w * F->x + F->y * F->z), (1.f - 2.f * ((F->x * F->x) + (F->y * F->y)))) * RAD_TO_DEG);
	}

	FRotator RotatorFromQuat = FRotator{ Pitch, Yaw, Roll };
	return RotatorFromQuat;
}

struct CamewaDescwipsion
{
	float FieldOfView;
	Vector3 Rotation;
	Vector3 Location;
};

CamewaDescwipsion UndetectedCamera()
{
	CamewaDescwipsion VirtualCamera;
	__int64 ViewStates;
	__int64 ViewState;

	ViewStates = read<__int64>(Localplayer + 0xD0);
	ViewState = read<__int64>(ViewStates + 8);

	VirtualCamera.FieldOfView = read<float>(PlayerController + 0x38C) * 90.f;

	VirtualCamera.Rotation.x = read<double>(ViewState + 0x9C0);
	__int64 ViewportClient = read<__int64>(Localplayer + 0x78);
	if (!ViewportClient) return VirtualCamera;
	__int64 AudioDevice = read<__int64>(ViewportClient + 0x98);
	if (!AudioDevice) return VirtualCamera;
	__int64 FListener = read<__int64>(AudioDevice + 0x1E0);
	if (!FListener) return VirtualCamera;
	FQuat Listener = read<FQuat>(FListener);
	VirtualCamera.Rotation.y = Rotator(&Listener).Yaw;

	VirtualCamera.Location = read<Vector3>(read<uintptr_t>(Uworld + 0x110));
	return VirtualCamera;
}

Vector3 ProjectWorldToScreen(Vector3 WorldLocation)
{
	CamewaDescwipsion vCamera = UndetectedCamera();
	vCamera.Rotation.x = (asin(vCamera.Rotation.x)) * (180.0 / M_PI);

	D3DMATRIX tempMatrix = Matrix(vCamera.Rotation);

	Vector3 vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	Vector3 vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	Vector3 vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	Vector3 vDelta = WorldLocation - vCamera.Location;
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	return Vector3((Width / 2.0f) + vTransformed.x * (((Width / 2.0f) / tanf(vCamera.FieldOfView * (float)M_PI / 360.f))) / vTransformed.z, (Height / 2.0f) - vTransformed.y * (((Width / 2.0f) / tanf(vCamera.FieldOfView * (float)M_PI / 360.f))) / vTransformed.z, 0);
}

struct HandleDisposer
{
	using pointer = HANDLE;
	void operator()(HANDLE handle) const
	{
		if (handle != NULL || handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(handle);
		}
	}
};
using unique_handle = std::unique_ptr<HANDLE, HandleDisposer>;

static std::uint32_t _GetProcessId(std::string process_name) {
	PROCESSENTRY32 processentry;
	const unique_handle snapshot_handle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

	if (snapshot_handle.get() == INVALID_HANDLE_VALUE)
		return 0;

	processentry.dwSize = sizeof(MODULEENTRY32);

	while (Process32Next(snapshot_handle.get(), &processentry) == TRUE) {
		if (process_name.compare(processentry.szExeFile) == 0)
			return processentry.th32ProcessID;
	}
	return 0;
}

int get_fps()
{
	using namespace std::chrono;
	static int count = 0;
	static auto last = high_resolution_clock::now();
	auto now = high_resolution_clock::now();
	static int fps = 0;

	count++;

	if (duration_cast<milliseconds>(now - last).count() > 1000) {
		fps = count;
		count = 0;
		last = now;
	}

	return fps;
}


void Box2D(int X, int Y, int W, int H, const ImU32& color, int thickness) {
	float lineW = (W / 1);
	float lineH = (H / 1);

	//black outlines
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y), ImVec2(X, Y + lineH), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y), ImVec2(X + lineW, Y), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W - lineW, Y), ImVec2(X + W, Y), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W, Y), ImVec2(X + W, Y + lineH), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y + H - lineH), ImVec2(X, Y + H), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y + H), ImVec2(X + lineW, Y + H), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W - lineW, Y + H), ImVec2(X + W, Y + H), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W, Y + H - lineH), ImVec2(X + W, Y + H), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 3);

	//corners
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y), ImVec2(X, Y + lineH), ImGui::GetColorU32(color), 0.200);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y), ImVec2(X + lineW, Y), ImGui::GetColorU32(color), 0.200);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W - lineW, Y), ImVec2(X + W, Y), ImGui::GetColorU32(color), 0.200);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W, Y), ImVec2(X + W, Y + lineH), ImGui::GetColorU32(color), 0.200);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y + H - lineH), ImVec2(X, Y + H), ImGui::GetColorU32(color), 0.200);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X, Y + H), ImVec2(X + lineW, Y + H), ImGui::GetColorU32(color), 0.200);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W - lineW, Y + H), ImVec2(X + W, Y + H), ImGui::GetColorU32(color), 0.200);
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(X + W, Y + H - lineH), ImVec2(X + W, Y + H), ImGui::GetColorU32(color), 0.200);
}

DWORD_PTR baseaddy;

int main(int argc, const char* argv[])
{

	MouseController::Init();

	std::cout << "Waiting For FortniteClient-Win64-Shipping.exe";


	while (hwnd == NULL)
	{
		XorS(wind, "Fortnite  ");
		hwnd = FindWindowA(0, wind.decrypt());
	}

	system(_xor_("cls").c_str());

	if (driver->Init(FALSE))
	{
		processID = _GetProcessId("FortniteClient-Win64-Shipping.exe");
		driver->Attach(processID);
		baseaddy = driver->GetModuleBase(L"FortniteClient-Win64-Shipping.exe");
	}

	std::cout << processID << std::endl;
	std::cout << baseaddy << std::endl;
	std::cout << "Pasted by LeProxy" << std::endl;

	xCreateWindow();
	xInitD3d();

	xMainLoop();
	xShutdown();

	return 0;
}

void SetWindowToTarget()
{
	while (true)
	{
		if (hwnd)
		{
			ZeroMemory(&GameRect, sizeof(GameRect));
			GetWindowRect(hwnd, &GameRect);
			Width = GameRect.right - GameRect.left;
			Height = GameRect.bottom - GameRect.top;
			DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

			if (dwStyle & WS_BORDER)
			{
				GameRect.top += 32;
				Height -= 39;
			}


			MoveWindow(Window, GameRect.left, GameRect.top, Width, Height, true);
		}
		else
		{
			exit(0);
		}
	}
}

void xCreateWindow()
{
	const MARGINS Margin = { -1 };
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)SetWindowToTarget, 0, 0, 0);

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.lpszClassName = "notepad.exe";
	wc.lpfnWndProc = WinProc;
	RegisterClassEx(&wc);

	GetClientRect(hwnd, &GameRect);
	POINT xy;
	ClientToScreen(hwnd, &xy);
	GameRect.left = xy.x;
	GameRect.top = xy.y;

	Width = GameRect.right;
	Height = GameRect.bottom;

	Window = CreateWindowEx(NULL, "notepad.exe", "notepad", WS_POPUP | WS_VISIBLE, 0, 0, Width, Height, 0, 0, 0, 0);

	DwmExtendFrameIntoClientArea(Window, &Margin);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);
}

void xInitD3d()
{
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &p_Object)))
		exit(3);

	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.BackBufferWidth = Width;
	d3dpp.BackBufferHeight = Height;
	d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
	d3dpp.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.hDeviceWindow = Window;
	d3dpp.Windowed = TRUE;

	p_Object->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &D3dDevice);

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui_ImplWin32_Init(Window);
	ImGui_ImplDX9_Init(D3dDevice);

	io.FontDefault = pArial = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 24.f, nullptr, io.Fonts->GetGlyphRangesDefault());
	pVerdana = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 24.f, nullptr, io.Fonts->GetGlyphRangesDefault());
	//pRobot = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Fixedsys Regular.ttf", 24.f, nullptr, io.Fonts->GetGlyphRangesDefault());
	//pSegoe = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Segoe UI.ttf", 24.f, nullptr, io.Fonts->GetGlyphRangesDefault());

	p_Object->Release();
}

float RandomFloat(float a, float b)
{
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = b - a;
	float r = random * diff;
	return a + r;
}

void aimbot(float x, float y)
{
	float ScreenCenterX = centerx;
	float ScreenCenterY = centery;
	int AimSpeed = 5;
	float TargetX = 0;
	float TargetY = 0;

	if (x != 0)
	{
		if (x > ScreenCenterX)
		{
			TargetX = -(ScreenCenterX - x);
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
		}

		if (x < ScreenCenterX)
		{
			TargetX = x - ScreenCenterX;
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX < 0) TargetX = 0;
		}
	}

	if (y != 0)
	{
		if (y > ScreenCenterY)
		{
			TargetY = -(ScreenCenterY - y);
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
		}

		if (y < ScreenCenterY)
		{
			TargetY = y - ScreenCenterY;
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY < 0) TargetY = 0;
		}
	}

	float targetx_min = TargetX - 1;
	float targetx_max = TargetX + 1;

	float targety_min = TargetY - 1;
	float targety_max = TargetY + 1;

	float offset_x = RandomFloat(targetx_min, targetx_max);
	float offset_y = RandomFloat(targety_min, targety_max);

	mouse_event(MOUSEEVENTF_MOVE, static_cast<int>((float)offset_x), static_cast<int>((float)offset_y / 2), NULL, NULL);

}

bool isVisible(uint64_t mesh)
{
	float fLastSubmitTime = read<float>(mesh + 0x338);
	float fLastRenderTimeOnScreen = read<float>(mesh + 0x340);
	const float fVisionTick = 0.06f;
	bool LeProxy = fLastRenderTimeOnScreen + fVisionTick >= fLastSubmitTime;
	return LeProxy;
}

bool IsVec3Valid(Vector3 vec3)
{
	return (vec3.x == 0 && vec3.y == 0 && vec3.z == 0);
}

double GetDistance(double x1, double y1, double z1, double x2, double y2) {
	return sqrtf(powf((x2 - x1), 2) + powf((y2 - y1), 2));
}

bool IsInScreen(Vector3 pos, int over = 30) {
	if (pos.x > -over && pos.x < Width + over && pos.y > -over && pos.y < Height + over) {
		return true;
	}
	else {
		return false;
	}
}

void AimAt(DWORD_PTR entity)
{
	uint64_t mesh = read<uint64_t>(entity + OFFSETS::Mesh);
	Vector3 PlayerPosition = GetBoneWithRotation(mesh, 106);
	PlayerPosition = ProjectWorldToScreen(PlayerPosition);

	if (IsInScreen(PlayerPosition) && isVisible(mesh)) {
		if (!IsVec3Valid(PlayerPosition))
			aimbot(PlayerPosition.x, PlayerPosition.y);
	}
}

void DrawLine(double x1, double x2, double x3, double x4, float chubbyness)
{
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(x1, x2), ImVec2(x3, x4), ImColor(0, 0, 0, 255), 3.f);
}

void DrawSkeleton(DWORD_PTR mesh)
{
	Vector3 vHeadBoneOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 106));
	Vector3 vNeckOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 67));
	Vector3 vHipOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 2));
	Vector3 vUpperArmRightOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 101));
	Vector3 vElboRightOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 102));
	Vector3 vRightHandWristOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, /*58*/113));

	Vector3 vRightHipOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 78));
	Vector3 vRightKneeOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 79));
	Vector3 vRightAnkleOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 87));
	Vector3 vRightFootOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 83));

	Vector3 vUpperArmLeftOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 35));
	Vector3 vElboLeftOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 10));
	Vector3 vLeftHandWristOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, /*30*/114/*39*/));

	Vector3 vLeftHipOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 71));
	Vector3 vLeftKneeOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 72));
	Vector3 vLeftAnkleOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 86));
	Vector3 vLeftFootOut = ProjectWorldToScreen(GetBoneWithRotation(mesh, 76));


	DrawLine(vHeadBoneOut.x, vHeadBoneOut.y, vNeckOut.x, vNeckOut.y, 0.5f);
	DrawLine(vNeckOut.x, vNeckOut.y, vHipOut.x, vHipOut.y, 0.5f);
	DrawLine(vNeckOut.x, vNeckOut.y, vUpperArmRightOut.x, vUpperArmRightOut.y, 0.5f);
	DrawLine(vUpperArmRightOut.x, vUpperArmRightOut.y, vElboRightOut.x, vElboRightOut.y, 0.5f);
	DrawLine(vElboRightOut.x, vElboRightOut.y, vRightHandWristOut.x, vRightHandWristOut.y, 0.5f);
	DrawLine(vHipOut.x, vHipOut.y, vRightHipOut.x, vRightHipOut.y, 0.5f);
	DrawLine(vRightHipOut.x, vRightHipOut.y, vRightKneeOut.x, vRightKneeOut.y, 0.5f);
	DrawLine(vRightKneeOut.x, vRightKneeOut.y, vRightAnkleOut.x, vRightAnkleOut.y, 0.5f);
	DrawLine(vRightAnkleOut.x, vRightAnkleOut.y, vRightFootOut.x, vRightFootOut.y, 0.5f);
	DrawLine(vNeckOut.x, vNeckOut.y, vUpperArmLeftOut.x, vUpperArmLeftOut.y, 0.5f);
	DrawLine(vUpperArmLeftOut.x, vUpperArmLeftOut.y, vElboLeftOut.x, vElboLeftOut.y, 0.5f);
	DrawLine(vElboLeftOut.x, vElboLeftOut.y, vLeftHandWristOut.x, vLeftHandWristOut.y, 0.5f);
	DrawLine(vHipOut.x, vHipOut.y, vLeftHipOut.x, vLeftHipOut.y, 0.5f);
	DrawLine(vLeftHipOut.x, vLeftHipOut.y, vLeftKneeOut.x, vLeftKneeOut.y, 0.5f);
	DrawLine(vLeftKneeOut.x, vLeftKneeOut.y, vLeftAnkleOut.x, vLeftAnkleOut.y, 0.5f);
	DrawLine(vLeftAnkleOut.x, vLeftAnkleOut.y, vLeftFootOut.x, vLeftFootOut.y, 0.5f);
}

void draw_rect(int x, int y, int w, int h, ImColor color, int thickness) {
	ImGui::GetOverlayDrawList()->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 3.f, 15, thickness);
}

void draw_rect_filled(int x, int y, int w, int h, ImColor color, int thickness) {
	ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), color, 3.f, 15);
}

void box_esp(float OffsetW, float OffsetH, int X, int Y, int W, int H, ImU32 Color, int thickness, bool filled, bool outlined) {
	if (filled) {
		ImU32 sdfg = ImColor(0, 0, 0, 100);
		draw_rect_filled(X, Y, W, H, sdfg, 2.0f);
	}

	if (outlined) {
		draw_rect(X, Y, W, H, ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), 0.5f);
	}

	draw_rect(X, Y, W, H, Color, 2.0f);
}

std::string GetPlayerName(__int64 PlayerState)
{
	__int64 FString = read<__int64>(PlayerState + 0xac0);
	int Length = read<int>(FString + 16i64);
	__int64 v6 = Length; // rbx
	if (!v6) return std::string("");
	uintptr_t FText = read<__int64>(FString + 8);

	wchar_t* NameBuffer = new wchar_t[Length];
	readptr(FText, (uint8_t*)NameBuffer, (Length * 2));

	char v21; // al
	int v22; // r8d
	int i; // ecx
	int v25; // eax
	_WORD* v23;

	v21 = v6 - 1;
	if (!(_DWORD)v6)
		v21 = 0;
	v22 = 0;
	v23 = (_WORD*)NameBuffer;
	for (i = (v21) & 3; ; *v23++ += i & 7)
	{
		v25 = v6 - 1;
		if (!(_DWORD)v6)
			v25 = 0;
		if (v22 >= v25)
			break;
		i += 3;
		++v22;
	}

	std::wstring Temp{ NameBuffer };
	return std::string(Temp.begin(), Temp.end());
}

double DegreeToRadian(double degree) {
	return degree * (M_PI / 180);
}
void Angles(const Vector3& angles, Vector3* forward)
{
	float	sp, sy, cp, cy;

	sy = sin(DegreeToRadian(angles.y));
	cy = cos(DegreeToRadian(angles.y));

	sp = sin(DegreeToRadian(angles.x));
	cp = cos(DegreeToRadian(angles.x));

	forward->x = cp * cy;
	forward->y = cp * sy;
	forward->z = -sp;
}

void DrawESP() 
{

	CamewaDescwipsion vCamera = UndetectedCamera();
	ImGui::GetOverlayDrawList()->AddCircle(ImVec2(Width / 2, Height / 2), (50.f * centerx / vCamera.FieldOfView) / 3, IM_COL32(255, 255, 255, 255), 2000, 1.0f); // sexy fov

	float closestDistance = 1337;
	DWORD_PTR closestPawn = NULL;

	Uworld = read<DWORD_PTR>(baseaddy + OFFSET_UWORLD);
	DWORD_PTR Gameinstance = read<DWORD_PTR>(Uworld + OFFSETS::Gameinstance);
	DWORD_PTR LocalPlayers = read<DWORD_PTR>(Gameinstance + OFFSETS::LocalPlayers);
	Localplayer = read<DWORD_PTR>(LocalPlayers);
	PlayerController = read<DWORD_PTR>(Localplayer + OFFSETS::PlayerController);
	LocalPawn = read<DWORD_PTR>(PlayerController + OFFSETS::LocalPawn);

	PlayerState = read<DWORD_PTR>(LocalPawn + OFFSETS::PlayerState);
	Rootcomp = read<DWORD_PTR>(LocalPawn + OFFSETS::RootComponet);
	Persistentlevel = read<DWORD_PTR>(Uworld + OFFSETS::PersistentLevel);
	DWORD ActorCount = read<DWORD>(Persistentlevel + OFFSETS::ActorCount);
	DWORD_PTR AActors = read<DWORD_PTR>(Persistentlevel + OFFSETS::AActor);

	for (int i = 0; i < ActorCount; i++)
	{
		uintptr_t CurrentWeapon = read<uintptr_t>(LocalPawn + OFFSETS::CurrentWeapon);
		uint64_t CurrentActor = read<uint64_t>(AActors + i * OFFSETS::CurrentActor);
		uint64_t CurrentActorMesh = read<uint64_t>(CurrentActor + OFFSETS::Mesh);
		if (read<float>(CurrentActor + OFFSETS::Revivefromdbnotime) != 10) continue;//changes by 10 every updated and it is importen/

		if (CurrentActor == LocalPawn) continue;

		int MyTeamId = read<int>(PlayerState + OFFSETS::TeamId);
		DWORD64 otherPlayerState = read<uint64_t>(CurrentActor + OFFSETS::PlayerState);

		Vector3 Headbone = GetBoneWithRotation(CurrentActorMesh, 68);
		Headbone = ProjectWorldToScreen(Headbone);
		if (IsVec3Valid(Headbone)) continue;

		Vector3 Rootbone = GetBoneWithRotation(CurrentActorMesh, NULL);
		Rootbone = ProjectWorldToScreen(Rootbone);
		if (IsVec3Valid(Rootbone)) continue;

		Vector3 HeadboneV2 = GetBoneWithRotation(CurrentActorMesh, 106);
		HeadboneV2 = ProjectWorldToScreen(HeadboneV2);
		Vector3 Headbox = Vector3(HeadboneV2.x, HeadboneV2.y, HeadboneV2.z + 22);
		Vector3 Rootbox = Vector3(Rootbone.x, Rootbone.y, Rootbone.z + 5);

		localactorpos = read<Vector3>(Rootcomp + OFFSETS::LocalActorPos);
		float distance = vCamera.Location.Distance(Rootbone) / 100.f;

		//if (distance < 200.f)
		//{
		if (IsInScreen(Rootbone))
		{
			char dist[16]; // Character array
			sprintf_s(dist, "(%.fm)", distance);
			ImVec2 text_size = ImGui::CalcTextSize(dist);
			ImGui::GetOverlayDrawList()->AddText(pVerdana, 19.f, ImVec2(Headbox.x - (text_size.x / 2), Headbox.y - 30), ImColor(255, 255, 255, 255), dist);

			//DWORD_PTR test_platform = read<DWORD_PTR>(otherPlayerState + 0x430);
			//wchar_t platform[64];
			//driver->ReadProcessMemory(test_platform, platform, sizeof(test_platform));
			//std::wstring balls_sus(platform);
			//std::string str_platform(balls_sus.begin(), balls_sus.end());
			//ImVec2 TextSize = ImGui::CalcTextSize(str_platform.c_str());

			//if (strstr(str_platform.c_str(), ("WIN")))
			//{
			//	ImGui::GetOverlayDrawList()->AddText(pArial, 18.f, ImVec2(Rootbone.x - (text_size.x / 2), Rootbone.y), ImColor(255, 255, 255, 255), "PC");

			//}
			//else if (strstr(str_platform.c_str(), ("XBL")) || strstr(str_platform.c_str(), ("XSX")))
			//{
			//	ImGui::GetOverlayDrawList()->AddText(pArial, 18.f, ImVec2(Rootbone.x - (text_size.x / 2), Rootbone.y), ImColor(255, 255, 255, 255), "Xbox");

			//}
			//else if (strstr(str_platform.c_str(), ("PSN")) || strstr(str_platform.c_str(), ("PS5")))
			//{
			//	ImGui::GetOverlayDrawList()->AddText(pArial, 18.f, ImVec2(Rootbone.x - (text_size.x / 2), Rootbone.y), ImColor(255, 255, 255, 255), "PSN");

			//}
			//else if (strstr(str_platform.c_str(), ("SWT")))
			//{
			//	ImGui::GetOverlayDrawList()->AddText(pArial, 18.f, ImVec2(Rootbone.x - (text_size.x / 2), Rootbone.y), ImColor(255, 255, 255, 255), "Nintendo");
			//}
			//else
			//{
			//	ImGui::GetOverlayDrawList()->AddText(pArial, 18.f, ImVec2(Rootbone.x - (text_size.x / 2), Rootbone.y), ImColor(255, 255, 255, 255), str_platform.c_str());
			//}

			if (isVisible(CurrentActorMesh)) {
				float BoxHeight = Rootbone.y - Headbox.y;
				float BoxWidth = BoxHeight / 1.9f;
				box_esp(4, 4, Rootbox.x - (BoxWidth / 2), Headbox.y,
					BoxWidth, BoxHeight, ImColor(0, 255, 0, 255), 1.f,
					true, false);
			}
			else {
				float BoxHeight = Rootbone.y - Headbox.y;
				float BoxWidth = BoxHeight / 1.9f;
				box_esp(4, 4, Rootbox.x - (BoxWidth / 2), Headbox.y,
					BoxWidth, BoxHeight, ImColor(255, 0, 0, 255), 1.f,
					true, false);
			}

			//ImGui::GetOverlayDrawList()->AddCircle(ImVec2(Headbone.x, Headbone.y), 100.f * distance, ImColor(255, 0, 0, 255), 2000, 0.5f);
			ImGui::GetOverlayDrawList()->AddLine(ImVec2(Width / 2, Height), ImVec2(Rootbone.x, Rootbone.y), ImColor(255, 0, 255, 255), 0.5);
			DrawSkeleton(CurrentActorMesh);
		}
		/*}*/

		auto dx = Headbone.x - centerx;
		auto dy = Headbone.y - centery;
		auto dist = sqrtf(dx * dx + dy * dy);
		if (dist <= 200.f) {
			closestDistance = dist;
			closestPawn = CurrentActor;
		}
	}

	if (GetAsyncKeyState(VK_RBUTTON) && closestPawn != 1337 && LocalPawn)
		AimAt(closestPawn);

}


void render() 
{
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	std::string Text = "FPS: ";
	ImGui::GetOverlayDrawList()->AddText(pArial, 40.f, ImVec2(20, 20), ImColor(255, 255, 255, 255), Text.c_str());

	DrawESP();

	ImGui::EndFrame();
	D3dDevice->SetRenderState(D3DRS_ZENABLE, false);
	D3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	D3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	D3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	if (D3dDevice->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		D3dDevice->EndScene();
	}
	HRESULT result = D3dDevice->Present(NULL, NULL, NULL, NULL);

	if (result == D3DERR_DEVICELOST && D3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		D3dDevice->Reset(&d3dpp);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}

void xMainLoop()
{
	static RECT old_rc;
	MSG Message = { NULL };
	ZeroMemory(&Message, sizeof(MSG));

	while (Message.message != WM_QUIT)
	{
		if (PeekMessage(&Message, Window, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}

		HWND hwnd_active = GetForegroundWindow();

		if (hwnd_active == hwnd) {
			HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
			SetWindowPos(Window, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		RECT rc;
		POINT xy;

		ZeroMemory(&rc, sizeof(RECT));
		ZeroMemory(&xy, sizeof(POINT));
		GetClientRect(hwnd, &rc);
		ClientToScreen(hwnd, &xy);
		rc.left = xy.x;
		rc.top = xy.y;

		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = NULL;
		io.ImeWindowHandle = hwnd;
		io.DeltaTime = 1.0f / 60.f;
		centerx = io.DisplaySize.x / 2;
		centery = io.DisplaySize.y / 2;

		POINT p;
		GetCursorPos(&p);
		io.MousePos.x = p.x - xy.x;
		io.MousePos.y = p.y - xy.y;

		if (GetAsyncKeyState(VK_LBUTTON)) {
			io.MouseDown[0] = true;
			io.MouseClicked[0] = true;
			io.MouseClickedPos[0].x = io.MousePos.x;
			io.MouseClickedPos[0].x = io.MousePos.y;
		}
		else
			io.MouseDown[0] = false;

		render();
	}

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	DestroyWindow(Window);
}

LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
		return true;

	switch (Message)
	{
	case WM_DESTROY:
		xShutdown();
		PostQuitMessage(0);
		exit(4);
		break;
	case WM_SIZE:
		if (D3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			d3dpp.BackBufferWidth = LOWORD(lParam);
			d3dpp.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = D3dDevice->Reset(&d3dpp);
			if (hr == D3DERR_INVALIDCALL)
				IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
		break;
	default:
		return DefWindowProc(hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}

void xShutdown()
{
	TriBuf->Release();
	D3dDevice->Release();
	p_Object->Release();

	DestroyWindow(Window);
	UnregisterClass("notepad.exe", NULL);
}
