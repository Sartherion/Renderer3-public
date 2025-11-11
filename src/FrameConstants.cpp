#include "stdafx.h"
#include "FrameConstants.h"

#include "Texture.h"

RWTexture& TemporaryRWTexture::Get()
{
	return texture;
}

TemporaryRWTexture::operator RWTexture& ()
{
	return texture;
}
