// This file is generated by the scripts available at https://github.com/hauuau/magpie-prescalers
// Please don't edit this file directly.
// Generated by: ravu.py --target luma --weights-file weights\ravu_weights-r3.py --float-format float16dx --use-compute-shader --use-magpie --overwrite
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

//!MAGPIE SHADER
//!VERSION 4

//!TEXTURE
Texture2D INPUT;

//!SAMPLER
//!FILTER POINT
SamplerState sam_INPUT;

//!TEXTURE
//!WIDTH  INPUT_WIDTH * 2
//!HEIGHT INPUT_HEIGHT * 2
Texture2D OUTPUT;

//!SAMPLER
//!FILTER LINEAR
SamplerState sam_INPUT_LINEAR;

//!TEXTURE
//!SOURCE ravu_lut3_f16.dds
//!FORMAT R16G16B16A16_FLOAT
Texture2D ravu_lut3;

//!SAMPLER
//!FILTER LINEAR
SamplerState sam_ravu_lut3;

//!TEXTURE
//!FORMAT R16_FLOAT
//!WIDTH  INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D ravu_int11;

//!SAMPLER
//!FILTER POINT
SamplerState sam_ravu_int11;

//!COMMON
#include "prescalers.hlsli"

#define LAST_PASS 2

//!PASS 1
//!DESC RAVU (step1, luma, r3, compute)
//!IN INPUT, ravu_lut3
//!OUT ravu_int11
//!BLOCK_SIZE 32, 8
//!NUM_THREADS 32, 8
shared float inp0[481];

#define CURRENT_PASS 1

#define GET_SAMPLE(x) dot(x.rgb, rgb2y)
#define imageStore(out_image, pos, val) imageStoreOverride(pos, val.x)
void imageStoreOverride(uint2 pos, float value) { ravu_int11[pos] = (value); }

#define INPUT_tex(pos) GET_SAMPLE(vec4(texture(INPUT, pos)))
static const float2 INPUT_size = float2(GetInputSize());
static const float2 INPUT_pt = float2(GetInputPt());

#define ravu_lut3_tex(pos) (vec4(texture(ravu_lut3, pos)))

#define HOOKED_tex(pos) INPUT_tex(pos)
#define HOOKED_size INPUT_size
#define HOOKED_pt INPUT_pt

void Pass1(uint2 blockStart, uint3 threadId) {
	ivec2 group_base = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize);
	int local_pos = int(gl_LocalInvocationID.x) * 13 + int(gl_LocalInvocationID.y);
	{
		for (int id = int(gl_LocalInvocationIndex); id < 481; id += int(gl_WorkGroupSize.x * gl_WorkGroupSize.y)) {
			uint x = (uint)id / 13, y = (uint)id % 13;
			inp0[id] =
				HOOKED_tex(HOOKED_pt * vec2(float(group_base.x + x) + (-1.5), float(group_base.y + y) + (-1.5))).x;
		}
	}
	barrier();
#if CURRENT_PASS == LAST_PASS
	uint2 destPos = blockStart + threadId.xy * 2;
	uint2 outputSize = GetOutputSize();
	if (destPos.x >= outputSize.x || destPos.y >= outputSize.y) {
		return;
	}
