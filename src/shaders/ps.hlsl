/*
    This file is part of Takoyaki, a custom screen share utility.

    Copyright (c) 2020-2023 Samuel Huang - All rights reserved.

    Takoyaki is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

Texture2D g_SharedTexture   : register( t0 );
SamplerState g_Sampler      : register( s0 );

struct PS_INPUT
{
    float4 Position         : SV_POSITION;
    float2 UV               : TEXCOORD;
};

float4 PS_Main(PS_INPUT input) : SV_Target
{
    return float4(1,0,0,1);
    return g_SharedTexture.Sample(g_Sampler, input.UV);
}

