#include "stdafx.h"

#include "App.h"
#include "Camera.h"
#include "ClusteredShading.h"
#include "CubeMap.h"
#include "D3DDrawHelpers.h"
#include "D3DGlobals.h"
#include "D3DInitHelpers.h"
#include "DDGI.h"
#include "DebugDrawing.h"
#include "DepthBuffer.h"
#include "FrameConstants.h"
#include "GBuffer.h"
#include "Geometry.h"
#include "ImguiHelpers.h"
#include "IndirectDiffuse.h"
#include "Input.h"
#include "Light.h"
#include "MipGeneration.h"
#include "PathTracer.h"
#include "PostProcess.h"
#include "Random.h"
#include "RenderTarget.h"
#include "SSAO.h"
#include "SSSR.h"
#include "SwapChain.h"
#include "TAA.h"
#include "Texture.h"
#include "Window.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd)
{
	HWND mainWindow = CreateMainWindow(hInstance, nShowCmd, App::name);

	ComPtr<ID3D12Device10> device = CreateDevice();

	if (!CheckEssentialFeatures(mainWindow, device.Get()))
	{
		return FALSE;
	}

	const uint32_t renderTargetWidth = App::renderSettings.renderTargetWidth;
	const uint32_t renderTargetHeight = App::renderSettings.renderTargetHeight;
	const float aspectRatio = static_cast<float>(renderTargetWidth) / renderTargetHeight;
	const uint32_t renderTargetMipCount = ComputeMaximumMipLevel(renderTargetWidth, renderTargetHeight);

	D3D::InitGlobalState(device.Get(), renderTargetWidth, renderTargetHeight);

	ComPtr<ID3D12CommandQueue> commandQueue = CreateCommandQueue(device.Get());
	ComPtr<ID3D12GraphicsCommandList10> commandList;
	CheckForErrors(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Frame::current->commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

	RWBufferResource scratchBuffer = CreateRWBufferResource(device.Get(), {.size = App::renderSettings.accelerationStructureScratchBufferSizeBytes }, D3D12_HEAP_TYPE_DEFAULT);

	App::Init(device.Get(), commandList.Get(), D3D::persistentAllocator, D3D::descriptorHeap, D3D::globalStaticBuffer, scratchBuffer);

	// initialize render systems
	{
		MipGenerator::Init(device.Get());
		BlueNoiseGeneration::Init(device.Get(), D3D::descriptorHeap, D3D::globalStaticBuffer);

		GBuffer::Init(device.Get(),
			renderTargetWidth,
			renderTargetHeight,
			D3D::depthStencilFormat,
			D3D::descriptorHeap,
			D3D::globalStaticBuffer);

		DDGI::Init(device.Get(),
			D3D::descriptorHeap,
			D3D::globalStaticBuffer,
			{ .probeSpacing = App::renderSettings.ddgiProbeSpacing, .relativeOffset = App::renderSettings.ddgiRelativeOffset });

		SSSR::Init(device.Get(), renderTargetWidth, renderTargetHeight, D3D::descriptorHeap, D3D::globalStaticBuffer);
		SSAO::Init(device.Get(), D3D::descriptorHeap, renderTargetWidth, renderTargetHeight);
		IndirectDiffuse::Init(device.Get(), renderTargetWidth, renderTargetHeight, D3D::descriptorHeap);
		TAA::Init(device.Get(), D3D::HDRRenderTargetFormat, renderTargetWidth, renderTargetHeight, D3D::descriptorHeap);
		PathTracer::Init(device.Get(), D3D::descriptorHeap, renderTargetWidth, renderTargetHeight);
		PostProcess::Init(device.Get(), D3D::backbufferFormat);
	}

	TemporaryTlas tlas;
	ResourceTransitions(commandList.Get(), { scratchBuffer.Barrier(ResourceState::ScratchBuildAccelerationStructure, ResourceState::ScratchBuildAccelerationStructure) });

	ShadowMaps cascadedShadowMap;
	cascadedShadowMap.Init(device.Get(),
		directionalLightsMaxCount * cascadeCount,
		App::renderSettings.cascadedShadowMapSize,
		App::renderSettings.cascadedShadowMapSize,
		DXGI_FORMAT_D32_FLOAT,
		D3D::descriptorHeap,
		D3D::globalStaticBuffer,
		L"Cascaded Shadow Map");

	ShadowMaps omnidirectionalShadowMaps;
	omnidirectionalShadowMaps.Init(device.Get(),
		App::renderSettings.shadowedPointLightsMaxCount * 6,
		App::renderSettings.omnidirectionalShadowMapSize,
		App::renderSettings.omnidirectionalShadowMapSize,
		App::renderSettings.omndirectionalShadowMapsFormt,
		D3D::descriptorHeap,
		D3D::globalStaticBuffer,
		L"Omnidirectional Shadow Maps");

	const uint32_t cubeMapSize = App::renderSettings.cubeMapSize;
	CubeMaps cubeMaps;
	cubeMaps.Init(device.Get(),
		cubeMapSize,
		App::renderSettings.cubeMapsMaxCount,
		ComputeMaximumMipLevel(cubeMapSize, cubeMapSize),
		D3D::HDRRenderTargetFormat,
		App::renderSettings.cubeMapsFormat,
		D3D::descriptorHeap,
		D3D::globalStaticBuffer); 
	cubeMaps.perFrameFaceUpdatesCount = App::renderSettings.cubeMapFacesPerFrameUpdateCount;

	ClusteredShadingContext mainViewClusteredShadingContext; 
	mainViewClusteredShadingContext.Init(device.Get(),
		D3D::descriptorHeap,
		D3D::globalStaticBuffer,
		renderTargetWidth,
		renderTargetHeight,
		L"Main View");

	SwapChain swapChain;
	swapChain.Init(device.Get(),
		CreateDXGIFactory().Get(),
		commandQueue.Get(),
		mainWindow,
		D3D::swapChainBufferCount,
		D3D::backbufferFormat);

	Camera camera(0.25f * DirectX::XM_PI, aspectRatio, D3D::nearZ, D3D::farZ);
	Camera debugCamera = camera;
	DebugView::Init(device.Get(),
		D3D::descriptorHeap,
		D3D::globalStaticBuffer,
		D3D::HDRRenderTargetFormat,
		D3D::depthStencilFormat,
		renderTargetWidth,
		renderTargetHeight);

	Input::Init(mainWindow);

	UI::Init(mainWindow,
		device.Get(),
		D3D::descriptorHeap,
		swapChain.renderTargets[0].properties.width,
		swapChain.renderTargets[0].properties.height,
		Frame::framesInFlightCount);

	UI::Context uiContext;
	uiContext.Init(cascadedShadowMap, *App::menu);

	MSG msg = { };
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			//Update frame counters
			PIXScopedEvent(commandList.Get(), PIX_COLOR_DEFAULT, "Frame: %u", Frame::timingData.frameId);
			Frame::Begin();
			swapChain.FrameBegin(commandList.Get());
			UI::FrameBegin();
			ScratchHeap& frameMemory = Frame::current->gpuMemory;
			TemporaryDescriptorHeap& frameDescriptorHeap = Frame::current->descriptorHeap;

			Input::Update(gMouseX, gMouseY);

			//Initialize render state
			D3D::PrepareCommandList(commandList.Get(), D3D::descriptorHeap, D3D::globalStaticBuffer);
			
			const RenderData renderData = App::Update(Frame::current->cpuMemory, Frame::timingData);

			uiContext.Update(frameDescriptorHeap);
			UI::DebugVisualizationSettings& debugVisualizationSettings = uiContext.sharedSettings.debugVisualizationSettings;

			camera.Update(
				uiContext.isFocusDebugCameraWindow ? Camera::Transform{} : renderData.cameraTransform,
				uiContext.taaSettings.useTaa ? HaltonSubPixelJitter(renderTargetWidth, renderTargetHeight, Frame::timingData.frameId) : DirectX::XMFLOAT2{}
			);

			TemporaryBuffer<FrameConstants> frameConstantsBuffer = UpdateFrameConstants(commandList.Get(),
				frameMemory,
				camera,
				GetTexture2DDimensions(D3D::mainRenderTarget),
				*BlueNoiseGeneration::texture,
				GBuffer::GetSrvIds(),
				Frame::timingData,
				uiContext.sharedSettings);

			BufferHeap::Offset cameraDataOffset = BufferMemberOffset(frameConstantsBuffer, mainCameraData);

			tlas.Build(device.Get(), commandList.Get(), frameDescriptorHeap, frameMemory, scratchBuffer, renderData.opaqueMeshes);

			BlueNoiseGeneration::Generate(commandList.Get());

			//Shadow map render pass
			DirectX::BoundingBox boundingBox = ComputeCompoundMeshBoundingBox(renderData.shadowCasters);

			LightsData lightsData = ComputeLightsData(frameMemory,
				camera,
				renderData.directionalLights,
				renderData.shadowedPointLights,
				renderData.pointLights,
				boundingBox,
				cascadedShadowMap,
				omnidirectionalShadowMaps);

			cascadedShadowMap.RenderShadowMaps(device.Get(),
				commandList.Get(),
				renderData.shadowCasters,
				cascadeCount * lightsData.directionalLightsCount,
				uiContext.directionalShadowSettings);

			omnidirectionalShadowMaps.RenderShadowMaps(device.Get(),
				commandList.Get(),
				renderData.shadowCasters,
				static_cast<uint32_t>(renderData.shadowedPointLights.size()) * 6,
				uiContext.omnidirectionalShadowSettings); 

			LightingData lightingData =
			{
				.lightsData = lightsData,
				.clusterDataOffset = mainViewClusteredShadingContext.GetClusterDataBufferOffset(),
				.ddgiDataOffset = DDGI::bufferOffset,
				.cubeMapSpecularId = cubeMaps.renderTargets.srvId,
			};
			BufferHeap::Offset lightingDataBufferOffset = WriteTemporaryData(frameMemory, lightingData);

			//GBuffer laydown 
			GBuffer::RenderBegin(commandList.Get(), cameraDataOffset);
			Draw::Opaque(commandList.Get(), {}, renderData.opaqueMeshes, false);
			GBuffer::RenderEnd(commandList.Get(), frameDescriptorHeap);

			// Clustered lights binning pass
			mainViewClusteredShadingContext.PrepareClusterData(commandList.Get(), frameDescriptorHeap, lightsData, cameraDataOffset, &GBuffer::depthPyramide);

			// Cubemaps rendering pass
			for (int i : AllCubeMaps{ cubeMaps,
				commandList.Get(),
				frameDescriptorHeap,
				frameMemory,
				lightingData,
				renderData.cubeMapsTransformsOffset,
				renderData.activeCubeMapsCount }) //@note: this works because of brace elision
			{
				Draw::SkyBox(commandList.Get(),  renderData.skyBoxSrvId);
				Draw::Opaque(commandList.Get(), {},	renderData.opaqueMeshes);
			}

			DDGI::Render(commandList.Get(), lightingDataBufferOffset, tlas.GetTlasData(), renderData.skyBoxSrvId);

			// per pixel passes
			SSSR::Render(commandList.Get(), D3D::mainRenderTarget.Other().srvId, lightingDataBufferOffset);
			SSAO::Render(commandList.Get(), DDGI::bufferOffset, D3D::mainRenderTarget.Other().srvId);

			IndirectDiffuse::Render(commandList.Get(),
				frameDescriptorHeap,
				tlas.GetTlasData(),
				renderData.skyBoxSrvId,
				lightingDataBufferOffset,
				SSAO::bufferSrvId,
				uiContext.sharedSettings.indirectDiffuseSettings);

			//GBuffer lighting and other, non-opaque rendering
			{
				PIXScopedEvent(commandList.Get(), PIX_COLOR_DEFAULT, "Rendering");
				{
					PIXScopedEvent(commandList.Get(), PIX_COLOR_DEFAULT, "GBuffer Lighting");
					D3D::mainRenderTarget->Clear(commandList.Get());
					D3D::mainRenderTarget->Bind(commandList.Get(), GBuffer::depthBuffer->GetDsv());

					ResourceTransitions(commandList.Get(),
						{
							D3D::mainRenderTarget->Barrier(ResourceState::RenderTarget, ResourceState::WriteCS),
							GBuffer::depthBuffer.Current().Barrier(ResourceState::ReadCS, ResourceState::DepthRead)
						});

					GBuffer::Lighting(commandList.Get(),
						D3D::mainRenderTarget,
						SSAO::bufferSrvId,
						IndirectDiffuse::bufferSrvId,
						SSSR::bufferSrvId,
						lightingDataBufferOffset);
				}

				ResourceTransitions(commandList.Get(),
					{
						D3D::mainRenderTarget->Barrier(ResourceState::WriteCS, ResourceState::RenderTarget),
						GBuffer::depthBuffer->Barrier(ResourceState::DepthRead, ResourceState::DepthWrite)
					});

				PIXScopedEvent(commandList.Get(), PIX_COLOR_DEFAULT, "Non-opaque Rendering");
				{
					BindFixedRenderGraphicsRootConstants(commandList.Get(),
						BufferHeap::InvalidOffset,
						cameraDataOffset,
						BufferHeap::InvalidOffset,
						{});

					Draw::SkyBox(commandList.Get(), renderData.skyBoxSrvId);

					debugVisualizationSettings.isActiveDDGIVisualization ? DDGI::DrawDebugVisualization(commandList.Get()) : void();
				}
				ResourceTransitions(commandList.Get(),
					{
						D3D::mainRenderTarget->Barrier(ResourceState::RenderTarget, ResourceState::ReadCS),
						GBuffer::depthBuffer->Barrier(ResourceState::DepthWrite, ResourceState::ReadCS)
					});
			}

			//debugCamera rendering
			{
				if (debugVisualizationSettings.isActiveDebugCamera)
				{
					if (uiContext.isFocusDebugCameraWindow)
					{
						debugCamera.Update(ProcessInput(Frame::timingData.deltaTimeMs, uiContext.isFocusDebugCameraWindow)); //@todo: what about jitter
					}
					DebugView::RenderBegin(commandList.Get(),
						WriteTemporaryData(frameMemory, debugCamera.constants.Current()),
						lightingDataBufferOffset);

					Draw::SkyBox(commandList.Get(), renderData.skyBoxSrvId);
					Draw::Opaque(commandList.Get(), { .ssaoBufferSrvId = SSAO::bufferSrvId, .sssrBufferSrvId = SSSR::bufferSrvId }, renderData.opaqueMeshes);

					debugVisualizationSettings.isActiveDDGIVisualization ? DDGI::DrawDebugVisualization(commandList.Get()) : void();

					DebugView::RenderEnd(commandList.Get());

					uiContext.isFocusDebugCameraWindow = UI::DebugCameraWindow(frameDescriptorHeap, DebugView::renderTarget, debugVisualizationSettings.isActiveDebugCamera);
				}
				else
				{
					uiContext.isFocusDebugCameraWindow = false;
				}
			}

			ResourceTransitions(commandList.Get(),
				{
					cascadedShadowMap.Done(),
					omnidirectionalShadowMaps.Done(),
					cubeMaps.Done()
				});

			//Generate blurred hdr pyramide
			MipGenerator::GenerateMipsSeparableKernel(commandList.Get(),
				frameDescriptorHeap,
				D3D::mainRenderTarget->GetSubresource(),
				MipGenerator::SeparableKernel::Gauss3x3,
				TemporaryRWTexture(D3D::temporaryHdrBufferPool),
				L"DownsampleHDR target");

			RWTexture& temporaryHdrBuffer = D3D::temporaryHdrBufferPool.Acquire();
			Texture* displayedBuffer = &TAA::Resolve(commandList.Get(), D3D::mainRenderTarget, uiContext.taaSettings.useTaa);

			//Reference path tracer
			if (Texture* pathTracerOutput = PathTracer::Render(commandList.Get(),
				temporaryHdrBuffer,
				tlas.GetTlasData(),
				renderData.skyBoxSrvId,
				lightingDataBufferOffset,
				uiContext.sharedSettings.pathtracerSettings))
			{
				displayedBuffer = pathTracerOutput;
			}

			//Render fullscreen postprocessing passes
			{
				PostProcess::RenderBloom(commandList.Get(),
					Frame::current->descriptorHeap,
					*displayedBuffer,
					temporaryHdrBuffer,
					TemporaryRWTexture(D3D::temporaryHdrBufferPool),
					TemporaryRWTexture(D3D::temporaryHdrBufferPool),
					uiContext.postProcessSettings.bloomSettings);

				ResourceTransitions(commandList.Get(), { temporaryHdrBuffer.Barrier(ResourceState::WriteCS, ResourceState::ReadPS)
			});

				PostProcess::ToneMap(commandList.Get(),
					frameMemory,
					temporaryHdrBuffer,
					*swapChain.currentBackBuffer,
					uiContext.postProcessSettings.colorGradingSettings);
			}
			D3D::temporaryHdrBufferPool.Release(temporaryHdrBuffer);

			UI::Render(commandList.Get(), swapChain.GetLinearRtvHandle());

			ResourceTransitions(commandList.Get(), 
				{
					swapChain.Done(),
					temporaryHdrBuffer.Done(ResourceState::ReadAny, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS),
					D3D::mainRenderTarget.Other().Done(ResourceState::ReadAny, D3D12_BARRIER_LAYOUT_RENDER_TARGET),
					BlueNoiseGeneration::Done(),
					SSAO::Done(),
					PathTracer::Done()
				});

			GBuffer::FrameEnd(commandList.Get());
			DDGI::FrameEnd(commandList.Get());
			SSSR::FrameEnd(commandList.Get());
			D3D::mainRenderTarget.Flip();

			//Close and submit command list
			commandList->Close();
			ID3D12CommandList* commandLists[] = { commandList.Get() };
			commandQueue->ExecuteCommandLists(1, commandLists);

			swapChain.Present(false);

			Frame::End(commandQueue.Get(), commandList.Get()/*, 33*/);
			D3D::stackAllocator.Reset(); 
		}
	}

	Frame::FlushCommandQueue();

	cascadedShadowMap.Free();
	omnidirectionalShadowMaps.Free();
	cubeMaps.Free();
	swapChain.Free();

	UI::Shutdown();
	D3D::Shutdown();
	
	return (int)msg.wParam;
}
