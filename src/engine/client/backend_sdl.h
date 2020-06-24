#ifndef ENGINE_CLIENT_BACKEND_SDL_H
#define ENGINE_CLIENT_BACKEND_SDL_H

#include "SDL.h"

#include "graphics_threaded.h"

# if defined(CONF_PLATFORM_MACOSX)
	#include <objc/objc-runtime.h>

	class CAutoreleasePool
	{
	private:
		id m_Pool;

	public:
		CAutoreleasePool()
		{
			Class NSAutoreleasePoolClass = (Class) objc_getClass("NSAutoreleasePool");
			m_Pool = class_createInstance(NSAutoreleasePoolClass, 0);
			SEL selector = sel_registerName("init");
			((id (*)(id, SEL))objc_msgSend)(m_Pool, selector);
		}

		~CAutoreleasePool()
		{
			SEL selector = sel_registerName("drain");
			((id (*)(id, SEL))objc_msgSend)(m_Pool, selector);
		}
	};
#endif


// basic threaded backend, abstract, missing init and shutdown functions
class CGraphicsBackend_Threaded : public IGraphicsBackend
{
public:
	// constructed on the main thread, the rest of the functions is run on the render thread
	class ICommandProcessor
	{
	public:
		virtual ~ICommandProcessor() {}
		virtual void RunBuffer(CCommandBuffer *pBuffer) = 0;
	};

	CGraphicsBackend_Threaded();

	virtual void RunBuffer(CCommandBuffer *pBuffer);
	virtual bool IsIdle() const;
	virtual void WaitForIdle();

protected:
	void StartProcessor(ICommandProcessor *pProcessor);
	void StopProcessor();

private:
	ICommandProcessor *m_pProcessor;
	CCommandBuffer * volatile m_pBuffer;
	volatile bool m_Shutdown;
	semaphore m_Activity;
	semaphore m_BufferDone;
	void *m_pThread;

	static void ThreadFunc(void *pUser);
};

// takes care of implementation independent operations
class CCommandProcessorFragment_General
{
	void Cmd_Nop();
	void Cmd_Signal(const CCommandBuffer::SCommand_Signal *pCommand);
public:
	bool RunCommand(const CCommandBuffer::SCommand  *pBaseCommand);
};

// takes care of opengl related rendering
class CCommandProcessorFragment_OpenGL
{
	struct CTexture
	{
		GLuint m_Tex;
		int m_MemSize;

		int m_Width;
		int m_Height;
		int m_RescaleCount;
	};
	CTexture m_aTextures[CCommandBuffer::MAX_TEXTURES];
	volatile int *m_pTextureMemoryUsage;

	GLint m_MaxTexSize;
public:
	enum
	{
		CMD_INIT = CCommandBuffer::CMDGROUP_PLATFORM_OPENGL,
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() : SCommand(CMD_INIT) {}
		volatile int *m_pTextureMemoryUsage;
	};

private:
	static int TexFormatToOpenGLFormat(int TexFormat);
	static unsigned char Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp);
	static void *Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData);

	void SetState(const CCommandBuffer::SState &State);

	void Cmd_Init(const SCommand_Init *pCommand);
	void Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand);
	void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand);
	void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand);
	void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand);
	void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand);
	void Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand);

public:
	CCommandProcessorFragment_OpenGL();

	bool RunCommand(const CCommandBuffer::SCommand  *pBaseCommand);
};


#define MAX_STREAM_BUFFER_COUNT 30

class CGLSLProgram;
class CGLSLTWProgram;
class CGLSLPrimitiveProgram;
class CGLSLQuadProgram;
class CGLSLTileProgram;
class CGLSLBorderTileProgram;
class CGLSLBorderTileLineProgram;
class CGLSLTextProgram;
class CGLSLSpriteProgram;
class CGLSLSpriteMultipleProgram;
// takes care of opengl 3.3 related rendering
class CCommandProcessorFragment_OpenGL3_3
{
	bool m_UseMultipleTextureUnits;
	bool m_UsePreinitializedVertexBuffer;
	
	struct CTexture
	{
		GLuint m_Tex;
		GLuint m_Sampler;
		int m_LastWrapMode;
		int m_MemSize;

		int m_Width;
		int m_Height;

		int m_RescaleCount;
	};
	CTexture m_aTextures[CCommandBuffer::MAX_TEXTURES];
	volatile int *m_pTextureMemoryUsage;

	CGLSLPrimitiveProgram *m_pPrimitiveProgram;
	CGLSLTileProgram *m_pTileProgram;
	CGLSLTileProgram *m_pTileProgramTextured;
	CGLSLBorderTileProgram *m_pBorderTileProgram;
	CGLSLBorderTileProgram *m_pBorderTileProgramTextured;
	CGLSLBorderTileLineProgram *m_pBorderTileLineProgram;
	CGLSLBorderTileLineProgram *m_pBorderTileLineProgramTextured;
	CGLSLQuadProgram *m_pQuadProgram;
	CGLSLQuadProgram *m_pQuadProgramTextured;
	CGLSLTextProgram *m_pTextProgram;
	CGLSLSpriteProgram *m_pSpriteProgram;
	CGLSLSpriteMultipleProgram *m_pSpriteProgramMultiple;

