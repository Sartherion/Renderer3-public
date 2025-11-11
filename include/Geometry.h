#pragma once
#include "Allocator.h"
#include "BufferMemory.h"
#include "DescriptorHeap.h"
#include "Texture.h"

struct Geometry 
{
	static constexpr DXGI_FORMAT indexBufferFormat = DXGI_FORMAT_R32_UINT;
	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;
	
	//@note: vertex buffer layout can be whatever is decided by the helper filling the geometry struct
	BufferHeap::Allocation memory;
	
	DirectX::BoundingBox aabb; 

	void Draw(ID3D12GraphicsCommandList10* commandList, uint32_t instanceCount = 1) const;
	void Bind(ID3D12GraphicsCommandList10* commandList) const;
	void Free();

	BufferHeap::Offset GetVertexBuffersOffset() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetVertexBuffersAddress() const;
	BufferHeap::Offset GetIndexBufferOffset() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetIndexBufferAddress() const;
};

//Create a single gpu buffer containing indices, positions, normals, and uvs of the mesh in SAO format
Geometry CreateGeometry(BufferHeap& heap,
	std::span<const uint32_t> indices,
	std::span<const DirectX::XMFLOAT3> positions,
	std::span<const DirectX::XMFLOAT3> normals,
	std::span<const DirectX::XMFLOAT2> uvs);

enum RaytracingGeometryType
{
	Opaque,
	Transparent
};

D3D12_RAYTRACING_GEOMETRY_DESC GetRaytracingGeometryDesc(const Geometry& geometry, RaytracingGeometryType type = RaytracingGeometryType::Transparent);

//complex model with several submeshes, i.e. materials, which share a transform
struct PbrMesh
{
	struct InstanceData
	{
		static const DirectX::XMFLOAT4X4 identity4x4;

		DirectX::XMFLOAT4X4 transforms;
		DirectX::XMFLOAT4X4 inverseTransposeTransform;
	};

	static const InstanceData InstanceDataDefault;

	struct Submesh
	{
		BufferHeap::Offset materialConstantsOffset = BufferHeap::InvalidOffset;
		uint32_t startIndexLocation;
		uint32_t indexCount;
	};
	
	struct MaterialConstants
	{
		static const uint32_t specularCubeMapsArrayMaxIndex;
		DirectX::XMFLOAT4 albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		float metallic = -1;
		float roughness = -1;
		DescriptorHeap::Id albedoTextureId = DescriptorHeap::InvalidId;
		DescriptorHeap::Id normalTextureId = DescriptorHeap::InvalidId;
		DescriptorHeap::Id roughnessTextureId = DescriptorHeap::InvalidId;
		DescriptorHeap::Id metallicTextureId = DescriptorHeap::InvalidId;
		uint32_t specularCubeMapsArrayIndex = specularCubeMapsArrayMaxIndex;
	};

	Geometry geometry;
	BufferResource rayTracingBlas;

	PersistentMemory<Submesh> submeshes; 
	std::vector<Texture> textures;
	PersistentBuffer<MaterialConstants> materialConstantsBuffer;
	PersistentBuffer<InstanceData> instanceDataBuffer;
	PersistentBuffer<Submesh> submeshDataBuffer;

	PersistentMemory<InstanceData> instanceData;
	uint32_t instanceCount = 1; 
	BufferHeap::Offset instanceDataOffset = BufferHeap::InvalidOffset;
	InstanceData* instanceDataPtr = nullptr; //@note: the additional offset and pointer exist in order for the PbrMesh also be able to use data temporary data instead of persistent

	void Draw(ID3D12GraphicsCommandList10* commandList) const;

	void DrawOneDrawcall(ID3D12GraphicsCommandList10* commandList) const;

	const InstanceData& GetInstanceData(uint32_t instance = 0) const
	{
		assert(instance < instanceCount);

		if (instanceDataPtr)
		{
			return instanceDataPtr[instance];
		}
		return PbrMesh::InstanceDataDefault;
	}

	void BuildBlas(ID3D12Device10* device,
		ID3D12GraphicsCommandList10* commandList,
		const RWBufferResource& scratchBuffer,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION);

