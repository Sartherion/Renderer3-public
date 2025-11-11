#pragma once
const static int blurRadius = 3;
const static int kernelSize = blurRadius * 2 + 1;

static const float offsets[kernelSize] =
{
    -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f
};
static const float weights[kernelSize] =
{
    0.001, 0.028, 0.233, 0.474, 0.233, 0.028, 0.001 
};