	GLuint m_LastProgramID;

	GLuint m_PrimitiveDrawVertexID[MAX_STREAM_BUFFER_COUNT];
	GLuint m_PrimitiveDrawBufferID[MAX_STREAM_BUFFER_COUNT];

	GLuint m_LastIndexBufferBound[MAX_STREAM_BUFFER_COUNT];

	int m_LastStreamBuffer;
	
	GLuint m_QuadDrawIndexBufferID;
	unsigned int m_CurrentIndicesInBuffer;
	
	GLint m_MaxTextureUnits;
	GLint m_MaxTexSize;
	
	int m_LastBlendMode; //avoid all possible opengl state changes
	bool m_LastClipEnable;
	
	struct STextureBound{
		int m_TextureSlot;
	};
	std::vector<STextureBound> m_TextureSlotBoundToUnit; //the texture index generated by loadtextureraw is stored in an index calculated by max texture units
	
	bool IsAndUpdateTextureSlotBound(int IDX, int Slot);
	void DestroyTexture(int Slot);
	void DestroyBufferContainer(int Index, bool DeleteBOs = true);
	
	void AppendIndices(unsigned int NewIndicesCount);
	
	struct SBufferContainer
	{
		SBufferContainer() : m_VertArrayID(0), m_LastIndexBufferBound(0) {}
		GLuint m_VertArrayID;
		GLuint m_LastIndexBufferBound;
		SBufferContainerInfo m_ContainerInfo;
	};
	std::vector<SBufferContainer> m_BufferContainers;

	std::vector<GLuint> m_BufferObjectIndices;
	
	CCommandBuffer::SColorf m_ClearColor;
public:
	enum
	{
		CMD_INIT = CCommandBuffer::CMDGROUP_PLATFORM_OPENGL3_3,
		CMD_SHUTDOWN,
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() : SCommand(CMD_INIT) {}
		class IStorage *m_pStorage;
		volatile int *m_pTextureMemoryUsage;
	};

	struct SCommand_Shutdown : public CCommandBuffer::SCommand
	{
		SCommand_Shutdown() : SCommand(CMD_SHUTDOWN) {}
	};

private:
	static int TexFormatToOpenGLFormat(int TexFormat);
	static unsigned char Sample(int w, int h, const unsigned char *pData, int u, int v, int Offset, int ScaleW, int ScaleH, int Bpp);
	static void *Rescale(int Width, int Height, int NewWidth, int NewHeight, int Format, const unsigned char *pData);
	
	void SetState(const CCommandBuffer::SState &State, CGLSLTWProgram *pProgram);
	void UseProgram(CGLSLTWProgram *pProgram);
	void UploadStreamBufferData(unsigned int PrimitiveType, const void* pVertices, unsigned int PrimitiveCount);
	void RenderText(const CCommandBuffer::SState& State, int DrawNum, int TextTextureIndex, int TextOutlineTextureIndex, int TextureSize, const float* pTextColor, const float* pTextOutlineColor);

	void Cmd_Init(const SCommand_Init *pCommand);
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand);
	void Cmd_Texture_Update(const CCommandBuffer::SCommand_Texture_Update *pCommand);
	void Cmd_Texture_Destroy(const CCommandBuffer::SCommand_Texture_Destroy *pCommand);
	void Cmd_Texture_Create(const CCommandBuffer::SCommand_Texture_Create *pCommand);
	void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand);
	void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand);
	void Cmd_Screenshot(const CCommandBuffer::SCommand_Screenshot *pCommand);

	void Cmd_CreateBufferObject(const CCommandBuffer::SCommand_CreateBufferObject *pCommand);
	void Cmd_RecreateBufferObject(const CCommandBuffer::SCommand_RecreateBufferObject *pCommand);
	void Cmd_UpdateBufferObject(const CCommandBuffer::SCommand_UpdateBufferObject *pCommand);
	void Cmd_CopyBufferObject(const CCommandBuffer::SCommand_CopyBufferObject *pCommand);
	void Cmd_DeleteBufferObject(const CCommandBuffer::SCommand_DeleteBufferObject *pCommand);

	void Cmd_CreateBufferContainer(const CCommandBuffer::SCommand_CreateBufferContainer *pCommand);
	void Cmd_UpdateBufferContainer(const CCommandBuffer::SCommand_UpdateBufferContainer *pCommand);
	void Cmd_DeleteBufferContainer(const CCommandBuffer::SCommand_DeleteBufferContainer *pCommand);
	void Cmd_IndicesRequiredNumNotify(const CCommandBuffer::SCommand_IndicesRequiredNumNotify *pCommand);
		
	void Cmd_RenderTileLayer(const CCommandBuffer::SCommand_RenderTileLayer *pCommand);	
	void Cmd_RenderBorderTile(const CCommandBuffer::SCommand_RenderBorderTile *pCommand);
	void Cmd_RenderBorderTileLine(const CCommandBuffer::SCommand_RenderBorderTileLine *pCommand);
	void Cmd_RenderQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer *pCommand);
	void Cmd_RenderText(const CCommandBuffer::SCommand_RenderText *pCommand);
	void Cmd_RenderTextStream(const CCommandBuffer::SCommand_RenderTextStream *pCommand);
	void Cmd_RenderQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer *pCommand);
	void Cmd_RenderQuadContainerAsSprite(const CCommandBuffer::SCommand_RenderQuadContainerAsSprite *pCommand);
	void Cmd_RenderQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *pCommand);
