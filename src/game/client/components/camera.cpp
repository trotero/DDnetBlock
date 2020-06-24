/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/string.h>

#include <engine/shared/config.h>

#include <base/math.h>
#include <game/collision.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>

#include "camera.h"
#include "controls.h"

#include <engine/serverbrowser.h>

CCamera::CCamera()
{
	m_CamType = CAMTYPE_UNDEFINED;
	m_ZoomSet = false;
	m_Zoom = 1.0f;
}

void CCamera::OnRender()
{
	if(!(m_pClient->m_Snap.m_SpecInfo.m_Active || GameClient()->m_GameInfo.m_AllowZoom || Client()->State() == IClient::STATE_DEMOPLAYBACK))
	{
		m_ZoomSet = false;
		m_Zoom = 1.0f;
	}
	else if(!m_ZoomSet && g_Config.m_ClDefaultZoom != 10)
	{
		m_ZoomSet = true;
		OnReset();
	}

	// update camera center
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		if(m_CamType != CAMTYPE_SPEC)
		{
			m_LastPos[g_Config.m_ClDummy] = m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy];
			m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy] = m_PrevCenter;
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_SPEC;
		}
		m_Center = m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy];
	}
	else
	{
		if(m_CamType != CAMTYPE_PLAYER)
		{
			if((m_LastPos[g_Config.m_ClDummy].x < g_Config.m_ClMouseMinDistance) || (m_LastPos[g_Config.m_ClDummy].x < g_Config.m_ClDyncamMinDistance))
				m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy].x = m_LastPos[g_Config.m_ClDummy].x + g_Config.m_ClMouseMinDistance + g_Config.m_ClDyncamMinDistance;
			else
				m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy] = m_LastPos[g_Config.m_ClDummy];
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_PLAYER;
		}

		vec2 CameraOffset(0, 0);

		float l = length(m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy]);
		if(l > 0.0001f) // make sure that this isn't 0
		{
			float DeadZone = g_Config.m_ClDyncam ? g_Config.m_ClDyncamDeadzone : g_Config.m_ClMouseDeadzone;
			float FollowFactor = (g_Config.m_ClDyncam ? g_Config.m_ClDyncamFollowFactor : g_Config.m_ClMouseFollowfactor) / 100.0f;
			float OffsetAmount = maximum(l-DeadZone, 0.0f) * FollowFactor;

			CameraOffset = normalize(m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy])*OffsetAmount;
		}

		if(m_pClient->m_Snap.m_SpecInfo.m_Active)
			m_Center = m_pClient->m_Snap.m_SpecInfo.m_Position + CameraOffset;
		else
			m_Center = m_pClient->m_LocalCharacterPos + CameraOffset;
	}

	m_PrevCenter = m_Center;
}

void CCamera::OnConsoleInit()
{
	Console()->Register("zoom+", "", CFGFLAG_CLIENT, ConZoomPlus, this, "Zoom increase");
	Console()->Register("zoom-", "", CFGFLAG_CLIENT, ConZoomMinus, this, "Zoom decrease");
	Console()->Register("zoom", "", CFGFLAG_CLIENT, ConZoomReset, this, "Zoom reset");
}

const float ZoomStep = 0.866025f;

void CCamera::OnReset()
{
	m_Zoom = 1.0f;

	if(g_Config.m_ClDefaultZoom < 10)
	{
		m_Zoom = pow(1/ZoomStep, 10 - g_Config.m_ClDefaultZoom);
	}
	else if(g_Config.m_ClDefaultZoom > 10)
	{
		m_Zoom = pow(ZoomStep, g_Config.m_ClDefaultZoom - 10);
	}

}

void CCamera::ConZoomPlus(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active || pSelf->GameClient()->m_GameInfo.m_AllowZoom || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
		((CCamera *)pUserData)->m_Zoom *= ZoomStep;
}
void CCamera::ConZoomMinus(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active || pSelf->GameClient()->m_GameInfo.m_AllowZoom || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		if(((CCamera *)pUserData)->m_Zoom < 500.0f/ZoomStep)
		{
			((CCamera *)pUserData)->m_Zoom *= 1/ZoomStep;
		}
	}
}
void CCamera::ConZoomReset(IConsole::IResult *pResult, void *pUserData)
{
	((CCamera *)pUserData)->OnReset();
}