#endif
	{
		float luma6 = inp0[local_pos + 13];
		float luma7 = inp0[local_pos + 14];
		float luma8 = inp0[local_pos + 15];
		float luma9 = inp0[local_pos + 16];
		float luma10 = inp0[local_pos + 17];
		float luma11 = inp0[local_pos + 18];
		float luma1 = inp0[local_pos + 1];
		float luma12 = inp0[local_pos + 26];
		float luma13 = inp0[local_pos + 27];
		float luma14 = inp0[local_pos + 28];
		float luma15 = inp0[local_pos + 29];
		float luma2 = inp0[local_pos + 2];
		float luma16 = inp0[local_pos + 30];
		float luma17 = inp0[local_pos + 31];
		float luma18 = inp0[local_pos + 39];
		float luma3 = inp0[local_pos + 3];
		float luma19 = inp0[local_pos + 40];
		float luma20 = inp0[local_pos + 41];
		float luma21 = inp0[local_pos + 42];
		float luma22 = inp0[local_pos + 43];
		float luma23 = inp0[local_pos + 44];
		float luma4 = inp0[local_pos + 4];
		float luma24 = inp0[local_pos + 52];
		float luma25 = inp0[local_pos + 53];
		float luma26 = inp0[local_pos + 54];
		float luma27 = inp0[local_pos + 55];
		float luma28 = inp0[local_pos + 56];
		float luma29 = inp0[local_pos + 57];
		float luma31 = inp0[local_pos + 66];
		float luma32 = inp0[local_pos + 67];
		float luma33 = inp0[local_pos + 68];
		float luma34 = inp0[local_pos + 69];
		vec3 abd = vec3(0.0, 0.0, 0.0);
		float gx, gy;
		gx = (luma13 - luma1) / 2.0;
		gy = (luma8 - luma6) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (luma14 - luma2) / 2.0;
		gy = (-luma10 + 8.0 * luma9 - 8.0 * luma7 + luma6) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma15 - luma3) / 2.0;
		gy = (-luma11 + 8.0 * luma10 - 8.0 * luma8 + luma7) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma16 - luma4) / 2.0;
		gy = (luma11 - luma9) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (-luma25 + 8.0 * luma19 - 8.0 * luma7 + luma1) / 12.0;
		gy = (luma14 - luma12) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma26 + 8.0 * luma20 - 8.0 * luma8 + luma2) / 12.0;
		gy = (-luma16 + 8.0 * luma15 - 8.0 * luma13 + luma12) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma27 + 8.0 * luma21 - 8.0 * luma9 + luma3) / 12.0;
		gy = (-luma17 + 8.0 * luma16 - 8.0 * luma14 + luma13) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma28 + 8.0 * luma22 - 8.0 * luma10 + luma4) / 12.0;
		gy = (luma17 - luma15) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma31 + 8.0 * luma25 - 8.0 * luma13 + luma7) / 12.0;
		gy = (luma20 - luma18) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma32 + 8.0 * luma26 - 8.0 * luma14 + luma8) / 12.0;
		gy = (-luma22 + 8.0 * luma21 - 8.0 * luma19 + luma18) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma33 + 8.0 * luma27 - 8.0 * luma15 + luma9) / 12.0;
		gy = (-luma23 + 8.0 * luma22 - 8.0 * luma20 + luma19) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma34 + 8.0 * luma28 - 8.0 * luma16 + luma10) / 12.0;
		gy = (luma23 - luma21) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma31 - luma19) / 2.0;
		gy = (luma26 - luma24) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (luma32 - luma20) / 2.0;
		gy = (-luma28 + 8.0 * luma27 - 8.0 * luma25 + luma24) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma33 - luma21) / 2.0;
		gy = (-luma29 + 8.0 * luma28 - 8.0 * luma26 + luma25) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma34 - luma22) / 2.0;
		gy = (luma29 - luma27) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		float a = abd.x, b = abd.y, d = abd.z;
		float T = a + d, D = a * d - b * b;
		float delta = sqrt(max(T * T / 4.0 - D, 0.0));
		float L1 = T / 2.0 + delta, L2 = T / 2.0 - delta;
		float sqrtL1 = sqrt(L1), sqrtL2 = sqrt(L2);
		float theta = mix(mod(atan(L1 - a, b) + 3.141592653589793, 3.141592653589793), 0.0, abs(b) < 1.192092896e-7);
		float lambda = sqrtL1;
		float mu = mix((sqrtL1 - sqrtL2) / (sqrtL1 + sqrtL2), 0.0, sqrtL1 + sqrtL2 < 1.192092896e-7);
		float angle = floor(theta * 24.0 / 3.141592653589793);
		float strength = clamp(floor(log2(lambda * 2000.0 + 1.192092896e-7)), 0.0, 8.0);
		float coherence = mix(mix(0.0, 1.0, mu >= 0.25), 2.0, mu >= 0.5);
		float coord_y = ((angle * 9.0 + strength) * 3.0 + coherence + 0.5) / 648.0;
		float res = 0.0;
		vec4 w;
		w = texture(ravu_lut3, vec2(0.1, coord_y));
		res += (inp0[local_pos + 0] + inp0[local_pos + 70]) * w[0];
		res += (inp0[local_pos + 1] + inp0[local_pos + 69]) * w[1];
		res += (inp0[local_pos + 2] + inp0[local_pos + 68]) * w[2];
		res += (inp0[local_pos + 3] + inp0[local_pos + 67]) * w[3];
		w = texture(ravu_lut3, vec2(0.3, coord_y));
		res += (inp0[local_pos + 4] + inp0[local_pos + 66]) * w[0];
		res += (inp0[local_pos + 5] + inp0[local_pos + 65]) * w[1];
		res += (inp0[local_pos + 13] + inp0[local_pos + 57]) * w[2];
		res += (inp0[local_pos + 14] + inp0[local_pos + 56]) * w[3];
		w = texture(ravu_lut3, vec2(0.5, coord_y));
		res += (inp0[local_pos + 15] + inp0[local_pos + 55]) * w[0];
		res += (inp0[local_pos + 16] + inp0[local_pos + 54]) * w[1];
		res += (inp0[local_pos + 17] + inp0[local_pos + 53]) * w[2];
		res += (inp0[local_pos + 18] + inp0[local_pos + 52]) * w[3];
		w = texture(ravu_lut3, vec2(0.7, coord_y));
		res += (inp0[local_pos + 26] + inp0[local_pos + 44]) * w[0];
		res += (inp0[local_pos + 27] + inp0[local_pos + 43]) * w[1];
		res += (inp0[local_pos + 28] + inp0[local_pos + 42]) * w[2];
		res += (inp0[local_pos + 29] + inp0[local_pos + 41]) * w[3];
		w = texture(ravu_lut3, vec2(0.9, coord_y));
		res += (inp0[local_pos + 30] + inp0[local_pos + 40]) * w[0];
		res += (inp0[local_pos + 31] + inp0[local_pos + 39]) * w[1];
		res = clamp(res, 0.0, 1.0);
		imageStore(out_image, ivec2(gl_GlobalInvocationID), res);
	}
}
//!PASS 2
//!DESC RAVU (step2, luma, r3, compute)
//!IN INPUT, ravu_lut3, ravu_int11
//!OUT OUTPUT
//!BLOCK_SIZE 64, 16
//!NUM_THREADS 32, 8
shared float inp0[481];
shared float inp1[481];

