#pragma once
const static int blurRadius = 5;
const static int kernelSize = blurRadius * 2 + 1;

static const float offsets[kernelSize] =
{
    -5.0f, -4.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f
};
static const float weights[kernelSize] =
{
    0.0117f, 0.0329f, 0.0714f, 0.1223f, 0.1682f, 0.1869f, 0.1682f, 0.1223f, 0.0714f, 0.0329f, 0.0117f
};
