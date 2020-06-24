/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_SKINS_H
#define GAME_CLIENT_COMPONENTS_SKINS_H
#include <base/vmath.h>
#include <base/color.h>
#include <base/tl/sorted_array.h>
#include <game/client/component.h>

class CSkins : public CComponent
{
public:
	// do this better and nicer
	struct CSkin
	{
		bool m_IsVanilla;
		IGraphics::CTextureHandle m_OrgTexture;
		IGraphics::CTextureHandle m_ColorTexture;
		char m_aName[24];
		ColorRGBA m_BloodColor;

		bool operator<(const CSkin &Other) { return str_comp(m_aName, Other.m_aName) < 0; }
	};

	void OnInit();

	int Num();
	const CSkin *Get(int Index);
	int Find(const char *pName) const;

private:
	sorted_array<CSkin> m_aSkins;
	char m_EventSkinPrefix[100];

	int FindImpl(const char *pName) const;
	static int SkinScan(const char *pName, int IsDir, int DirType, void *pUser);
};
#endif
