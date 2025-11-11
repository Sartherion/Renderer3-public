#include "stdafx.h"
#include "Geometry.h"

#include "Frame.h"
#include "Raytracing.h"
#include "SharedDefines.h"

static Geometry LoadGeometryData(BufferHeap& bufferHeap, const rapidobj::Result& model, std::span<PbrMesh::Submesh> submeshes = {});
static void LoadMeshMaterials(ID3D12Device10* device, std::span<PbrMesh::MaterialConstants> materialConstants, std::vector<Texture>& textures, DescriptorHeap& descriptorHeap, const rapidobj::Materials& materials);

const DirectX::XMFLOAT4X4 PbrMesh::InstanceData::identity4x4 = DirectX::XMFLOAT4X4(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
);

const uint32_t PbrMesh::MaterialConstants::specularCubeMapsArrayMaxIndex = ::specularCubeMapsArrayMaxIndex;

const PbrMesh::InstanceData PbrMesh::InstanceDataDefault = { InstanceData::identity4x4, InstanceData::identity4x4 };

void Geometry::Draw(ID3D12GraphicsCommandList10* commandList, uint32_t instanceCount) const
{
	Bind(commandList);
	commandList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
}

void Geometry::Bind(ID3D12GraphicsCommandList10* commandList) const
{
	D3D12_INDEX_BUFFER_VIEW indexBufferView =
	{
		.BufferLocation = GetIndexBufferAddress(),
		.SizeInBytes = (indexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2) * indexCount,
		.Format = indexBufferFormat
	};
	commandList->IASetIndexBuffer(&indexBufferView);

	BindGraphicsRootConstants<2>(commandList,
		GetVertexBuffersOffset(),
		vertexCount);
}

void Geometry::Free()
{
	Frame::SafeRelease(memory);
	memory.offset = BufferHeap::InvalidOffset;
	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;
}

BufferHeap::Offset Geometry::GetVertexBuffersOffset() const
{
	int indexSizeBytes = indexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2;
	return GetIndexBufferOffset() + indexSizeBytes * indexCount;
}

D3D12_GPU_VIRTUAL_ADDRESS Geometry::GetVertexBuffersAddress() const
{
	int indexSizeBytes = indexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2;
	return GetIndexBufferAddress() + indexSizeBytes * indexCount;
}

BufferHeap::Offset Geometry::GetIndexBufferOffset() const
{
	return memory.offset;
}

D3D12_GPU_VIRTUAL_ADDRESS Geometry::GetIndexBufferAddress() const
{
	return memory.offset + memory.allocator->parentBuffer.resource->GetGPUVirtualAddress();
}

void PbrMesh::Draw(ID3D12GraphicsCommandList10* commandList) const
{
	geometry.Bind(commandList);
	commandList->SetGraphicsRoot32BitConstant(0, instanceDataOffset, 0);

	for (uint32_t i = 0; i < submeshes.Count(); i++)
	{
		commandList->SetGraphicsRoot32BitConstant(0, submeshes.Get(i).materialConstantsOffset, 1);
		commandList->DrawIndexedInstanced(submeshes.Get(i).indexCount, instanceCount, submeshes.Get(i).startIndexLocation, 0, 0); //the index buffer was constructed such that no baseVertexLocation is required. This has the advantage that mesh can also be drawn in one drawcall with the same material and it still works
	}
}

void PbrMesh::DrawOneDrawcall(ID3D12GraphicsCommandList10* commandList) const
{
	commandList->SetGraphicsRoot32BitConstant(0, instanceDataOffset, 0);
	geometry.Draw(commandList, instanceCount);
}

void PbrMesh::BuildBlas(ID3D12Device10* device, ID3D12GraphicsCommandList10* commandList, const RWBufferResource& scratchBuffer, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags)
{
	StackContext stackContext;

	const uint32_t subMeshCount = submeshes.Count();
	std::span<D3D12_RAYTRACING_GEOMETRY_DESC> rtGeometryDescs{ stackContext.Allocate<D3D12_RAYTRACING_GEOMETRY_DESC>(subMeshCount), subMeshCount };
	GenerateGeometryDescs(*this, rtGeometryDescs); 

	BuildAccelerationStructure(rayTracingBlas, device, commandList, GetBlasInputs(rtGeometryDescs, flags), scratchBuffer, L"BLAS");

	ResourceTransitions(commandList,
		{ 
			scratchBuffer.Barrier(ResourceState::ScratchBuildAccelerationStructure, ResourceState::ScratchBuildAccelerationStructure),
			rayTracingBlas.Barrier(ResourceState::Any, ResourceState::ReadAccelerationStructure)
		}); //@note: it is important that there is a barrier between building of blas and tlas!
}

