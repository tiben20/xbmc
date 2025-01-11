/*
 *  Copyright (C) 2010-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
 
#version 150

in vec2 m_attrpos;
in vec4 m_attrcol;
in vec2 m_attrcord0;
in vec2 m_attrcord1;
out vec2 m_cord0;
out vec2 m_cord1;
out vec4 m_colour;
uniform mat4 m_proj;
uniform mat4 m_model;
uniform float m_depth;

void main()
{
  mat4 mvp = m_proj * m_model;
  gl_Position = mvp * vec4(m_attrpos, 0., 1.);
  gl_Position.z = m_depth * gl_Position.w;
  m_colour = m_attrcol;
  m_cord0 = m_attrcord0;
  m_cord1 = m_attrcord1;
}
