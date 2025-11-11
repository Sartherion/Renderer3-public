#define universalRS "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED),"\
                 "RootConstants(num32BitConstants = 11, b0),"\
                 "SRV(t0, flags = DATA_VOLATILE),"\
                 "RootConstants(num32BitConstants = 1, b1),"\
                 "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_POINT, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"\
                 "StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_POINT, addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP),"\
                 "StaticSampler(s2, filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"\
                 "StaticSampler(s3, filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP),"\
                 "StaticSampler(s4, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"\
                 "StaticSampler(s5, filter = FILTER_ANISOTROPIC),"\
                 "StaticSampler(s6, filter = FILTER_ANISOTROPIC, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP),"\
                 "StaticSampler(s7, filter = FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP),"\
                 "StaticSampler(s8, filter = FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP),"\
                 "StaticSampler(s9, filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, addressU = TEXTURE_ADDRESS_BORDER, addressV = TEXTURE_ADDRESS_BORDER, addressW = TEXTURE_ADDRESS_BORDER, comparisonFunc = COMPARISON_LESS_EQUAL, borderColor = STATIC_BORDER_COLOR_OPAQUE_WHITE),"

                