#define CURRENT_PASS 2

#define GET_SAMPLE(x) dot(x.rgb, rgb2y)
#define imageStore(out_image, pos, val) imageStoreOverride(pos, val.x)
void imageStoreOverride(uint2 pos, float value) {
	float2 UV = mul(rgb2uv, INPUT.SampleLevel(sam_INPUT_LINEAR, HOOKED_map(pos), 0).rgb);
	OUTPUT[pos] = float4(mul(yuv2rgb, float3(value.x, UV)), 1.0);
}

#define INPUT_tex(pos) GET_SAMPLE(vec4(texture(INPUT, pos)))
static const float2 INPUT_size = float2(GetInputSize());
static const float2 INPUT_pt = float2(GetInputPt());

#define ravu_lut3_tex(pos) (vec4(texture(ravu_lut3, pos)))

#define ravu_int11_tex(pos) (float(texture(ravu_int11, pos).x))
static const float2 ravu_int11_size = float2(GetInputSize().x, GetInputSize().y);
static const float2 ravu_int11_pt = float2(1.0 / (ravu_int11_size.x), 1.0 / (ravu_int11_size.y));

#define HOOKED_tex(pos) INPUT_tex(pos)
#define HOOKED_size INPUT_size
#define HOOKED_pt INPUT_pt