void PbrMesh::RefitBlas(ID3D12GraphicsCommandList10* commandList, const RWBufferResource& scratchBuffer)
{
	StackContext stackContext;

	std::span<D3D12_RAYTRACING_GEOMETRY_DESC> rtGeometryDescs{ stackContext.Allocate<D3D12_RAYTRACING_GEOMETRY_DESC>(submeshes.Count()), submeshes.Count() };
	GenerateGeometryDescs(*this, rtGeometryDescs); 

	RefitAccelerationStructure(rayTracingBlas, commandList, GetBlasInputs(rtGeometryDescs), scratchBuffer);

	ResourceTransitions(commandList,
		{ 
			scratchBuffer.Barrier(ResourceState::ScratchBuildAccelerationStructure, ResourceState::ScratchBuildAccelerationStructure),
			rayTracingBlas.Barrier(ResourceState::Any, ResourceState::ReadAccelerationStructure)
		}); //@note: it is important that there is a barrier between building of blas and tlas!
}

void PbrMesh::CompactBlas()
{
	//@todo:
}

void PbrMesh::Free()
{
	geometry.Free();
	Frame::SafeRelease(std::move(rayTracingBlas.resource));

	submeshes.Free();
	std::vector<Texture> textures;
	for (auto& texture : textures)
	{
		Frame::SafeRelease(std::move(texture.ptr));
	}

	Frame::SafeRelease(materialConstantsBuffer);
	Frame::SafeRelease(instanceDataBuffer);
	Frame::SafeRelease(submeshDataBuffer);

	instanceData.Free();
	instanceDataOffset = BufferHeap::InvalidOffset;
	instanceCount = 1;
	instanceDataPtr = nullptr;
}

void LoadMeshMaterials(ID3D12Device10* device, std::span<PbrMesh::MaterialConstants> materialConstants, std::vector<Texture>& textures, DescriptorHeap& descriptorHeap, const rapidobj::Materials& materials)
{
	assert(materialConstants.size() == 1 || materialConstants.size() == materials.size());
	for (int i = 0; i < materials.size(); i++)
	{
		const auto& material = materials[i];
		auto& materialConstant = materialConstants[i];
		if (material.diffuse_texname.compare("") != 0)
		{
			Texture&& texture = LoadTexture(AnsiToWString(material.diffuse_texname.c_str()).c_str(), device, descriptorHeap, ColorMode::ForceSRGB);
			materialConstant.albedoTextureId = texture.srvId;
			textures.push_back(std::move(texture));
		}

		if (material.bump_texname.compare("") != 0)
		{
			Texture&& texture = LoadTexture(AnsiToWString(material.bump_texname.c_str()).c_str(), device, descriptorHeap, ColorMode::ForceLinear);
			materialConstant.normalTextureId = texture.srvId;
			textures.push_back(std::move(texture));
		}

		if (material.roughness_texname.compare("") != 0)
		{
			Texture&& texture = LoadTexture(AnsiToWString(material.roughness_texname.c_str()).c_str(), device, descriptorHeap, ColorMode::ForceLinear);
			materialConstant.roughnessTextureId = texture.srvId;
			textures.push_back(std::move(texture));
		}

		if (material.metallic_texname.compare("") != 0)
		{
			Texture&& texture = LoadTexture(AnsiToWString(material.metallic_texname.c_str()).c_str(), device, descriptorHeap, ColorMode::ForceLinear);
			materialConstant.metallicTextureId = texture.srvId;
			textures.push_back(std::move(texture));
		}
	}
}

