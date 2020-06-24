/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <engine/graphics.h>
#include <engine/demo.h>

#include <game/generated/client_data.h>
#include <game/client/render.h>
#include <game/gamecore.h>
#include "particles.h"

CParticles::CParticles()
{
	OnReset();
	m_RenderTrail.m_pParts = this;
	m_RenderExplosions.m_pParts = this;
	m_RenderGeneral.m_pParts = this;
}


void CParticles::OnReset()
{
	// reset particles
	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		m_aParticles[i].m_PrevPart = i-1;
		m_aParticles[i].m_NextPart = i+1;
	}

	m_aParticles[0].m_PrevPart = 0;
	m_aParticles[MAX_PARTICLES-1].m_NextPart = -1;
	m_FirstFree = 0;

	for(int i = 0; i < NUM_GROUPS; i++)
		m_aFirstPart[i] = -1;
}

void CParticles::Add(int Group, CParticle *pPart, float TimePassed)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		if(pInfo->m_Paused)
			return;
	}
	else
	{
		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED)
			return;
	}

	if(m_FirstFree == -1)
		return;

	// remove from the free list
	int Id = m_FirstFree;
	m_FirstFree = m_aParticles[Id].m_NextPart;
	if(m_FirstFree != -1)
		m_aParticles[m_FirstFree].m_PrevPart = -1;

	// copy data
	m_aParticles[Id] = *pPart;

	// insert to the group list
	m_aParticles[Id].m_PrevPart = -1;
	m_aParticles[Id].m_NextPart = m_aFirstPart[Group];
	if(m_aFirstPart[Group] != -1)
		m_aParticles[m_aFirstPart[Group]].m_PrevPart = Id;
	m_aFirstPart[Group] = Id;

	// set some parameters
	m_aParticles[Id].m_Life = TimePassed;
}

void CParticles::Update(float TimePassed)
{
	if(TimePassed <= 0.0f)
		return;

	static float FrictionFraction = 0;
	FrictionFraction += TimePassed;

	if(FrictionFraction > 2.0f) // safety messure
		FrictionFraction = 0;

	int FrictionCount = 0;
	while(FrictionFraction > 0.05f)
	{
		FrictionCount++;
		FrictionFraction -= 0.05f;
	}

	for(int g = 0; g < NUM_GROUPS; g++)
	{
		int i = m_aFirstPart[g];
		while(i != -1)
		{
			int Next = m_aParticles[i].m_NextPart;
			//m_aParticles[i].vel += flow_get(m_aParticles[i].pos)*time_passed * m_aParticles[i].flow_affected;
			m_aParticles[i].m_Vel.y += m_aParticles[i].m_Gravity*TimePassed;

			for(int f = 0; f < FrictionCount; f++) // apply friction
				m_aParticles[i].m_Vel *= m_aParticles[i].m_Friction;

			// move the point
			vec2 Vel = m_aParticles[i].m_Vel*TimePassed;
			Collision()->MovePoint(&m_aParticles[i].m_Pos, &Vel, 0.1f+0.9f*frandom(), NULL);
			m_aParticles[i].m_Vel = Vel* (1.0f/TimePassed);

			m_aParticles[i].m_Life += TimePassed;
			m_aParticles[i].m_Rot += TimePassed * m_aParticles[i].m_Rotspeed;

			// check particle death
			if(m_aParticles[i].m_Life > m_aParticles[i].m_LifeSpan)
			{
				// remove it from the group list
				if(m_aParticles[i].m_PrevPart != -1)
					m_aParticles[m_aParticles[i].m_PrevPart].m_NextPart = m_aParticles[i].m_NextPart;
				else
					m_aFirstPart[g] = m_aParticles[i].m_NextPart;

				if(m_aParticles[i].m_NextPart != -1)
					m_aParticles[m_aParticles[i].m_NextPart].m_PrevPart = m_aParticles[i].m_PrevPart;

				// insert to the free list
				if(m_FirstFree != -1)
					m_aParticles[m_FirstFree].m_PrevPart = i;
				m_aParticles[i].m_PrevPart = -1;
				m_aParticles[i].m_NextPart = m_FirstFree;
				m_FirstFree = i;
			}

			i = Next;
		}
	}
}

void CParticles::OnRender()
{
	if(Client()->State() < IClient::STATE_ONLINE)
		return;

	set_new_tick();
	static int64 LastTime = 0;
	int64 t = time();

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		if(!pInfo->m_Paused)
			Update((float)((t-LastTime)/(double)time_freq())*pInfo->m_Speed);
	}
	else
	{
		if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED))
			Update((float)((t-LastTime)/(double)time_freq()));
	}

	LastTime = t;
}