public:
	CCommandProcessorFragment_OpenGL3_3();

	bool RunCommand(const CCommandBuffer::SCommand  *pBaseCommand);
};

// takes care of sdl related commands
class CCommandProcessorFragment_SDL
{
	// SDL stuff
	SDL_Window *m_pWindow;
	SDL_GLContext m_GLContext;
public:
	enum
	{
		CMD_INIT = CCommandBuffer::CMDGROUP_PLATFORM_SDL,
		CMD_UPDATE_VIEWPORT,
		CMD_SHUTDOWN,
	};

	struct SCommand_Init : public CCommandBuffer::SCommand
	{
		SCommand_Init() : SCommand(CMD_INIT) {}
		SDL_Window *m_pWindow;
		SDL_GLContext m_GLContext;
	};

	struct SCommand_Update_Viewport : public CCommandBuffer::SCommand
	{
		SCommand_Update_Viewport() : SCommand(CMD_UPDATE_VIEWPORT) {}
		int m_X;
		int m_Y;
		int m_Width;
		int m_Height;
	};

	struct SCommand_Shutdown : public CCommandBuffer::SCommand
	{
		SCommand_Shutdown() : SCommand(CMD_SHUTDOWN) {}
	};

private:
	void Cmd_Init(const SCommand_Init *pCommand);
	void Cmd_Update_Viewport(const SCommand_Update_Viewport *pCommand);
	void Cmd_Shutdown(const SCommand_Shutdown *pCommand);
	void Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand);
	void Cmd_VSync(const CCommandBuffer::SCommand_VSync *pCommand);
	void Cmd_Resize(const CCommandBuffer::SCommand_Resize *pCommand);
	void Cmd_VideoModes(const CCommandBuffer::SCommand_VideoModes *pCommand);
public:
	CCommandProcessorFragment_SDL();

	bool RunCommand(const CCommandBuffer::SCommand *pBaseCommand);
};

// command processor impelementation, uses the fragments to combine into one processor
class CCommandProcessor_SDL_OpenGL : public CGraphicsBackend_Threaded::ICommandProcessor
{
	CCommandProcessorFragment_OpenGL m_OpenGL;
	CCommandProcessorFragment_OpenGL3_3 m_OpenGL3_3;
	CCommandProcessorFragment_SDL m_SDL;
	CCommandProcessorFragment_General m_General;
	
	bool m_UseOpenGL3_3;
public:
	void UseOpenGL3_3(bool Use) { m_UseOpenGL3_3 = Use; }
	virtual void RunBuffer(CCommandBuffer *pBuffer);
};

// graphics backend implemented with SDL and OpenGL
class CGraphicsBackend_SDL_OpenGL : public CGraphicsBackend_Threaded
{
	SDL_Window *m_pWindow;
	SDL_GLContext m_GLContext;
	ICommandProcessor *m_pProcessor;
	volatile int m_TextureMemoryUsage;
	int m_NumScreens;
	
	bool m_UseOpenGL3_3;
public:
	virtual int Init(const char *pName, int *Screen, int *pWidth, int *pHeight, int FsaaSamples, int Flags, int *pDesktopWidth, int *pDesktopHeight, int *pCurrentWidth, int *pCurrentHeight, class IStorage *pStorage);
	virtual int Shutdown();

	virtual int MemoryUsage() const;

	virtual int GetNumScreens() const { return m_NumScreens; }

	virtual void Minimize();
	virtual void Maximize();
	virtual bool Fullscreen(bool State);
	virtual void SetWindowBordered(bool State);	// on=true/off=false
	virtual bool SetWindowScreen(int Index);
	virtual int GetWindowScreen();
	virtual int WindowActive();
	virtual int WindowOpen();
	virtual void SetWindowGrab(bool Grab);
	virtual void NotifyWindow();
	
	virtual bool IsOpenGL3_3() { return m_UseOpenGL3_3; }
};

#endif // ENGINE_CLIENT_BACKEND_SDL_H
