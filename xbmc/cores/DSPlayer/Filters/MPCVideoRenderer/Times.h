/*
* (C) 2018 see Authors.txt
*
* This file is part of MPC-BE. But modified for Kodi
*
* Kodi is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* MPC-BE is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#pragma once

uint64_t GetPreciseTick();
uint64_t GetPreciseTicksPerSecondI();
double   GetPreciseTicksPerSecond();
double   GetPreciseSecondsPerTick();