Geometry CreateGeometry(BufferHeap& heap,
	std::span<const uint32_t> indices,
	std::span<const DirectX::XMFLOAT3> positions,
	std::span<const DirectX::XMFLOAT3> normals,
	std::span<const DirectX::XMFLOAT2> uvs)
{
	Geometry geometryData
	{
		.indexCount = static_cast<uint32_t>(indices.size()),
		.vertexCount = static_cast<uint32_t>(positions.size())
	};
	assert(normals.size() == positions.size() && uvs.size() == positions.size());

	uint32_t indicesSizeBytes = static_cast<uint32_t>(indices.size_bytes());
	uint32_t positionsSizeBytes = static_cast<uint32_t>(positions.size_bytes());
	uint32_t normalsSizeBytes = static_cast<uint32_t>(normals .size_bytes());
	uint32_t uvsSizeBytes = static_cast<uint32_t>(uvs.size_bytes());

	//create GPU resources for index and vertex buffers
	const uint32_t totalRequiredMemoryBytes = indicesSizeBytes + positionsSizeBytes + normalsSizeBytes + uvsSizeBytes;
	geometryData.memory = heap.Allocate(totalRequiredMemoryBytes);

	uint32_t positionsBufferOffset = geometryData.memory.offset + indicesSizeBytes;
	uint32_t normalsBufferOffset = positionsBufferOffset + positionsSizeBytes;
	uint32_t uvBufferOffset = normalsBufferOffset + normalsSizeBytes;

	//copy to GPU
	heap.WriteRaw(geometryData.memory.offset, indices.data(), indicesSizeBytes);
	heap.WriteRaw(positionsBufferOffset, positions.data(), positionsSizeBytes);
	heap.WriteRaw(normalsBufferOffset, normals.data(), normalsSizeBytes);
	heap.WriteRaw(uvBufferOffset, uvs.data(), uvsSizeBytes);

	//Calculate geometry aabb
	DirectX::BoundingBox::CreateFromPoints(geometryData.aabb, geometryData.vertexCount, positions.data(), sizeof(DirectX::XMFLOAT3));

	return geometryData;
}

//helper struct to identify unique vertices from vertex data loaded from obj file. A vertex is implicitly defined by an index into each of the position, texture coordinate, and normal lists.
struct ImplicitVertex
{
	//original position of this Index in list of indices
	unsigned int originalIndexListPosition;
	// respective index into list of positions, texture coordinates, and normals, as stored in .obj file
	int position;
	int coordinate;
	int normal;

	//note: operator= and operator< do not include originalIndex, since originalIndex will always be different
	//test if index composed of position coordinate and normal is equal
	bool operator==(const ImplicitVertex& other) const
	{
		return position == other.position && coordinate == other.coordinate && normal == other.normal;
	}

	//defines an order for the indices
	bool operator<(const ImplicitVertex& other) const
	{
		//first try to sort by position index
		if (position != other.position)
			return position < other.position;
		//only if position indices are equal sort by texture coordinate index
		if (coordinate != other.coordinate)
			return coordinate < other.coordinate;
		//only if also texture coordinate indices are equal sort by normal index
		if (normal != other.normal)
			return normal < other.normal;
		//if all three components are equal this is not strictly smaller than other
		return false;
	}
};

void SetTemporaryInstanceData(PbrMesh& mesh, LinearAllocator& allocator, ScratchHeap& bufferHeap, const PbrMesh::InstanceData& instanceData)
{
	SetTemporaryInstanceData(mesh, allocator, bufferHeap, { &instanceData, 1 });
}

void SetTemporaryInstanceData(PbrMesh& mesh, LinearAllocator& allocator, ScratchHeap& bufferHeap, std::span<const PbrMesh::InstanceData> instanceData)
{
	mesh.instanceCount = static_cast<uint32_t>(instanceData.size());

	mesh.instanceDataPtr = WriteTemporaryData(allocator, instanceData);
	mesh.instanceDataOffset = WriteTemporaryData(bufferHeap, instanceData);
}

void InitPersistentInstanceData(PbrMesh& mesh, PersistentAllocator& allocator, BufferHeap& bufferHeap, std::span<PbrMesh::InstanceData> instanceData)
{
	mesh.instanceCount = static_cast<uint32_t>(instanceData.size());

	mesh.instanceDataBuffer = CreatePersistentBuffer<PbrMesh::InstanceData>(bufferHeap, mesh.instanceCount);
	mesh.instanceData = AllocatePersistentMemory<PbrMesh::InstanceData>(allocator, mesh.instanceCount);

	UpdatePersistentInstanceData(mesh, instanceData);
}

void UpdatePersistentInstanceData(PbrMesh& mesh, const PbrMesh::InstanceData& instanceData, uint32_t elementIndex)
{
	assert(elementIndex < mesh.instanceCount);
	assert(mesh.instanceData.allocation.IsValid() && mesh.instanceDataPtr != nullptr);

	mesh.instanceData.Get(elementIndex) = instanceData;

	mesh.instanceDataBuffer.Write(instanceData, elementIndex);
}