void CParticles::OnInit()
{
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	m_ParticleQuadContainerIndex = Graphics()->CreateQuadContainer();

	for(int i = 0; i <= (SPRITE_PART9 - SPRITE_PART_SLICE); ++i)
	{
		RenderTools()->SelectSprite(i);
		RenderTools()->QuadContainerAddSprite(m_ParticleQuadContainerIndex, 1.f, false);
	}
}

void CParticles::RenderGroup(int Group)
{
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_PARTICLES].m_Id);
	
	// don't use the buffer methods here, else the old renderer gets many draw calls
	if(Graphics()->IsBufferingEnabled())
	{
		int i = m_aFirstPart[Group];

		static IGraphics::SRenderSpriteInfo s_aParticleRenderInfo[MAX_PARTICLES];

		int CurParticleRenderCount = 0;

		// batching makes sense for stuff like ninja particles
		float LastColor[4];
		int LastQuadOffset = 0;

		if(i != -1)
		{
			LastColor[0] = m_aParticles[i].m_Color.r;
			LastColor[1] = m_aParticles[i].m_Color.g;
			LastColor[2] = m_aParticles[i].m_Color.b;
			LastColor[3] = m_aParticles[i].m_Color.a;

			Graphics()->SetColor(
				m_aParticles[i].m_Color.r,
				m_aParticles[i].m_Color.g,
				m_aParticles[i].m_Color.b,
				m_aParticles[i].m_Color.a);

			LastQuadOffset = m_aParticles[i].m_Spr;
		}
		
		while(i != -1)
		{
			int QuadOffset = m_aParticles[i].m_Spr;
			float a = m_aParticles[i].m_Life / m_aParticles[i].m_LifeSpan;
			vec2 p = m_aParticles[i].m_Pos;
			float Size = mix(m_aParticles[i].m_StartSize, m_aParticles[i].m_EndSize, a);
			
			if(LastColor[0] != m_aParticles[i].m_Color.r || LastColor[1] != m_aParticles[i].m_Color.g || LastColor[2] != m_aParticles[i].m_Color.b || LastColor[3] != m_aParticles[i].m_Color.a || LastQuadOffset != QuadOffset)
			{
				Graphics()->RenderQuadContainerAsSpriteMultiple(m_ParticleQuadContainerIndex, LastQuadOffset, CurParticleRenderCount, s_aParticleRenderInfo);
				CurParticleRenderCount = 0;
				LastQuadOffset = QuadOffset;

				Graphics()->SetColor(
					m_aParticles[i].m_Color.r,
					m_aParticles[i].m_Color.g,
					m_aParticles[i].m_Color.b,
					m_aParticles[i].m_Color.a);

				LastColor[0] = m_aParticles[i].m_Color.r;
				LastColor[1] = m_aParticles[i].m_Color.g;
				LastColor[2] = m_aParticles[i].m_Color.b;
				LastColor[3] = m_aParticles[i].m_Color.a;
			}

			s_aParticleRenderInfo[CurParticleRenderCount].m_Pos[0] = p.x;
			s_aParticleRenderInfo[CurParticleRenderCount].m_Pos[1] = p.y;

			s_aParticleRenderInfo[CurParticleRenderCount].m_Scale = Size;
			s_aParticleRenderInfo[CurParticleRenderCount].m_Rotation = m_aParticles[i].m_Rot;
			
			++CurParticleRenderCount;			

			i = m_aParticles[i].m_NextPart;
		}

		Graphics()->RenderQuadContainerAsSpriteMultiple(m_ParticleQuadContainerIndex, LastQuadOffset, CurParticleRenderCount, s_aParticleRenderInfo);

	}
	else
	{
		int i = m_aFirstPart[Group];

		Graphics()->BlendNormal();
		//gfx_blend_additive();
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_PARTICLES].m_Id);
		Graphics()->QuadsBegin();

		while(i != -1)
		{
			RenderTools()->SelectSprite(m_aParticles[i].m_Spr);
			float a = m_aParticles[i].m_Life / m_aParticles[i].m_LifeSpan;
			vec2 p = m_aParticles[i].m_Pos;
			float Size = mix(m_aParticles[i].m_StartSize, m_aParticles[i].m_EndSize, a);

			Graphics()->QuadsSetRotation(m_aParticles[i].m_Rot);

			Graphics()->SetColor(
				m_aParticles[i].m_Color.r,
				m_aParticles[i].m_Color.g,
				m_aParticles[i].m_Color.b,
				m_aParticles[i].m_Color.a); // pow(a, 0.75f) *

			IGraphics::CQuadItem QuadItem(p.x, p.y, Size, Size);
			Graphics()->QuadsDraw(&QuadItem, 1);

			i = m_aParticles[i].m_NextPart;
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
	}
}
