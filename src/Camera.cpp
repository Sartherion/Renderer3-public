#include "stdafx.h"
#include "Camera.h"

#include "Input.h"
#include "Random.h"

using namespace DirectX;

static float LinearToNonlinearDepth(float linearDepth, float nearZ, float farZ);
static XMMATRIX XM_CALLCONV InvertViewMatrix(XMMATRIX viewMatrix);
static XMMATRIX XM_CALLCONV InvertProjectionMatrix(FXMMATRIX projectionMatrix);


Camera::Camera(float fieldOfViewAngleY, float aspectRatio, float nearZ, float farZ)
{
	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(fieldOfViewAngleY, aspectRatio, nearZ, farZ);
	XMStoreFloat4x4(&mProjectionMatrix, projectionMatrix);
	XMStoreFloat4x4(&mInverseProjectionMatrix, InvertProjectionMatrix(projectionMatrix));

	mFrustumData.nearZ = nearZ;
	mFrustumData.farZ = farZ;

	projectionMatrix = XMMatrixTranspose(projectionMatrix);
	XMVECTOR planeLeft = projectionMatrix.r[3] + projectionMatrix.r[0];
	planeLeft = planeLeft * XMVectorReciprocal(XMVector3Length(planeLeft));
	XMStoreFloat3(&mFrustumData.planeLeftNormalVS, planeLeft);

	XMVECTOR planeTop = projectionMatrix.r[3] - projectionMatrix.r[1];
	planeTop = planeTop * XMVectorReciprocal(XMVector3Length(planeTop));
	XMStoreFloat3(&mFrustumData.planeTopNormalVS, planeTop);
}

void Camera::Move(float forward, float up, float left)
{
	const XMVECTOR forwardDirection = XMLoadFloat4(&mForwardDirection);
	const XMVECTOR leftDirection = XMLoadFloat4(&mLeftDirection);
	const XMVECTOR upDirection = XMLoadFloat4(&mUpDirection);
	XMVECTOR position = XMLoadFloat3(&mPosition);

	position += forwardDirection * forward;
	position += upDirection * up;
	position += leftDirection * left;

	XMStoreFloat3(&mPosition, position);
}

void Camera::Rotate(float pitch, float yaw, float roll)
{
	XMVECTOR forwardDirection = XMLoadFloat4(&mForwardDirection);
	XMVECTOR leftDirection = XMLoadFloat4(&mLeftDirection);
	XMVECTOR upDirection = XMLoadFloat4(&mUpDirection);

	XMMATRIX rotationMatrix = XMMatrixRotationAxis(leftDirection, pitch) * XMMatrixRotationAxis(upDirection, yaw) * XMMatrixRotationAxis(forwardDirection, roll);
	forwardDirection = XMVector3Normalize(XMVector3TransformNormal(forwardDirection, rotationMatrix));
	leftDirection = XMVector3Normalize(XMVector3TransformNormal(leftDirection, rotationMatrix));
	upDirection = XMVector3Cross(forwardDirection, leftDirection);

	XMStoreFloat4(&mForwardDirection, forwardDirection);
	XMStoreFloat4(&mLeftDirection, leftDirection);
	XMStoreFloat4(&mUpDirection, upDirection);
}

void Camera::SetPosition(DirectX::XMFLOAT3 position)
{
	mPosition = position;
}

void Camera::ResetRotation()
{
	mForwardDirection = { 0.0f, 0.0f, 1.0f, 0.0f };
	mLeftDirection = { -1.0f, 0.0f, 0.0f, 0.0f }; //use a left-handed coordinate system
	mUpDirection = { 0.0f, 1.0f, 0.0f, 0.0f };
}

void Camera::Update(const Transform& transform, XMFLOAT2 jitter)
{
	Rotate(transform.pitch, transform.yaw, transform.roll);
	Move(transform.moveForward, transform.moveUp, transform.moveLeft);

	if (transform.HasMoved())
	{
		staticFramesCount = 0;

	}
	else
	{
		staticFramesCount++;
	}

	constants.Flip();

	Constants& current = constants.Current();
	current.cameraPosition = mPosition;

	const XMVECTOR forwardDirection = XMLoadFloat4(&mForwardDirection);
	const XMVECTOR leftDirection = XMLoadFloat4(&mLeftDirection);
	const XMVECTOR upDirection = XMLoadFloat4(&mUpDirection);
	const XMVECTOR position = XMLoadFloat3(&mPosition);
	XMMATRIX projectionMatrix = XMLoadFloat4x4(&mProjectionMatrix);

	XMMATRIX jitterMatrix = XMMatrixTranslation(jitter.x, jitter.y, 0.0f);

	projectionMatrix = XMMatrixMultiply(projectionMatrix, jitterMatrix);

	XMMATRIX viewMatrix = XMMatrixLookToLH(position, forwardDirection, upDirection);
	XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

	XMStoreFloat4x4(&current.viewProjectionMatrix, XMMatrixTranspose(viewProjectionMatrix));
	XMStoreFloat4x4(&current.viewMatrix, XMMatrixTranspose(viewMatrix));
	XMStoreFloat4x4(&current.projectionMatrix, XMMatrixTranspose(projectionMatrix));

	XMMATRIX inverseViewMatrix = InvertViewMatrix(viewMatrix);
	XMMATRIX inverseProjectionMatrix = InvertProjectionMatrix(projectionMatrix);
	XMMATRIX inverseViewProjectionMatrix = inverseProjectionMatrix * inverseViewMatrix;


	XMStoreFloat4x4(&current.inverseViewProjectionMatrix, XMMatrixTranspose(inverseViewProjectionMatrix));
	XMStoreFloat4x4(&current.inverseViewMatrix, XMMatrixTranspose(inverseViewMatrix));
	XMStoreFloat4x4(&current.inverseProjectionMatrix, XMMatrixTranspose(inverseProjectionMatrix));

	current.frustumData = mFrustumData;
	current.subPixelJitter = { jitter.x, jitter.y };
}

