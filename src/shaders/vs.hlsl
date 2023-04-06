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

struct VS_INPUT
{
    float4 Position         : POSITION;
    float2 UV               : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Position         : SV_POSITION;
    float2 UV               : TEXCOORD;
};

VS_OUTPUT VS_Main(VS_INPUT input)
{
    return input;
}
