//!MPC SCALER
//!VERSION 1
//!SCALER_TYPE UPSCALER
//!DESCRIPTION Nearest Linear upscaler

//!TEXTURE
Texture2D INPUT;

//!SAMPLER
//!FILTER LINEAR
SamplerState sam;


//!PASS 1
//!BIND INPUT

float4 Pass1(float2 pos) {
	return INPUT.Sample(sam, pos);
}