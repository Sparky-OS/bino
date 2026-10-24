/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025
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

#include <QCoreApplication>

#include "modes.hpp"


const char* inputModeToString(InputMode mode)
{
    switch (mode) {
    case Input_Unknown:
        return "unknown";
        break;
    case Input_Mono:
        return "mono";
        break;
    case Input_Top_Bottom:
        return "top-bottom";
        break;
    case Input_Top_Bottom_Half:
        return "top-bottom-half";
        break;
    case Input_Bottom_Top:
        return "bottom-top";
        break;
    case Input_Bottom_Top_Half:
        return "bottom-top-half";
        break;
    case Input_Left_Right:
        return "left-right";
        break;
    case Input_Left_Right_Half:
        return "left-right-half";
        break;
    case Input_Right_Left:
        return "right-left";
        break;
    case Input_Right_Left_Half:
        return "right-left-half";
        break;
    case Input_Alternating_LR:
        return "alternating-left-right";
        break;
    case Input_Alternating_RL:
        return "alternating-right-left";
        break;
    };
    return nullptr;
}

QString inputModeToStringUI(InputMode mode)
{
    switch (mode) {
    case Input_Unknown:
        return QCoreApplication::translate("Mode", "Input autodetection");
        break;
    case Input_Mono:
        return QCoreApplication::translate("Mode", "Input 2D");
        break;
    case Input_Top_Bottom:
        return QCoreApplication::translate("Mode", "Input top/bottom");
        break;
    case Input_Top_Bottom_Half:
        return QCoreApplication::translate("Mode", "Input top/bottom half height");
        break;
    case Input_Bottom_Top:
        return QCoreApplication::translate("Mode", "Input bottom/top");
        break;
    case Input_Bottom_Top_Half:
        return QCoreApplication::translate("Mode", "Input bottom/top half height");
        break;
    case Input_Left_Right:
        return QCoreApplication::translate("Mode", "Input left/right");
        break;
    case Input_Left_Right_Half:
        return QCoreApplication::translate("Mode", "Input left/right half width");
        break;
    case Input_Right_Left:
        return QCoreApplication::translate("Mode", "Input right/left");
        break;
    case Input_Right_Left_Half:
        return QCoreApplication::translate("Mode", "Input right/left half width");
        break;
    case Input_Alternating_LR:
        return QCoreApplication::translate("Mode", "Input alternating left/right");
        break;
    case Input_Alternating_RL:
        return QCoreApplication::translate("Mode", "Input alternating right/left");
        break;
    };
    return QString();
}

InputMode inputModeFromString(const QString& s, bool* ok)
{
    InputMode mode = Input_Unknown;
    bool r = true;
    if (s == "mono")
        mode = Input_Mono;
    else if (s == "top-bottom")
        mode = Input_Top_Bottom;
    else if (s == "top-bottom-half")
        mode = Input_Top_Bottom_Half;
    else if (s == "bottom-top")
        mode = Input_Bottom_Top;
    else if (s == "bottom-top-half")
        mode = Input_Bottom_Top_Half;
    else if (s == "left-right")
        mode = Input_Left_Right;
    else if (s == "left-right-half")
        mode = Input_Left_Right_Half;
    else if (s == "right-left")
        mode = Input_Right_Left;
    else if (s == "right-left-half")
        mode = Input_Right_Left_Half;
    else if (s == "alternating-left-right")
        mode = Input_Alternating_LR;
    else if (s == "alternating-right-left")
        mode = Input_Alternating_RL;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}

const char* surroundModeToString(SurroundMode mode)
{
    switch (mode) {
    case Surround_Unknown:
        return "unknown";
        break;
    case Surround_Off:
        return "off";
        break;
    case Surround_180:
        return "180";
        break;
    case Surround_360:
        return "360";
        break;
    }
    return nullptr;
}

QString surroundModeToStringUI(SurroundMode mode)
{
    switch (mode) {
    case Surround_Unknown:
        return QCoreApplication::translate("Mode", "Surround autodetection");
        break;
    case Surround_Off:
        return QCoreApplication::translate("Mode", "Surround off");
        break;
    case Surround_180:
        return QCoreApplication::translate("Mode", "Surround 180°");
        break;
    case Surround_360:
        return QCoreApplication::translate("Mode", "Surround 360°");
        break;
    }
    return QString();
}

SurroundMode surroundModeFromString(const QString& s, bool* ok)
{
    SurroundMode mode = Surround_Unknown;
    bool r = true;
    if (s == "off")
        mode = Surround_Off;
    else if (s == "180")
        mode = Surround_180;
    else if (s == "360")
        mode = Surround_360;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}

const char* outputModeToString(OutputMode mode)
{
    return mode == Output_OpenGL_Stereo ? "stereo" : "mono";
}

const char* loopModeToString(LoopMode mode)
{
    switch (mode) {
    case Loop_Off:
        return "off";
        break;
    case Loop_One:
        return "one";
        break;
    case Loop_All:
        return "all";
        break;
    }
    return nullptr;

}

QString loopModeToStringUI(LoopMode mode)
{
    switch (mode) {
    case Loop_Off:
        return QCoreApplication::translate("Mode", "Loop off");
        break;
    case Loop_One:
        return QCoreApplication::translate("Mode", "Loop one");
        break;
    case Loop_All:
        return QCoreApplication::translate("Mode", "Loop all");
        break;
    }
    return QString();
}

LoopMode loopModeFromString(const QString& s, bool* ok)
{
    LoopMode mode = Loop_Off;
    bool r = true;
    if (s == "off")
        mode = Loop_Off;
    else if (s == "one")
        mode = Loop_One;
    else if (s == "all")
        mode = Loop_All;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}

const char* waitModeToString(WaitMode mode)
{
    switch (mode) {
    case Wait_Off:
        return "off";
        break;
    case Wait_On:
        return "on";
        break;
    }
    return nullptr;

}

QString waitModeToStringUI(WaitMode mode)
{
    switch (mode) {
    case Wait_Off:
        return QCoreApplication::translate("Mode", "Wait off");
        break;
    case Wait_On:
        return QCoreApplication::translate("Mode", "Wait on");
        break;
    }
    return QString();
}

WaitMode waitModeFromString(const QString& s, bool* ok)
{
    WaitMode mode = Wait_Off;
    bool r = true;
    if (s == "off")
        mode = Wait_Off;
    else if (s == "on")
        mode = Wait_On;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}
