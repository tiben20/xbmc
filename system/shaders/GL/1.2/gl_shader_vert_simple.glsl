/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#version 120

attribute vec2 m_attrpos;
attribute vec4 m_attrcol;
attribute vec2 m_attrcord0;
attribute vec2 m_attrcord1;
varying vec2 m_cord0;
varying vec2 m_cord1;
varying vec4 m_colour;
uniform mat4 m_matrix;

void main()
{
  gl_Position = m_matrix * vec4(m_attrpos, 0., 1.);
  m_colour = m_attrcol;
  m_cord0 = m_attrcord0;
  m_cord1 = m_attrcord1;
}
