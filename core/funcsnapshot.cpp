/*
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "funcsnapshot.h"
#include "section.h"
#include "function.h"
#include "subfunction.h"
#include "exportfuncs.h"

#include <sstream>
#include <string>

static func* funcAt(section* sec, int k)
{
    switch(k) {
    case 0: return sec->rollFunc;
    case 1: return sec->normForce;
    default: return sec->latForce;
    }
}

QByteArray funcsnapshot::save(section* sec)
{
    std::stringstream ss;
    for(int k = 0; k < 3; ++k) {
        func* f = funcAt(sec, k);
        if(f) f->saveFunction(ss);
    }
    std::string s = ss.str();
    return QByteArray(s.data(), (int)s.size());
}

static bool restoreFunc(func* f, std::stringstream& ss)
{
    if(readString(&ss, 4) != "FUNC") return false;
    int n = readInt(&ss);
    if(n < 1 || n > 10000) return false;
    while(f->funcList.size() > n) f->removeSubFunction(f->funcList.size()-1);
    while(f->funcList.size() < n) f->appendSubFunction(1.f, f->funcList.size()-1);
    for(int i = 0; i < n; ++i) f->funcList[i]->loadSubFunc(ss);
    return true;
}

bool funcsnapshot::restore(section* sec, const QByteArray& data)
{
    std::stringstream ss(std::string(data.constData(), data.size()));
    for(int k = 0; k < 3; ++k) {
        func* f = funcAt(sec, k);
        if(f && !restoreFunc(f, ss)) return false;
    }
    return true;
}
