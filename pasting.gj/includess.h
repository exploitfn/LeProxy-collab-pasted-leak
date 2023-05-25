#include <cstdint>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>

#include "C:\Users\niger\Desktop\paid\Framework\imgui.h"
#include "C:\Users\niger\Desktop\paid\Framework\imgui_internal.h"
#include <WinNls.h>

namespace custom_interface
{
	bool tab(const char* label, bool selected);
	bool subtab(const char* label, bool selected);
}

std::string string_To_UTF8(const std::string& str)
{
	int nwLen = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	ZeroMemory(pwBuf, nwLen * 2 + 2);

	::MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char* pBuf = new char[nLen + 1];
	ZeroMemory(pBuf, nLen + 1);

	::WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr(pBuf);

	delete[]pwBuf;
	delete[]pBuf;

	pwBuf = NULL;
	pBuf = NULL;

	return retStr;
}

void DrawFortniteText(ImFont* Font, ImVec2 pos, ImU32 color, const char* str)
{
	std::string utf_8_1 = std::string(str);
	std::string utf_8_2 = string_To_UTF8(utf_8_1);
	ImDrawList* Drawlist{ };
	Drawlist->AddText(Font, 18.0f, pos, ImGui::GetColorU32(color), utf_8_2.c_str());
}