void UpdatePersistentInstanceData(PbrMesh& mesh, std::span<const PbrMesh::InstanceData> instanceData)
{
	assert(instanceData.size() == mesh.instanceCount);
	mesh.instanceDataBuffer.Write(instanceData);
	mesh.instanceDataOffset = mesh.instanceDataBuffer.Offset();

	mesh.instanceDataPtr = &mesh.instanceData.Get();
	std::memcpy(mesh.instanceDataPtr, instanceData.data(), instanceData.size_bytes());

}

PbrMesh LoadMesh(ID3D12Device10* device,
	PersistentAllocator& allocator,
	DescriptorHeap& descriptorHeap,
	BufferHeap& bufferHeap,
	LPCWSTR fileName)
{
	StackContext stackContext;
	//Load model using rapidobj library
	rapidobj::Result model;
	{
		model = rapidobj::ParseFile(fileName);
	}

	PbrMesh mesh;
	uint32_t submeshCount = static_cast<uint32_t>(model.shapes.size());
	mesh.submeshes = AllocatePersistentMemory<PbrMesh::Submesh>(allocator, submeshCount);
	mesh.geometry = LoadGeometryData(bufferHeap, model, { &mesh.submeshes.Get(), submeshCount });

	//@todo: at the moment, if several materials use the same texture the texture will be uploaded several times
	const uint32_t materialConstantsCount = Max(static_cast<uint32_t>(model.materials.size()), 1u);//always reserve at least on material constant element for PbrMeshes
	PbrMesh::MaterialConstants* materialConstants = stackContext.Allocate<PbrMesh::MaterialConstants>(materialConstantsCount);
	auto materialConstantsSpan = std::span{ materialConstants, materialConstantsCount };
	LoadMeshMaterials(device, materialConstantsSpan, mesh.textures, descriptorHeap, model.materials);
	mesh.materialConstantsBuffer = CreatePersistentBuffer<PbrMesh::MaterialConstants>(bufferHeap, materialConstantsCount); 
	mesh.materialConstantsBuffer.Write(materialConstantsSpan);

	mesh.submeshDataBuffer = CreatePersistentBuffer<PbrMesh::Submesh>(bufferHeap, submeshCount);

	for (uint32_t i = 0; i < submeshCount; i++)
	{
		PbrMesh::Submesh& submesh = mesh.submeshes.Get(i);
		submesh.materialConstantsOffset = submesh.materialConstantsOffset != BufferHeap::InvalidOffset ? mesh.materialConstantsBuffer.Offset(submesh.materialConstantsOffset) : BufferHeap::InvalidOffset;

		mesh.submeshDataBuffer.Write(submesh, i);
	}

	return mesh;
}

Geometry LoadGeometryData(BufferHeap& bufferHeap, LPCWSTR fileName)
{
	//Load model using rapidobj library
	rapidobj::Result model;
	{
		model = rapidobj::ParseFile(fileName);
	}

	return LoadGeometryData(bufferHeap, model);
}

void Free(PbrMesh& mesh)
{
	mesh.geometry.memory.Free();

	mesh.materialConstantsBuffer.Free();
	mesh.instanceDataBuffer.Free();
	mesh.submeshDataBuffer.Free();

	mesh.instanceData.Free();
	mesh.instanceDataOffset = BufferHeap::InvalidOffset;
	mesh.instanceDataPtr = nullptr;

	mesh.instanceCount = 1;
}

void SetMaterial(PbrMesh& mesh, const PbrMesh::MaterialConstants& material)
{
	assert(mesh.submeshes.Count() > 0);
	mesh.materialConstantsBuffer.Write({ { material} });
	PbrMesh::Submesh& submesh = mesh.submeshes.Get(0);
	submesh.materialConstantsOffset = mesh.materialConstantsBuffer.Offset();
	mesh.submeshDataBuffer.Write(submesh);
}