void Pass2(uint2 blockStart, uint3 threadId) {
	ivec2 group_base = ivec2(gl_WorkGroupID) * ivec2(gl_WorkGroupSize);
	int local_pos = int(gl_LocalInvocationID.x) * 13 + int(gl_LocalInvocationID.y);
	{
		for (int id = int(gl_LocalInvocationIndex); id < 481; id += int(gl_WorkGroupSize.x * gl_WorkGroupSize.y)) {
			uint x = (uint)id / 13, y = (uint)id % 13;
			inp0[id] =
				ravu_int11_tex(ravu_int11_pt * vec2(float(group_base.x + x) + (-2.5), float(group_base.y + y) + (-2.5)))
					.x;
		}
	}
	{
		for (int id = int(gl_LocalInvocationIndex); id < 481; id += int(gl_WorkGroupSize.x * gl_WorkGroupSize.y)) {
			uint x = (uint)id / 13, y = (uint)id % 13;
			inp1[id] =
				HOOKED_tex(HOOKED_pt * vec2(float(group_base.x + x) + (-1.5), float(group_base.y + y) + (-1.5))).x;
		}
	}
	barrier();
#if CURRENT_PASS == LAST_PASS
	uint2 destPos = blockStart + threadId.xy * 2;
	uint2 outputSize = GetOutputSize();
	if (destPos.x >= outputSize.x || destPos.y >= outputSize.y) {
		return;
	}
#endif
	{
		float luma12 = inp0[local_pos + 15];
		float luma7 = inp0[local_pos + 16];
		float luma2 = inp0[local_pos + 17];
		float luma24 = inp0[local_pos + 27];
		float luma19 = inp0[local_pos + 28];
		float luma14 = inp0[local_pos + 29];
		float luma9 = inp0[local_pos + 30];
		float luma4 = inp0[local_pos + 31];
		float luma31 = inp0[local_pos + 40];
		float luma26 = inp0[local_pos + 41];
		float luma21 = inp0[local_pos + 42];
		float luma16 = inp0[local_pos + 43];
		float luma11 = inp0[local_pos + 44];
		float luma33 = inp0[local_pos + 54];
		float luma28 = inp0[local_pos + 55];
		float luma23 = inp0[local_pos + 56];
		float luma18 = inp1[local_pos + 14];
		float luma13 = inp1[local_pos + 15];
		float luma8 = inp1[local_pos + 16];
		float luma3 = inp1[local_pos + 17];
		float luma25 = inp1[local_pos + 27];
		float luma20 = inp1[local_pos + 28];
		float luma15 = inp1[local_pos + 29];
		float luma6 = inp1[local_pos + 2];
		float luma10 = inp1[local_pos + 30];
		float luma1 = inp1[local_pos + 3];
		float luma32 = inp1[local_pos + 40];
		float luma27 = inp1[local_pos + 41];
		float luma22 = inp1[local_pos + 42];
		float luma17 = inp1[local_pos + 43];
		float luma34 = inp1[local_pos + 54];
		float luma29 = inp1[local_pos + 55];
		vec3 abd = vec3(0.0, 0.0, 0.0);
		float gx, gy;
		gx = (luma13 - luma1) / 2.0;
		gy = (luma8 - luma6) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (luma14 - luma2) / 2.0;
		gy = (-luma10 + 8.0 * luma9 - 8.0 * luma7 + luma6) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma15 - luma3) / 2.0;
		gy = (-luma11 + 8.0 * luma10 - 8.0 * luma8 + luma7) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma16 - luma4) / 2.0;
		gy = (luma11 - luma9) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (-luma25 + 8.0 * luma19 - 8.0 * luma7 + luma1) / 12.0;
		gy = (luma14 - luma12) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma26 + 8.0 * luma20 - 8.0 * luma8 + luma2) / 12.0;
		gy = (-luma16 + 8.0 * luma15 - 8.0 * luma13 + luma12) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma27 + 8.0 * luma21 - 8.0 * luma9 + luma3) / 12.0;
		gy = (-luma17 + 8.0 * luma16 - 8.0 * luma14 + luma13) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma28 + 8.0 * luma22 - 8.0 * luma10 + luma4) / 12.0;
		gy = (luma17 - luma15) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma31 + 8.0 * luma25 - 8.0 * luma13 + luma7) / 12.0;
		gy = (luma20 - luma18) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma32 + 8.0 * luma26 - 8.0 * luma14 + luma8) / 12.0;
		gy = (-luma22 + 8.0 * luma21 - 8.0 * luma19 + luma18) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma33 + 8.0 * luma27 - 8.0 * luma15 + luma9) / 12.0;
		gy = (-luma23 + 8.0 * luma22 - 8.0 * luma20 + luma19) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma34 + 8.0 * luma28 - 8.0 * luma16 + luma10) / 12.0;
		gy = (luma23 - luma21) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma31 - luma19) / 2.0;
		gy = (luma26 - luma24) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (luma32 - luma20) / 2.0;
		gy = (-luma28 + 8.0 * luma27 - 8.0 * luma25 + luma24) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma33 - luma21) / 2.0;
		gy = (-luma29 + 8.0 * luma28 - 8.0 * luma26 + luma25) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma34 - luma22) / 2.0;
		gy = (luma29 - luma27) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		float a = abd.x, b = abd.y, d = abd.z;
		float T = a + d, D = a * d - b * b;
		float delta = sqrt(max(T * T / 4.0 - D, 0.0));
		float L1 = T / 2.0 + delta, L2 = T / 2.0 - delta;
		float sqrtL1 = sqrt(L1), sqrtL2 = sqrt(L2);
		float theta = mix(mod(atan(L1 - a, b) + 3.141592653589793, 3.141592653589793), 0.0, abs(b) < 1.192092896e-7);
		float lambda = sqrtL1;
		float mu = mix((sqrtL1 - sqrtL2) / (sqrtL1 + sqrtL2), 0.0, sqrtL1 + sqrtL2 < 1.192092896e-7);
		float angle = floor(theta * 24.0 / 3.141592653589793);
		float strength = clamp(floor(log2(lambda * 2000.0 + 1.192092896e-7)), 0.0, 8.0);
		float coherence = mix(mix(0.0, 1.0, mu >= 0.25), 2.0, mu >= 0.5);
		float coord_y = ((angle * 9.0 + strength) * 3.0 + coherence + 0.5) / 648.0;
		float res = 0.0;
		vec4 w;
		w = texture(ravu_lut3, vec2(0.1, coord_y));
		res += (inp0[local_pos + 3] + inp0[local_pos + 68]) * w[0];
		res += (inp1[local_pos + 3] + inp1[local_pos + 54]) * w[1];
		res += (inp0[local_pos + 17] + inp0[local_pos + 54]) * w[2];
		res += (inp1[local_pos + 17] + inp1[local_pos + 40]) * w[3];
		w = texture(ravu_lut3, vec2(0.3, coord_y));
		res += (inp0[local_pos + 31] + inp0[local_pos + 40]) * w[0];
		res += (inp1[local_pos + 31] + inp1[local_pos + 26]) * w[1];
		res += (inp1[local_pos + 2] + inp1[local_pos + 55]) * w[2];
		res += (inp0[local_pos + 16] + inp0[local_pos + 55]) * w[3];
		w = texture(ravu_lut3, vec2(0.5, coord_y));
		res += (inp1[local_pos + 16] + inp1[local_pos + 41]) * w[0];
		res += (inp0[local_pos + 30] + inp0[local_pos + 41]) * w[1];
		res += (inp1[local_pos + 30] + inp1[local_pos + 27]) * w[2];
		res += (inp0[local_pos + 44] + inp0[local_pos + 27]) * w[3];
		w = texture(ravu_lut3, vec2(0.7, coord_y));
		res += (inp0[local_pos + 15] + inp0[local_pos + 56]) * w[0];
		res += (inp1[local_pos + 15] + inp1[local_pos + 42]) * w[1];
		res += (inp0[local_pos + 29] + inp0[local_pos + 42]) * w[2];
		res += (inp1[local_pos + 29] + inp1[local_pos + 28]) * w[3];
		w = texture(ravu_lut3, vec2(0.9, coord_y));
		res += (inp0[local_pos + 43] + inp0[local_pos + 28]) * w[0];
		res += (inp1[local_pos + 43] + inp1[local_pos + 14]) * w[1];
		res = clamp(res, 0.0, 1.0);
		imageStore(out_image, ivec2(gl_GlobalInvocationID) * 2 + ivec2(0, 1), res);
	}
	{
		float luma6 = inp0[local_pos + 15];
		float luma1 = inp0[local_pos + 16];
		float luma18 = inp0[local_pos + 27];
		float luma13 = inp0[local_pos + 28];
		float luma8 = inp0[local_pos + 29];
		float luma3 = inp0[local_pos + 30];
		float luma25 = inp0[local_pos + 40];
		float luma20 = inp0[local_pos + 41];
		float luma15 = inp0[local_pos + 42];
		float luma10 = inp0[local_pos + 43];
		float luma32 = inp0[local_pos + 53];
		float luma27 = inp0[local_pos + 54];
		float luma22 = inp0[local_pos + 55];
		float luma17 = inp0[local_pos + 56];
		float luma34 = inp0[local_pos + 67];
		float luma29 = inp0[local_pos + 68];
		float luma12 = inp1[local_pos + 14];
		float luma7 = inp1[local_pos + 15];
		float luma2 = inp1[local_pos + 16];
		float luma24 = inp1[local_pos + 26];
		float luma19 = inp1[local_pos + 27];
		float luma14 = inp1[local_pos + 28];
		float luma9 = inp1[local_pos + 29];
		float luma4 = inp1[local_pos + 30];
		float luma31 = inp1[local_pos + 39];
		float luma26 = inp1[local_pos + 40];
		float luma21 = inp1[local_pos + 41];
		float luma16 = inp1[local_pos + 42];
		float luma11 = inp1[local_pos + 43];
		float luma33 = inp1[local_pos + 53];
		float luma28 = inp1[local_pos + 54];
		float luma23 = inp1[local_pos + 55];
		vec3 abd = vec3(0.0, 0.0, 0.0);
		float gx, gy;
		gx = (luma13 - luma1) / 2.0;
		gy = (luma8 - luma6) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (luma14 - luma2) / 2.0;
		gy = (-luma10 + 8.0 * luma9 - 8.0 * luma7 + luma6) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma15 - luma3) / 2.0;
		gy = (-luma11 + 8.0 * luma10 - 8.0 * luma8 + luma7) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma16 - luma4) / 2.0;
		gy = (luma11 - luma9) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (-luma25 + 8.0 * luma19 - 8.0 * luma7 + luma1) / 12.0;
		gy = (luma14 - luma12) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma26 + 8.0 * luma20 - 8.0 * luma8 + luma2) / 12.0;
		gy = (-luma16 + 8.0 * luma15 - 8.0 * luma13 + luma12) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma27 + 8.0 * luma21 - 8.0 * luma9 + luma3) / 12.0;
		gy = (-luma17 + 8.0 * luma16 - 8.0 * luma14 + luma13) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma28 + 8.0 * luma22 - 8.0 * luma10 + luma4) / 12.0;
		gy = (luma17 - luma15) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma31 + 8.0 * luma25 - 8.0 * luma13 + luma7) / 12.0;
		gy = (luma20 - luma18) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (-luma32 + 8.0 * luma26 - 8.0 * luma14 + luma8) / 12.0;
		gy = (-luma22 + 8.0 * luma21 - 8.0 * luma19 + luma18) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma33 + 8.0 * luma27 - 8.0 * luma15 + luma9) / 12.0;
		gy = (-luma23 + 8.0 * luma22 - 8.0 * luma20 + luma19) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.07901060453704994;
		gx = (-luma34 + 8.0 * luma28 - 8.0 * luma16 + luma10) / 12.0;
		gy = (luma23 - luma21) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma31 - luma19) / 2.0;
		gy = (luma26 - luma24) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		gx = (luma32 - luma20) / 2.0;
		gy = (-luma28 + 8.0 * luma27 - 8.0 * luma25 + luma24) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma33 - luma21) / 2.0;
		gy = (-luma29 + 8.0 * luma28 - 8.0 * luma26 + luma25) / 12.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.06153352068439959;
		gx = (luma34 - luma22) / 2.0;
		gy = (luma29 - luma27) / 2.0;
		abd += vec3(gx * gx, gx * gy, gy * gy) * 0.04792235409415088;
		float a = abd.x, b = abd.y, d = abd.z;
		float T = a + d, D = a * d - b * b;
		float delta = sqrt(max(T * T / 4.0 - D, 0.0));
		float L1 = T / 2.0 + delta, L2 = T / 2.0 - delta;
		float sqrtL1 = sqrt(L1), sqrtL2 = sqrt(L2);
		float theta = mix(mod(atan(L1 - a, b) + 3.141592653589793, 3.141592653589793), 0.0, abs(b) < 1.192092896e-7);
		float lambda = sqrtL1;
		float mu = mix((sqrtL1 - sqrtL2) / (sqrtL1 + sqrtL2), 0.0, sqrtL1 + sqrtL2 < 1.192092896e-7);
		float angle = floor(theta * 24.0 / 3.141592653589793);
		float strength = clamp(floor(log2(lambda * 2000.0 + 1.192092896e-7)), 0.0, 8.0);
		float coherence = mix(mix(0.0, 1.0, mu >= 0.25), 2.0, mu >= 0.5);
		float coord_y = ((angle * 9.0 + strength) * 3.0 + coherence + 0.5) / 648.0;
		float res = 0.0;
		vec4 w;
		w = texture(ravu_lut3, vec2(0.1, coord_y));
		res += (inp1[local_pos + 2] + inp1[local_pos + 67]) * w[0];
		res += (inp0[local_pos + 16] + inp0[local_pos + 67]) * w[1];
		res += (inp1[local_pos + 16] + inp1[local_pos + 53]) * w[2];
		res += (inp0[local_pos + 30] + inp0[local_pos + 53]) * w[3];
		w = texture(ravu_lut3, vec2(0.3, coord_y));
		res += (inp1[local_pos + 30] + inp1[local_pos + 39]) * w[0];
		res += (inp0[local_pos + 44] + inp0[local_pos + 39]) * w[1];
		res += (inp0[local_pos + 15] + inp0[local_pos + 68]) * w[2];
		res += (inp1[local_pos + 15] + inp1[local_pos + 54]) * w[3];
		w = texture(ravu_lut3, vec2(0.5, coord_y));
		res += (inp0[local_pos + 29] + inp0[local_pos + 54]) * w[0];
		res += (inp1[local_pos + 29] + inp1[local_pos + 40]) * w[1];
		res += (inp0[local_pos + 43] + inp0[local_pos + 40]) * w[2];
		res += (inp1[local_pos + 43] + inp1[local_pos + 26]) * w[3];
		w = texture(ravu_lut3, vec2(0.7, coord_y));
		res += (inp1[local_pos + 14] + inp1[local_pos + 55]) * w[0];
		res += (inp0[local_pos + 28] + inp0[local_pos + 55]) * w[1];
		res += (inp1[local_pos + 28] + inp1[local_pos + 41]) * w[2];
		res += (inp0[local_pos + 42] + inp0[local_pos + 41]) * w[3];
		w = texture(ravu_lut3, vec2(0.9, coord_y));
		res += (inp1[local_pos + 42] + inp1[local_pos + 27]) * w[0];
		res += (inp0[local_pos + 56] + inp0[local_pos + 27]) * w[1];
		res = clamp(res, 0.0, 1.0);
		imageStore(out_image, ivec2(gl_GlobalInvocationID) * 2 + ivec2(1, 0), res);
	}
	float res;
	res = inp0[local_pos + 42];
	imageStore(out_image, ivec2(gl_GlobalInvocationID) * 2 + ivec2(1, 1), res);
	res = inp1[local_pos + 28];
	imageStore(out_image, ivec2(gl_GlobalInvocationID) * 2 + ivec2(0, 0), res);
}