	void RefitBlas(ID3D12GraphicsCommandList10* commandList, const RWBufferResource& scratchBuffer);
	void CompactBlas(); 

	void Free();
};

//Loads geomtry from a given .obj file and uploads vertices and indices to the GPU. //@note: Meshes need to be triangulated!
PbrMesh LoadMesh(ID3D12Device10* device,
	PersistentAllocator& allocator,
	DescriptorHeap & descriptorHeap,
	BufferHeap& bufferHeap,
	LPCWSTR fileName);

Geometry LoadGeometryData(BufferHeap& bufferHeap, LPCWSTR fileName);

void SetMaterial(PbrMesh& mesh, const PbrMesh::MaterialConstants& material);

void SetTemporaryInstanceData(PbrMesh& mesh, LinearAllocator& allocator, ScratchHeap& bufferHeap, const PbrMesh::InstanceData& instanceData);
void SetTemporaryInstanceData(PbrMesh& mesh, LinearAllocator& allocator, ScratchHeap& bufferHeap, std::span<const PbrMesh::InstanceData> instanceData);
void InitPersistentInstanceData(PbrMesh& mesh, PersistentAllocator& allocator, BufferHeap& bufferHeap, std::span<PbrMesh::InstanceData> instanceData);
void UpdatePersistentInstanceData(PbrMesh& mesh, const PbrMesh::InstanceData& instanceData, uint32_t elementIndex);
void UpdatePersistentInstanceData(PbrMesh& mesh, std::span<const PbrMesh::InstanceData> instanceData);

D3D12_RAYTRACING_GEOMETRY_DESC GetRaytracingGeometryDesc(const Geometry& geometry, RaytracingGeometryType type);
void GenerateGeometryDescs(const PbrMesh& mesh, std::span<D3D12_RAYTRACING_GEOMETRY_DESC> outGeometryDescs, D3D12_GPU_VIRTUAL_ADDRESS transformAddress = 0);

struct RaytracingInstanceGeometryData
{
	BufferHeap::Offset submeshDataOffset = BufferHeap::InvalidOffset;
	BufferHeap::Offset indexBufferOffset;
	BufferHeap::Offset vertexBufferOffset;
	BufferHeap::Offset vertexCount;
	BufferHeap::Offset transformOffset = BufferHeap::InvalidOffset; //@note: this is only needed because CandidiateObjectToWorld3x4() etc. does not seem to work for some reason
};

RaytracingInstanceGeometryData GetRaytracingInstanceGeometryData(const PbrMesh& mesh);

struct TlasData
{
	DescriptorHeap::Id accelerationStructureSrvId = DescriptorHeap::InvalidId;
	BufferHeap::Offset instanceGeometryDataOffset = BufferHeap::InvalidOffset;
};

struct PersistentTlas
{
	BufferResource accelerationStructureBuffer;
	DescriptorHeap::Allocation srvId;
	PersistentBuffer<D3D12_RAYTRACING_INSTANCE_DESC> instanceDataBuffer;
	PersistentBuffer<RaytracingInstanceGeometryData> instanceGeometryDataBuffer;

	void Build(ID3D12Device10* device,
		ID3D12GraphicsCommandList10* commandList,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap,
		const RWBufferResource& scratchBuffer,
		std::span<const PbrMesh*> meshes);

	TlasData GetTlasData() const
	{
		return
		{
			.accelerationStructureSrvId = srvId,
			.instanceGeometryDataOffset = instanceGeometryDataBuffer.offset
		};
	}
};

struct TemporaryTlas
{
	BufferResource accelerationStructureBuffer;
	BufferHeap::Offset instanceGeometryDataOffset;
	DescriptorHeap::Id srvId;

	void Build(ID3D12Device10* device,
		ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		ScratchHeap& bufferHeap,
		const RWBufferResource& scratchBuffer,
		std::span<const PbrMesh*> meshes);

	TlasData GetTlasData() const
	{
		return
		{
			.accelerationStructureSrvId = srvId,
			.instanceGeometryDataOffset = instanceGeometryDataOffset
		};
	}
};