Geometry LoadGeometryData(BufferHeap& bufferHeap, const rapidobj::Result& model, std::span<PbrMesh::Submesh> submeshes)
{
	assert(submeshes.empty() || submeshes.size() == model.shapes.size());
	Geometry geometry;
	StackContext stackContext;
	DirectX::XMFLOAT3* positions = nullptr;
	DirectX::XMFLOAT3* normals = nullptr;
	DirectX::XMFLOAT2* uvs = nullptr;
	uint32_t* indices = nullptr;

	//For simplicity, determine total number of indices in all submeshes first
	uint32_t indexTotalCount = 0;
	for (int shapeIndex = 0; shapeIndex < model.shapes.size(); shapeIndex++)
	{
		const auto& loadedIndices = model.shapes[shapeIndex].mesh.m_indices;
		indexTotalCount += static_cast<uint32_t>(loadedIndices.size());
	}

	indices = stackContext.Allocate<uint32_t>(indexTotalCount);
	//Use indexCount as worst case estimate for size of vertex Buffers
	positions = stackContext.Allocate<DirectX::XMFLOAT3>(indexTotalCount);
	normals = stackContext.Allocate<DirectX::XMFLOAT3>(indexTotalCount);
	uvs = stackContext.Allocate<DirectX::XMFLOAT2>(indexTotalCount);

	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;

	for (int shapeIndex = 0; shapeIndex < model.shapes.size(); shapeIndex++)
	{
		//list of triangle indices determines assembly of primitves in mesh. However, index saved in .obj file is multicomponent index, where each component is an index into a separate respective list of positions, texture coordinates, and normals
		const auto& loadedIndices = model.shapes[shapeIndex].mesh.m_indices;

		PbrMesh::Submesh subMesh;
		subMesh.materialConstantsOffset = model.shapes[shapeIndex].mesh.material_ids.front(); //assumption: materialId constant for each shape
		subMesh.indexCount = static_cast<uint32_t>(loadedIndices.size());
		subMesh.startIndexLocation = indexCount;
		uint32_t baseVertexLocation = vertexCount;
		indexCount += subMesh.indexCount;

		const auto& loadedPositions = model.attributes.positions;
		const auto& loadedNormals = model.attributes.normals;
		const auto& loadedUvs = model.attributes.texcoords;

		//In order to use this with d3d api index buffer and vertex streams, unique vertices  (i.e. unique combination of position, texture coordinate, and normal indices) need to be identified

		//copy vertices defined by the three indices to own sortable helper struct, also remembering their original position in index list in order to rebuild topology correctly.
		ImplicitVertex* implicitVertices;
		implicitVertices = stackContext.Allocate<ImplicitVertex>(subMesh.indexCount);
		uint32_t implicitVertexCount = 0;
		for (unsigned int i = subMesh.startIndexLocation; i < indexCount; i++, implicitVertexCount++)
		{
			implicitVertices[i - subMesh.startIndexLocation] = { i, loadedIndices[i - subMesh.startIndexLocation].position_index, loadedIndices[i - subMesh.startIndexLocation].texcoord_index, loadedIndices[i - subMesh.startIndexLocation].normal_index };
		}

		//sort the list using Index::operator< for comparison to put duplicate vertices adjacent to one another
		std::sort(implicitVertices, implicitVertices + implicitVertexCount);

		//remove duplicates from list (more precisely: rebuild list without duplicates in place) and also build final index buffer holding the correct index of the unique vertex
		uint32_t uniqueVertexCount = 0;
		{
			//create index buffer with the same size as loaded index Buffer
			ImplicitVertex lastUniqueVertex = implicitVertices[0];
			uniqueVertexCount = 1;
			for (size_t i = 0; i < implicitVertexCount; i++)
			{
				ImplicitVertex& thisVertex = implicitVertices[i];
				//this vertex is unique, if it is different from the previous unique vertex
				if (lastUniqueVertex != thisVertex)
				{
					//uniqueVertexCount is always smaller or equal i, and elements with position <= i in indexVector are not needed any more and can be overwritten to build unique vertex vector in place
					implicitVertices[uniqueVertexCount++] = thisVertex;
					lastUniqueVertex = thisVertex;
				}
				//put index of the current unique vertex at correct position in index buffer
				indices[thisVertex.originalIndexListPosition] = baseVertexLocation + uniqueVertexCount - 1;
			}
			//index buffer is made up of only unique vertices
			vertexCount += uniqueVertexCount;
		}

		//now implicitVertices truly contains only unique vertices
		implicitVertexCount = uniqueVertexCount; 
		ImplicitVertex* uniqueVertices = implicitVertices;


		//use indices stored in unique vertices to load actual position, texture coordinate, and normal data from loaded lists.
		for (unsigned int i = 0; i < uniqueVertexCount; i++)
		{
			//The loaded lists are flat, thus  * 3,2 + 0,1,2
			positions[i + baseVertexLocation] = { loadedPositions[uniqueVertices[i].position * 3 + 0], loadedPositions[uniqueVertices[i].position * 3 + 1], loadedPositions[uniqueVertices[i].position * 3 + 2] };
			normals[i + baseVertexLocation] = { loadedNormals[uniqueVertices[i].normal * 3 + 0], loadedNormals[uniqueVertices[i].normal * 3 + 1], loadedNormals[uniqueVertices[i].normal * 3 + 2] };
			uvs[i + baseVertexLocation] = { loadedUvs[uniqueVertices[i].coordinate * 2 + 0], 1.0f - loadedUvs[uniqueVertices[i].coordinate * 2 + 1] }; //need to do 1.0f - v, because of in d3d v axis points down but in blender up (?)
		}

		if (!submeshes.empty())
		{
			submeshes[shapeIndex] = subMesh;
		}
	}

	//CalculateTangentFrame();
	geometry = CreateGeometry(bufferHeap, { indices, indexCount }, { positions, vertexCount }, { normals, vertexCount }, { uvs, vertexCount });

	return geometry;
}

