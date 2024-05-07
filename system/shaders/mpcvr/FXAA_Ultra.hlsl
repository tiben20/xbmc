// 移植自 https://github.com/libretro/slang-shaders/blob/master/anti-aliasing/shaders/fxaa.slang

//!MPC SCALER
//!VERSION 1
//!SCALER_TYPE POST
//!OUTPUT_WIDTH INPUT_WIDTH
//!OUTPUT_HEIGHT INPUT_HEIGHT
//!DESCRIPTION FXAA ULTRA

//!CONSTANT
//!VALUE INPUT_PT_X
float inputPtX;

//!CONSTANT
//!VALUE INPUT_PT_Y
float inputPtY;


//!TEXTURE
Texture2D INPUT;

//!SAMPLER
//!FILTER LINEAR
SamplerState sam;


//!PASS 1
//!BIND INPUT

#define FXAA_PRESET 5
#include "FXAA.hlsli"


float4 Pass1(float2 pos) {
	return FXAA(INPUT, sam, pos, float2(inputPtX, inputPtY));
}
