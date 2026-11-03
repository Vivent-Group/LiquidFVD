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

#include "theme.h"
#include "qcustomplot.h"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QPalette>
#include <QWidget>

namespace
{
    bool    gDark = false;
    QString gDefaultStyle;
    QPalette gDefaultPalette;
    bool    gInitialized = false;

    QPalette darkPalette()
    {
        const QColor window(45, 45, 48);
        const QColor base(30, 30, 33);
        const QColor altBase(40, 40, 44);
        const QColor button(55, 55, 59);
        const QColor text(225, 225, 228);
        const QColor disabledText(120, 120, 125);
        const QColor highlight(61, 142, 230);

        QPalette p;
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, altBase);
        p.setColor(QPalette::ToolTipBase, button);
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::BrightText, QColor(255, 80, 80));
        p.setColor(QPalette::Link, highlight.lighter(120));
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Light, button.lighter(140));
        p.setColor(QPalette::Midlight, button.lighter(115));
        p.setColor(QPalette::Mid, button.darker(120));
        p.setColor(QPalette::Dark, base.darker(130));
        p.setColor(QPalette::Shadow, Qt::black);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        p.setColor(QPalette::PlaceholderText, disabledText);
#endif

        p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
        p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
        p.setColor(QPalette::Disabled, QPalette::Highlight, button.lighter(130));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);
        return p;
    }
}

void theme::init()
{
    if(gInitialized) return;
    gDefaultStyle = QApplication::style()->objectName();
    gDefaultPalette = QApplication::palette();
    gInitialized = true;
}

bool theme::isDark()
{
    return gDark;
}

QPen theme::boundaryPen()
{
    return gDark ? QPen(QColor(255, 255, 255, 110), 1, Qt::DashDotLine)
                 : QPen(QColor(0, 0, 0, 150), 1, Qt::DashDotLine);
}

QBrush theme::boundaryBrush()
{
    return gDark ? QBrush(QColor(255, 255, 255, 18))
                 : QBrush(QColor(0, 0, 0, 20));
}

void theme::stylePlot(QCustomPlot* plot)
{
    if(!plot) return;

    const QColor bg     = gDark ? QColor(30, 30, 33)    : QColor(Qt::white);
    const QColor fg     = gDark ? QColor(200, 200, 205) : QColor(Qt::black);
    const QColor grid   = gDark ? QColor(70, 70, 76)    : QColor(220, 220, 220);

    plot->setBackground(QBrush(bg));
    plot->axisRect()->setBackground(QBrush(bg));

    QList<QCPAxis*> axes = plot->axisRect()->axes();
    for(int i = 0; i < axes.size(); ++i) {
        QCPAxis* a = axes[i];
        a->setBasePen(QPen(fg, 1));
        a->setTickPen(QPen(fg, 1));
        a->setSubTickPen(QPen(fg, 1));
        a->setTickLabelColor(fg);
        a->setLabelColor(fg);
        a->grid()->setPen(QPen(grid, 1, Qt::DotLine));
        a->grid()->setSubGridPen(QPen(grid.darker(gDark ? 130 : 110), 1, Qt::DotLine));
        a->grid()->setZeroLinePen(QPen(grid, 1));
    }

    for(int i = 0; i < plot->plottableCount(); ++i) {
        QCPAbstractPlottable* p = plot->plottable(i);
        if(p->property("fvdBoundary").toBool()) {
            p->setPen(boundaryPen());
            p->setBrush(boundaryBrush());
        }
    }
    plot->replot();
}

void theme::apply(bool dark)
{
    init();
    gDark = dark;

    if(dark) {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(darkPalette());
    } else {
        QStyle* s = QStyleFactory::create(gDefaultStyle);
        if(s) QApplication::setStyle(s);
        QApplication::setPalette(gDefaultPalette);
    }

    // restyle every plot that already exists
    QList<QWidget*> widgets = QApplication::allWidgets();
    for(int i = 0; i < widgets.size(); ++i) {
        QCustomPlot* plot = qobject_cast<QCustomPlot*>(widgets[i]);
        if(plot) stylePlot(plot);
    }
}