D3D12_RAYTRACING_GEOMETRY_DESC GetRaytracingGeometryDesc(const Geometry & geometry, RaytracingGeometryType type)
{
	return {
		.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
		.Flags = type == RaytracingGeometryType::Opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE :  D3D12_RAYTRACING_GEOMETRY_FLAG_NONE,
		.Triangles =
		{
			.Transform3x4 = 0,
			.IndexFormat = geometry.indexBufferFormat,
			.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
			.IndexCount = geometry.indexCount,
			.VertexCount = geometry.vertexCount,
			.IndexBuffer = geometry.GetIndexBufferAddress(),
			.VertexBuffer =
			{
				.StartAddress = geometry.GetVertexBuffersAddress(),
				.StrideInBytes = sizeof(DirectX::XMFLOAT3),
			}
		}
	};
}

void GenerateGeometryDescs(const PbrMesh& mesh, std::span<D3D12_RAYTRACING_GEOMETRY_DESC> outGeometryDescs, D3D12_GPU_VIRTUAL_ADDRESS transformAddress)
{
	assert(mesh.submeshes.Count() == outGeometryDescs.size());
	for (uint32_t i = 0; i < mesh.submeshes.Count(); i++)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC& geometryDesc = outGeometryDescs[i];
		const PbrMesh::Submesh& submesh = mesh.submeshes.Get(i);
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = mesh.geometry.GetIndexBufferAddress() + submesh.startIndexLocation * sizeof(mesh.geometry.indexBufferFormat);
		geometryDesc.Triangles.IndexFormat = mesh.geometry.indexBufferFormat;
		geometryDesc.Triangles.IndexCount = submesh.indexCount;
		geometryDesc.Triangles.VertexBuffer.StartAddress = mesh.geometry.GetVertexBuffersAddress();
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(DirectX::XMFLOAT3);
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDesc.Triangles.VertexCount = mesh.geometry.vertexCount;

		assert(IsAlignedTo(transformAddress, 16));
		geometryDesc.Triangles.Transform3x4 = transformAddress;
		geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
		//geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; //@todo: submeshes with no alpha test required should be opaque
	}
}

RaytracingInstanceGeometryData GetRaytracingInstanceGeometryData(const PbrMesh& mesh)
{
	return
	{
		.submeshDataOffset = mesh.submeshDataBuffer.Offset(),
		.indexBufferOffset = mesh.geometry.GetIndexBufferOffset(),
		.vertexBufferOffset = mesh.geometry.GetVertexBuffersOffset(),
		.vertexCount = mesh.geometry.vertexCount,
		.transformOffset = mesh.instanceDataOffset
	};
}

template <template<typename> typename BufferType> // PersistentBuffer or TemporaryBuffer
static void WriteTlasInstanceData(BufferType<D3D12_RAYTRACING_INSTANCE_DESC>& instanceBuffer, BufferType<RaytracingInstanceGeometryData>& instanceGeometryDataBuffer, const RaytracingInstanceGeometryData& instanceGeometryData, const DirectX::XMFLOAT4X4& transform, D3D12_GPU_VIRTUAL_ADDRESS asAddress, uint32_t elementIndex)
{
	instanceGeometryDataBuffer.Write(instanceGeometryData, elementIndex);

	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc =
	{
		.Transform = { { transform._11, transform._12, transform._13, transform._14 },
		{ transform._21, transform._22, transform._23, transform._24 },
		{ transform._31, transform._32, transform._33, transform._34 } },
		.InstanceID = elementIndex,
		.InstanceMask = 1, 
		.AccelerationStructure = asAddress
	};

	instanceBuffer.Write(instanceDesc, elementIndex);
}

