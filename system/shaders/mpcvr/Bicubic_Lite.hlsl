// Bicubic 插值算法
// 移植自 https://github.com/libretro/common-shaders/blob/master/bicubic/shaders/bicubic-normal.cg

//!MPC SCALER
//!VERSION 1
//!SCALER_TYPE UPSCALER
//!DESCRIPTION bicubic lite same as bicubic but smoother

//!CONSTANT
//!VALUE INPUT_PT_X
float inputPtX;

//!CONSTANT
//!VALUE INPUT_PT_Y
float inputPtY;


//!CONSTANT
//!LABEL Sharper arg 1
//!DEFAULT 0.333333
//!MIN 0
//!MAX 1

float paramB;

//!CONSTANT
//!LABEL Sharper arg 2
//!DEFAULT 0.333333
//!MIN 0
//!MAX 1

float paramC;

//!TEXTURE
Texture2D INPUT;

//!SAMPLER
//!FILTER POINT
SamplerState sam;


//!PASS 1
//!BIND INPUT


min16float weight(min16float x, min16float B, min16float C) {
	min16float ax = abs(x);

	if (ax < 1.0) {
		return (x * x * ((12.0 - 9.0 * B - 6.0 * C) * ax + (-18.0 + 12.0 * B + 6.0 * C)) + (6.0 - 2.0 * B)) / 6.0;
	} else if (ax >= 1.0 && ax < 2.0) {
		return (x * x * ((-B - 6.0 * C) * ax + (6.0 * B + 30.0 * C)) + (-12.0 * B - 48.0 * C) * ax + (8.0 * B + 24.0 * C)) / 6.0;
	} else {
		return 0.0;
	}
}

min16float4 weight4(min16float x) {
	min16float B = paramB;
	min16float C = paramC;


	return min16float4(
		weight(x - 2.0, B, C),
		weight(x - 1.0, B, C),
		weight(x, B, C),
		weight(x + 1.0, B, C)
	);
}

float3 line_run(float ypos, float4 xpos, float4 linetaps) {
	return INPUT.Sample(sam, float2(xpos.r, ypos)).rgb * linetaps.r
		+ INPUT.Sample(sam, float2(xpos.g, ypos)).rgb * linetaps.g
		+ INPUT.Sample(sam, float2(xpos.b, ypos)).rgb * linetaps.b
		+ INPUT.Sample(sam, float2(xpos.a, ypos)).rgb * linetaps.a;
}


float4 Pass1(float2 pos) {
	float2 f = frac(pos / float2(inputPtX, inputPtY) + 0.5);

	min16float4 linetaps = weight4(min16float(1.0 - f.x));
	min16float4 columntaps = weight4(min16float(1.0 - f.y));

	// make sure all taps added together is exactly 1.0, otherwise some (very small) distortion can occur
	linetaps /= linetaps.r + linetaps.g + linetaps.b + linetaps.a;
	columntaps /= columntaps.r + columntaps.g + columntaps.b + columntaps.a;

	// !!!change the current coordinates
	pos -= (f + 1) * float2(inputPtX, inputPtY);

	float4 xpos = float4(pos.x, pos.x + inputPtX, pos.x + 2 * inputPtX, pos.x + 3 * inputPtX);


	float4 linetapsF = float4(linetaps);
	float4 columntapsF = float4(columntaps);
	// final sum and weight normalization
	return float4(line_run(pos.y, xpos, linetapsF) * columntapsF.r
		+ line_run(pos.y + inputPtY, xpos, linetapsF) * columntapsF.g
		+ line_run(pos.y + 2 * inputPtY, xpos, linetapsF) * columntapsF.b
		+ line_run(pos.y + 3 * inputPtY, xpos, linetapsF) * columntapsF.a,
		1);
}
