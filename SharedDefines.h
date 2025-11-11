#pragma once
static const float omnidirectionalShadowMapNearZ = 0.1f;

static const int clusteredShadingTileSizeX = 32;
static const int clusteredShadingTileSizeY = 32; 
static const int clusterCountZ = 64;

static const int bufferClearThreadGroupSize = 32;

static const int textureClearThreadGroupSizeX = 8;
static const int textureClearThreadGroupSizeY = 8;

static const int specularCubeMapsArrayMaxIndex = 255; // maximum index is also invalid index