static uint32_t CountInstances(std::span<const PbrMesh*> meshes)
{
	uint32_t instanceCount = 0;
	for (const auto* mesh : meshes)
	{
		instanceCount += mesh->instanceCount;
	}

	return instanceCount;
}

template <template<typename> typename BufferType> // PersistentBuffer or TemporaryBuffer
static void BuildTlasHelper(BufferResource& accelerationStructureBuffer,
	ID3D12Device10* device,
	ID3D12GraphicsCommandList10* commandList,
	BufferType<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDataBuffer,
	BufferType<RaytracingInstanceGeometryData>& instanceGeometryDataBuffer,
	const RWBufferResource& scratchBuffer,
	std::span<const PbrMesh*> meshes)
{
	uint32_t instanceCount = 0;
	for (const auto* mesh : meshes)
	{
		if (!mesh->rayTracingBlas.resource)
		{
			assert(false);
			continue;
		}

		for (uint32_t i = 0; i < mesh->instanceCount; i++)
		{
			WriteTlasInstanceData(instanceDataBuffer, instanceGeometryDataBuffer, GetRaytracingInstanceGeometryData(*mesh), mesh->GetInstanceData(i).transforms, mesh->rayTracingBlas.resource->GetGPUVirtualAddress(), instanceCount);
			instanceCount++;
		}
	}

	BuildAccelerationStructure(accelerationStructureBuffer, device, commandList, GetTlasInputs(instanceDataBuffer.GPUAddress(), instanceCount), scratchBuffer, L"TLAS");

	ResourceTransitions(commandList, { accelerationStructureBuffer.Barrier(ResourceState::Any, ResourceState::RayTracing) });
}


void PersistentTlas::Build(ID3D12Device10* device,
	ID3D12GraphicsCommandList10* commandList,
	DescriptorHeap& descriptorHeap,
	BufferHeap& bufferHeap,
	const RWBufferResource& scratchBuffer,
	std::span<const PbrMesh*> meshes)
{
	uint32_t instanceCount = CountInstances(meshes);

	Frame::SafeRelease(instanceDataBuffer);
	instanceDataBuffer = CreatePersistentBuffer<D3D12_RAYTRACING_INSTANCE_DESC>(bufferHeap, instanceCount, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);

	Frame::SafeRelease(instanceGeometryDataBuffer);
	instanceGeometryDataBuffer = CreatePersistentBuffer<RaytracingInstanceGeometryData>(bufferHeap, instanceCount);

	BuildTlasHelper(accelerationStructureBuffer, device, commandList, instanceDataBuffer, instanceGeometryDataBuffer, scratchBuffer, meshes);

	Frame::SafeRelease(srvId);
	srvId = CreateSrvOnHeap(descriptorHeap, nullptr, GetBufferAccelerationStructureSrvDesc(accelerationStructureBuffer.resource->GetGPUVirtualAddress()));
}

void TemporaryTlas::Build(ID3D12Device10* device,
	ID3D12GraphicsCommandList10* commandList,
	TemporaryDescriptorHeap& descriptorHeap,
	ScratchHeap& bufferHeap,
	const RWBufferResource& scratchBuffer,
	std::span<const PbrMesh*> meshes)
{
	uint32_t instanceCount = CountInstances(meshes);

	TemporaryBuffer instanceDataBuffer = CreateTemporaryBuffer<D3D12_RAYTRACING_INSTANCE_DESC>(bufferHeap, instanceCount, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);

	TemporaryBuffer instanceGeometryDataBuffer = CreateTemporaryBuffer<RaytracingInstanceGeometryData>(bufferHeap, instanceCount);
	instanceGeometryDataOffset = instanceGeometryDataBuffer.offset;

	BuildTlasHelper(accelerationStructureBuffer, device, commandList, instanceDataBuffer, instanceGeometryDataBuffer, scratchBuffer, meshes);

	srvId = CreateSrvOnHeap(descriptorHeap, nullptr, GetBufferAccelerationStructureSrvDesc(accelerationStructureBuffer.resource->GetGPUVirtualAddress()));
}