void Camera::ComputeViewSpaceSubFrustaBoundingBoxes(std::span<const float> subfrustaRatios,
	std::span<DirectX::BoundingSphere>boundingSpheres,
	std::span<DirectX::BoundingBox>boundingBoxes) const
{
	assert(subfrustaRatios.size() == boundingBoxes.size());
	const size_t subfrustaCount = subfrustaRatios.size();
	using namespace DirectX;
	XMMATRIX inverseProjectionMatrix = XMLoadFloat4x4(&mInverseProjectionMatrix);
	for (size_t i = 0; i < subfrustaCount; i++)
	{
		float subFrustumFarZ = LinearToNonlinearDepth(subfrustaRatios[i] * mFrustumData.farZ);
		XMVECTORF32 ndcVertices[8] =
		{
			{-1.0f, 1.0f, 0.0f, 1.0f},
			{1.0f, 1.0f, 0.0f, 1.0f},
			{-1.0f, -1.0f, 0.0f, 1.0f},
			{1.0f, -1.0f, 0.0f, 1.0f},
			{-1.0f, 1.0f, subFrustumFarZ, 1.0f},
			{1.0f, 1.0f, subFrustumFarZ, 1.0f},
			{-1.0f, -1.0f, subFrustumFarZ, 1.0f},
			{1.0f, -1.0f, subFrustumFarZ, 1.0f},
		};

		XMFLOAT3 vsVertices[8];
		for (int j = 0; j < 8; j++)
		{
			XMVECTOR vsVertex = XMVector4Transform(ndcVertices[j], inverseProjectionMatrix);
			float w = XMVectorGetW(vsVertex);
			vsVertex *= 1.0f / w;
			XMStoreFloat3(&vsVertices[j], vsVertex);
		}
		//BoundingSphere boundingSphere;
		BoundingSphere::CreateFromPoints(boundingSpheres[i], 8, vsVertices, sizeof(XMFLOAT3));
		
		BoundingBox::CreateFromPoints(boundingBoxes[i], 8, vsVertices, sizeof(XMFLOAT3));
	}
}

// inversion of view matrices is simpler than general case
XMMATRIX XM_CALLCONV InvertViewMatrix(XMMATRIX viewMatrix) 
{
	XMVECTOR translation = viewMatrix.r[3];
	viewMatrix.r[3] = XMVectorSet(0.0f, 0.0f, 0.0, 1.0f);
	float u = XMVectorGetX(XMVector4Dot(-translation, viewMatrix.r[0]));
	float v = XMVectorGetX(XMVector4Dot(-translation, viewMatrix.r[1]));
	float w = XMVectorGetX(XMVector4Dot(-translation, viewMatrix.r[2]));
	translation = XMVectorSet(u, v, w, 1.0f);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	viewMatrix.r[3] = translation;
	return viewMatrix;
}

// inversion of projection matrices is simpler than general case
XMMATRIX XM_CALLCONV InvertProjectionMatrix(FXMMATRIX projectionMatrix) 
{
	XMFLOAT4X4 inverseProjection{}; //value initialization
	XMFLOAT4X4 projection;
	XMStoreFloat4x4(&projection, projectionMatrix);
	inverseProjection._11 = 1.0f / projection._11;
	inverseProjection._22 = 1.0f / projection._22;
	inverseProjection._34 = 1.0f / projection._43;
	inverseProjection._41 = -projection._31 * inverseProjection._11;
	inverseProjection._42 = -projection._32 * inverseProjection._22;
	inverseProjection._43 = 1.0f;
	inverseProjection._44 = -projection._33 / projection._43;

	return XMLoadFloat4x4(&inverseProjection);
}

float LinearToNonlinearDepth(float linearDepth, float nearZ, float farZ)
{
	const float a = farZ / (farZ - nearZ);
	const float b = -a * nearZ;
    return  a + b / linearDepth;
}

Camera::Transform ProcessInput(float deltaTime, bool bCaptureMouse, const MovementSpeed& controler)
{
	Camera::Transform returnValue{};
	if (Input::IsPressed(MouseButton::Left))
	{
		if (bCaptureMouse)
		{
			float dx, dy;
			Input::GetMouseDelta(dx, dy);
			returnValue.pitch = controler.pitchSpeed * dy ;
			returnValue.yaw = controler.yawSpeed * dx;
		}
	}

	const float displacement = controler.moveSpeed * deltaTime;

	returnValue.roll = controler.rollSpeed * deltaTime * Input::GetTwoWayAction('F', 'G');

	returnValue.moveForward = displacement * Input::GetTwoWayAction('W', 'S');
	returnValue.moveUp = displacement * Input::GetTwoWayAction('Q', 'E');
	returnValue.moveLeft = displacement * Input::GetTwoWayAction('D', 'A');

	return returnValue;
}

DirectX::XMFLOAT2 HaltonSubPixelJitter(uint32_t width, uint32_t height, uint64_t frameId)
{
	return 
	{
		0.5f * (2 * HaltonSequence(2, 1 + frameId % 8) - 1) / width,
		0.5f * (2 * HaltonSequence(3, 1 + frameId % 8) - 1) / height
	};
}
