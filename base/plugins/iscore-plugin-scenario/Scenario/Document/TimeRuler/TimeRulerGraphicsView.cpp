#include "TimeRulerGraphicsView.hpp"
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>


namespace Scenario
{

TimeRulerGraphicsView::TimeRulerGraphicsView(QGraphicsScene* scene):
    QGraphicsView{scene}
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::NoFocus);
    setSceneRect(ScenarioLeftSpace, -70, 800, 35);
    setFixedHeight(40);
    setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);

    setBackgroundBrush(ScenarioStyle::instance().TimeRulerBackground.getBrush());

#if defined(__APPLE__)
    setRenderHints(0);
    setOptimizationFlags(QGraphicsView::IndirectPainting);
#endif
}

}
