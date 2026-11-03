#ifndef THEME_H
#define THEME_H

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

#include <QColor>
#include <QPen>
#include <QBrush>

class QCustomPlot;

// Light / dark appearance for the whole application, including the graph plots.
namespace theme
{
    // call once after QApplication is created, before any theme change
    void init();

    // switches palette + style of the application and restyles every open plot
    void apply(bool dark);
    bool isDark();

    // colors of the section boundary bands in the graph widget
    QPen boundaryPen();
    QBrush boundaryBrush();

    // background, axes, grid and boundary graphs of one plot
    void stylePlot(QCustomPlot* plot);
}

#endif // THEME_H
