/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023
 * Martin Lambers <marlam@marlam.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

uniform sampler2D view0;
uniform sampler2D view1;
uniform float relativeWidth;
uniform float relativeHeight;
uniform bool nativeSamples;
uniform int nativeOffsetX;
uniform int outputModeLeftRightView;
smooth in vec2 vtexcoord;
layout(location = 0) out vec4 fcolor;

float to_nonlinear(float x)
{
    return x <= 0.0031308 ? x * 12.92 : 1.055 * pow(x, 0.416666666667) - 0.055;
}
void main()
{
    vec2 position = (vtexcoord - 0.5 * (vec2(1.0) - vec2(relativeWidth, relativeHeight)))
            / vec2(relativeWidth, relativeHeight);
    vec3 rgb = vec3(0.0);
    if (nativeSamples) {
        ivec2 pixel = ivec2(gl_FragCoord.xy) - ivec2(nativeOffsetX, 0);
        rgb = outputModeLeftRightView == 0 ? texelFetch(view0, pixel, 0).rgb : texelFetch(view1, pixel, 0).rgb;
    } else if (all(greaterThanEqual(position, vec2(0.0))) && all(lessThanEqual(position, vec2(1.0)))) {
        rgb = outputModeLeftRightView == 0 ? texture(view0, position).rgb : texture(view1, position).rgb;
    }
    fcolor = vec4(to_nonlinear(rgb.r), to_nonlinear(rgb.g), to_nonlinear(rgb.b), 1.0);
}
