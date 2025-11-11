#pragma once
#include "D3DUtility.h"

class Camera
{
public:
	Camera(float fieldOfViewAngleY, float aspectRatio, float nearZ, float farZ); 
	Camera() = default;

	void SetPosition(DirectX::XMFLOAT3 position);
	void ResetRotation();

	struct Transform
	{
		float pitch;
		float yaw;
		float roll;
		float moveForward;
		float moveLeft;
		float moveUp;

		bool operator==(const Transform&) const = default;

		bool HasMoved() const
		{
			return *this != Transform{};
		}
	};

	struct FrustumData
	{
		DirectX::XMFLOAT3 planeTopNormalVS; //@note: frustum is assumed to be symmetric
		DirectX::XMFLOAT3 planeLeftNormalVS;
		float nearZ;
		float farZ;
	};

	struct Constants
	{
		DirectX::XMFLOAT3 cameraPosition; 
		DirectX::XMFLOAT4X4 viewProjectionMatrix;
		DirectX::XMFLOAT4X4 viewMatrix;
		DirectX::XMFLOAT4X4 projectionMatrix;
		DirectX::XMFLOAT4X4 inverseViewProjectionMatrix;
		DirectX::XMFLOAT4X4 inverseViewMatrix;
		DirectX::XMFLOAT4X4 inverseProjectionMatrix;
		FrustumData frustumData;
		DirectX::XMFLOAT2 subPixelJitter;
	};

	float LinearToNonlinearDepth(float linearDepth) const
	{
		const float a = mProjectionMatrix.m[2][2];
		const float b = mProjectionMatrix.m[3][2];
		return  a + b / linearDepth;
	}

	void Update(const Transform& transform, DirectX::XMFLOAT2 jitter = DirectX::XMFLOAT2{});

	uint32_t GetStaticFramesCount() const
	{
		return staticFramesCount;
	}

	void ComputeViewSpaceSubFrustaBoundingBoxes(std::span<const float> subfrustaRatios,
		std::span<DirectX::BoundingSphere>boundingSpheres,
		std::span<DirectX::BoundingBox>boundingBoxes) const;

	PingPong<Constants> constants;

private:
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 mForwardDirection = { 0.0f, 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT4 mLeftDirection = { 1.0f, 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 mUpDirection = { 0.0f, 1.0f, 0.0f, 0.0f };

	DirectX::XMFLOAT4X4 mProjectionMatrix;
	DirectX::XMFLOAT4X4 mInverseProjectionMatrix;

	uint32_t staticFramesCount = 0;

	FrustumData mFrustumData;

	void Move(float forward, float up, float left);
	void Rotate(float pitch, float yaw, float roll);
};

struct MovementSpeed
{
	float pitchSpeed = 0.001f;
	float yawSpeed = 0.001f;
	float rollSpeed = 0.005f;
	float moveSpeed = 0.005f;
};

Camera::Transform ProcessInput(float deltaTime, bool bCaptureMouse = true, const MovementSpeed& controler = {});

DirectX::XMFLOAT2 HaltonSubPixelJitter(uint32_t width, uint32_t height, uint64_t frameId);
