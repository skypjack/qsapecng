/*
    QSapecNG - Qt based SapecNG GUI front-end
    Copyright (C) 2009, Michele Caini
    Copyright (C) 2026, Wojciech Kaczmarski

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "gui/resultpanel.h"
#include "gui/theme.h"
#include "gui/workplane/workplane.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFileInfo>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTabBar>
#include <QMenu>
#include <QTextBrowser>
#include <QLabel>
#include <QLocale>
#include <cmath>
#include <QTimer>
#include <QTextDocument>
#include <QFont>
#include <QUrl>
#include <QVBoxLayout>


namespace qsapecng
{


namespace
{


// width of a piece of the fraction as shown (large font)
int htmlWidth(const QString& html, const QFont& font)
{
  QTextDocument doc;
  doc.setDefaultFont(font);
  doc.setDocumentMargin(0);
  doc.setHtml(QStringLiteral("<span style='font-size: large;'>%1</span>")
    .arg(html));
  return int(std::ceil(doc.idealWidth()));
}


// The expression on one line of at most 'width' pixels: whole terms
// (highest power of s first) while they fit, then as many products of the
// next term's coefficient as fit, and "+ ...":
//   C1 C2 R1 R2 s^4 + (C1 C2 R1 + C1 C2 R2 + ...) s^3 + ...
QString elidedHtml(const formula::Expr& e, int width, const QFont& font)
{
  using formula::Style;
  const QString full = formula::render(e, Style::Html);
  if(htmlWidth(full, font) <= width)
    return full;
  const QString more = QStringLiteral(" + …");

  auto join = [](const QString& left, const QString& right) {
      if(left.isEmpty())
        return right;
      if(right.startsWith(QStringLiteral("−")))
        return left + QStringLiteral(" − ") + right.mid(1);
      return left + QStringLiteral(" + ") + right;
    };
  auto power = [](int degree) {
      if(degree == 1)
        return QStringLiteral(" · <i>s</i>");
      if(degree > 1)
        return QStringLiteral(" · <i>s</i><sup>%1</sup>").arg(degree);
      return QString();
    };

  QString shown;
  for(auto d = e.rbegin(); d != e.rend(); ++d) {
    // the whole term
    formula::Expr one;
    one[d->first] = d->second;
    const QString whole = join(shown, formula::render(one, Style::Html));
    if(htmlWidth(whole + more, font) <= width) {
      shown = whole;
      continue;
    }
    // part of it: the first products of its coefficient
    QString products;
    for(const auto& term : d->second) {
      formula::Expr p;
      p[0].push_back(term);
      const QString next = join(products, formula::render(p, Style::Html));
      const QString candidate = join(shown, QStringLiteral("(") + next + more
        + QStringLiteral(")") + power(d->first)) + more;
      // at least one product when nothing is shown yet
      if(htmlWidth(candidate, font) > width
          && !(shown.isEmpty() && products.isEmpty()))
        break;
      products = next;
    }
    if(!products.isEmpty())
      shown = join(shown, QStringLiteral("(") + products + more
        + QStringLiteral(")") + power(d->first));
    break;
  }
  return shown + more;
}


// width: 0 = everything; otherwise numerator and denominator are each cut
// to one line (elidedHtml)
QString fractionHtml(const formula::Fraction& f, const QColor& rule,
  int width = 0, const QFont& font = QFont())
{
  // [-] num / den [* factor], laid out as one table row
  const QString cell = QStringLiteral(
    "<td valign='middle' style='padding: 0 4px; font-size: large;'>%1</td>");
  const QString sign = f.negative ? cell.arg(QStringLiteral("−")) : QString();
  const QString factor = formula::renderFactor(f, formula::Style::Html);
  const bool plainNum = formula::isOne(f.num) && !factor.isEmpty();

  QString middle;
  if(formula::isOne(f.den)) {
    // one line: "−10 · V1"
    QString line = f.negative ? QStringLiteral("−") : QString();
    if(!plainNum) {
      const QString n = width > 0 ? elidedHtml(f.num, width, font)
                                  : formula::render(f.num, formula::Style::Html);
      // a sum times the input, or negated: (L1 · s + R1) · I1
      line += (formula::isSum(f.num) && (!factor.isEmpty() || f.negative))
        ? QStringLiteral("(%1)").arg(n) : n;
    }
    if(!factor.isEmpty())
      line += (plainNum ? QString() : QStringLiteral(" · ")) + factor;
    return QStringLiteral(
        "<p style='margin: 6px 0 4px 22px; font-size: large;'>%1</p>")
      .arg(line);
  } else {
    // the fraction line is the bottom border of the numerator cell
    middle = QStringLiteral(
        "<td valign='middle'><table cellspacing='0' cellpadding='0' "
        "style='border-collapse: collapse;'>"
        "<tr><td align='center' style='padding: 2px 10px 4px 10px; "
        "border-bottom-style: solid; border-bottom-width: 1px; "
        "border-bottom-color: %3; font-size: large;'>%1</td></tr>"
        "<tr><td align='center' style='padding: 4px 10px 2px 10px; "
        "font-size: large;'>%2</td></tr>"
        "</table></td>")
      .arg(width > 0 ? elidedHtml(f.num, width, font)
                     : formula::render(f.num, formula::Style::Html),
           width > 0 ? elidedHtml(f.den, width, font)
                     : formula::render(f.den, formula::Style::Html),
           rule.name());
  }

  QString tail;
  if(!factor.isEmpty())
    tail = cell.arg((middle.isEmpty() ? QString() : QStringLiteral("· "))
                    + factor);

  return QStringLiteral(
      "<table cellspacing='0' cellpadding='0' style='margin: 6px 0 4px 12px;'>"
      "<tr>%1%2%3</tr></table>").arg(sign, middle, tail);
}


}


ResultPanel::ResultPanel(QWidget* parent)
  : QWidget(parent), hasResult_(false), outOfDate_(false)
{
  view_ = new QTextBrowser;
  view_->setOpenLinks(false);
  view_->setFrameShape(QFrame::NoFrame);
  // the usual menu (copy, select all) and Clear
  view_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(view_, &QWidget::customContextMenuRequested, this,
    [this](const QPoint& pos) {
      QMenu* menu = view_->createStandardContextMenu(pos);
      menu->addSeparator();
      QAction* clearAct = menu->addAction(tr("C&lear"));
      clearAct->setEnabled(hasResult_);
      connect(clearAct, &QAction::triggered, this,
        &ResultPanel::clearRequested);
      menu->exec(view_->viewport()->mapToGlobal(pos));
      delete menu;
    });
  connect(view_, &QTextBrowser::anchorClicked,
    this, &ResultPanel::linkClicked);

  tabs_ = new QTabBar;
  tabs_->setDocumentMode(true);
  tabs_->setExpanding(false);
  tabs_->setDrawBase(true);
  tabs_->hide();
  connect(tabs_, &QTabBar::currentChanged, this, [this](int index) {
      if(index < 0 || index >= result_.outputs.size())
        return;
      currentName_ = result_.outputs.at(index).name;
      render();
    });

  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(tabs_);
  // a strip of its own: showing it does not lay out the result again
  notice_ = new QLabel(tr("Out of date: the circuit changed since it was "
    "solved. Resolve again (F8) to update."));
  notice_->setWordWrap(true);
  notice_->setContentsMargins(8, 5, 8, 5);
  notice_->setAutoFillBackground(true);
  notice_->hide();
  layout->addWidget(notice_);
  layout->addWidget(view_);

  render();
}


void ResultPanel::showResult(const SolveResult& result, bool outOfDate)
{
  // while the symbolic part runs: the estimate made before it started,
  // until the main window gives the time left at the speed so far
  if(result.symbolic == SolveResult::SymbolicRunning) {
    if(result_.symbolic != SolveResult::SymbolicRunning || timeLeft_.isEmpty())
      timeLeft_ = result.seconds > 0. ? shortTime(result.seconds) : QString();
  } else {
    timeLeft_.clear();
  }
  result_ = result;
  hasResult_ = true;
  outOfDate_ = outOfDate;
  updateTabs();
  render();
}


void ResultPanel::setOutOfDate(bool outOfDate)
{
  outOfDate_ = outOfDate && hasResult_;
  updateNotice();
}


void ResultPanel::updateNotice()
{
  if(!notice_)
    return;
  const QColor bg = theme::isDark() ? QColor(0x4a, 0x3b, 0x12)
                                    : QColor(0xfd, 0xf1, 0xc8);
  const QColor fg = theme::isDark() ? QColor(0xf5, 0xd9, 0x8a)
                                    : QColor(0x6b, 0x4e, 0x00);
  QPalette pal = notice_->palette();
  pal.setColor(QPalette::Window, bg);
  pal.setColor(QPalette::WindowText, fg);
  notice_->setPalette(pal);
  notice_->setVisible(outOfDate_ && hasResult_);
}


void ResultPanel::showPlaceholder()
{
  hasResult_ = false;
  outOfDate_ = false;
  result_ = SolveResult();
  updateTabs();
  render();
}


void ResultPanel::updateTabs()
{
  QSignalBlocker block(tabs_);
  while(tabs_->count())
    tabs_->removeTab(0);

  int index = 0;
  for(int i = 0; i < result_.outputs.size(); ++i) {
    const OutputResult& o = result_.outputs.at(i);
    tabs_->addTab(o.name);
    if(o.singular)
      tabs_->setTabToolTip(i, tr("No unique solution"));
    if(o.name == currentName_)
      index = i;
  }
  tabs_->setCurrentIndex(index);
  if(index < result_.outputs.size())
    currentName_ = result_.outputs.at(index).name;
  // tabs only when there is a choice
  tabs_->setVisible(hasResult_ && result_.outputs.size() > 1);
}


const OutputResult* ResultPanel::current() const
{
  const int index = tabs_->currentIndex();
  if(index >= 0 && index < result_.outputs.size())
    return &result_.outputs.at(index);
  return result_.outputs.isEmpty() ? nullptr : &result_.outputs.first();
}


QString ResultPanel::currentOutput() const
{
  const OutputResult* out = current();
  return out ? out->name : QString();
}


void ResultPanel::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  // the layout (side by side or stacked) may change with the width
  if(hasResult_)
    QTimer::singleShot(0, this, [this]() { render(); });
}


void ResultPanel::changeEvent(QEvent* event)
{
  QWidget::changeEvent(event);
  if(event->type() == QEvent::PaletteChange)
    render();
}


QString ResultPanel::shortTime(double seconds)
{
  if(!(seconds >= 0.) || !std::isfinite(seconds))
    return QString();
  const long long s = std::llround(seconds);
  if(s < 60)
    return tr("%1s").arg(s);
  const long long minutes = std::llround(seconds / 60.);
  if(minutes < 60)
    return tr("%1min").arg(minutes);
  return tr("%1h %2min").arg(minutes / 60).arg(minutes % 60);
}


void ResultPanel::setTimeLeft(double seconds)
{
  if(!hasResult_ || result_.symbolic != SolveResult::SymbolicRunning)
    return;
  const QString text = seconds > 0. ? shortTime(seconds) : QString();
  if(text == timeLeft_)
    return;   // drawn again only when the text changes (once a minute)
  timeLeft_ = text;
  render();
}


void ResultPanel::linkClicked(const QUrl& url)
{
  if(url.scheme() == QLatin1String("symbolic")) {
    emit symbolicRequested();
    return;
  }

  if(url.scheme() == QLatin1String("plot")) {
    emit plotRequested(url.path().toInt());
    return;
  }

  if(url.scheme() == QLatin1String("expand")
      || url.scheme() == QLatin1String("collapse")) {
    const QString key = currentOutput() + QLatin1Char('/') + url.path();
    if(url.scheme() == QLatin1String("expand"))
      expanded_.insert(key);
    else
      expanded_.remove(key);
    render();
    return;
  }

  const bool latex = url.scheme() == QLatin1String("latex");
  if(!latex && url.scheme() != QLatin1String("copy"))
    return;

  const OutputResult* out = current();
  if(!out)
    return;
  const QString which = url.path();
  const formula::Fraction* f = 0;
  if(which == QLatin1String("result"))
    f = &out->mixed;
  else if(which == QLatin1String("raw"))
    f = &out->raw;
  else if(which == QLatin1String("digit"))
    f = &out->digit;
  if(!f)
    return;

  const QString text = formula::clipboardText(*f,
    latex ? formula::Style::Latex : formula::Style::Text);
  if(!text.isEmpty())
    QApplication::clipboard()->setText(text);
}


void ResultPanel::render()
{
  const QPalette pal = palette();
  const QColor text = pal.color(QPalette::Text);
  QColor dim = text;
  dim.setAlphaF(0.6);
  const QString dimCss = QStringLiteral("rgba(%1,%2,%3,0.6)")
    .arg(dim.red()).arg(dim.green()).arg(dim.blue());
  const QColor link = pal.color(QPalette::Link);

  QString html = QStringLiteral("<html><body style='margin: 6px;'>");

  if(!hasResult_) {
    html += QStringLiteral("<p style='color:%1;'>%2</p>")
      .arg(dimCss, tr("No result yet. Press <b>Resolve</b> (F8) to solve "
                      "the active schematic."));
    view_->setHtml(html + QStringLiteral("</body></html>"));
    updateNotice();
    return;
  }

  updateNotice();

  const OutputResult* out = current();
  QString caption = QFileInfo(result_.file).fileName().toHtmlEscaped();
  // the output's name, unless the tabs already say it
  if(out && !out->name.isEmpty() && result_.outputs.size() == 1)
    caption = (caption.isEmpty() ? QString() : caption + QStringLiteral(" · "))
      + tr("output %1").arg(out->name.toHtmlEscaped());
  html += QStringLiteral("<p style='color:%1; margin: 0;'>%2</p>")
    .arg(dimCss, caption);

  if(!out) {
    html += QStringLiteral("</body></html>");
    view_->setHtml(html);
    return;
  }

  if(out->singular) {
    const QColor fg = theme::isDark() ? QColor(0xff, 0x9a, 0x8a)
                                      : QColor(0xa0, 0x1b, 0x10);
    html += QStringLiteral("<p style='margin: 12px 0 0 0; color:%1;'><b>%2</b></p>"
        "<p style='margin: 6px 0 0 0;'>%3</p><ul style='margin-top: 4px;'>"
        "<li>%4</li><li>%5</li><li>%6</li><li>%7</li></ul>")
      .arg(fg.name(), tr("No unique solution"),
           tr("The circuit's equations are singular, so there is no transfer "
              "function to show. Typical causes:"),
           tr("voltage sources (or a source and a short) in a loop"),
           tr("current sources in series, or a current source into a node "
              "with no other path"),
           tr("a node or a group of parts not connected to the ground"),
           tr("an ideal op-amp whose inputs cannot be held equal (no "
              "negative feedback path)"));
    html += QStringLiteral("</body></html>");
    view_->setHtml(html);
    return;
  }

  // A big circuit's symbolic result has up to millions of terms: laid out
  // whole it took minutes and gigabytes. Shown: the first ones (highest
  // power of s first, as it reads), and how many there are; Copy and Copy
  // LaTeX still give all of it
  const int shownTerms = 200, longFrom = 500;
  auto count = [](const formula::Expr& e) {
      long long n = 0;
      for(const auto& g : e)
        n += (long long)g.second.size();
      return n;
    };
  auto firstTerms = [](const formula::Expr& e, int keep) {
      formula::Expr cut;
      int n = 0;
      for(auto it = e.rbegin(); it != e.rend() && n < keep; ++it)
        for(const auto& t : it->second) {
          if(n++ >= keep)
            break;
          cut[it->first].push_back(t);
        }
      return cut;
    };
  long long allTerms[3] = { 0, 0, 0 };
  auto shown = [&](const formula::Fraction& f, int i) {
      allTerms[i] = count(f.num) + count(f.den);
      if(allTerms[i] < longFrom)
        return f;
      formula::Fraction g = f;
      if(count(g.num) > shownTerms)
        g.num = firstTerms(f.num, shownTerms);
      if(count(g.den) > shownTerms)
        g.den = firstTerms(f.den, shownTerms);
      return g;
    };

  struct Section { QString key, title, note; formula::Fraction f; };
  const Section sections[] = {
    { QStringLiteral("result"), tr("Result"),
      tr("symbolic components by name, the others by value"),
      shown(out->mixed, 0) },
    { QStringLiteral("raw"), tr("Raw"),
      tr("every component by name"),
      shown(out->raw, 1) },
    { QStringLiteral("digit"), tr("Digit"),
      tr("numeric"),
      shown(out->digit, 2) }
  };
  // under a formula cut short: what is shown of it
  auto cutNote = [&](int i) {
      if(allTerms[i] < longFrom)
        return QString();
      return QStringLiteral("<p style='margin: 2px 0 0 22px; color:%1;'>%2</p>")
        .arg(dimCss, tr("Too long to show in full: only its beginning is "
          "shown. Copy and Copy LaTeX give the whole of it."));
    };

  // a section's heading with its links (Expand / Collapse for a long one)
  auto heading = [&](const Section& s, const QString& toggle) {
      QString links = QStringLiteral(
          "<a href='copy:%1' style='color:%2;'>%3</a>&nbsp;&nbsp;"
          "<a href='latex:%1' style='color:%2;'>%4</a>")
        .arg(s.key, link.name(), tr("Copy"), tr("Copy LaTeX"));
      if(!toggle.isEmpty())
        links += QStringLiteral("&nbsp;&nbsp;<a href='%1:%2' style='color:%3;'>%4</a>")
          .arg(toggle, s.key, link.name(),
               toggle == QLatin1String("expand") ? tr("Expand") : tr("Collapse"));
      return QStringLiteral(
          "<p style='margin: 12px 0 0 0;'><b>%1</b> "
          "<span style='color:%2;'>&nbsp;%3&nbsp;&nbsp;</span>%4</p>")
        .arg(s.title, dimCss, s.note, links);
    };

  auto idealWidth = [this](const QString& html) {
      QTextDocument doc;
      doc.setDefaultFont(view_->font());
      doc.setDocumentMargin(0);
      doc.setHtml(html);
      return int(std::ceil(doc.idealWidth()));
    };

  // Result and Raw when the symbolic result is not there: why, and a link
  // to compute it where that makes sense
  const bool symbolic = result_.symbolic == SolveResult::SymbolicDone;
  QString missing;
  if(!symbolic) {
    // the time it would take, as estimated (nothing when not known)
    const QString what = result_.seconds > 0.
      ? QStringLiteral(" (") + tr("an estimated %1 on this computer")
          .arg(shortTime(result_.seconds)) + QStringLiteral(")")
      : QString();
    const QString compute = QStringLiteral(
        "<a href='symbolic:' style='color:%1;'>%2</a>")
      .arg(link.name());
    switch(result_.symbolic) {
    case SolveResult::SymbolicRunning:
      // "Calculating... 3h 50min left." (the time left as the solve goes)
      missing = tr("Calculating...");
      if(!timeLeft_.isEmpty())
        missing += QStringLiteral(" ") + tr("%1 left.").arg(timeLeft_);
      missing += QStringLiteral(" ") + tr("Editing the circuit stops it.");
      break;
    case SolveResult::SymbolicSkipped:
      missing = tr("Not computed%1.").arg(what) + QStringLiteral(" ")
        + compute.arg(tr("Compute it now"));
      break;
    case SolveResult::SymbolicStopped:
      missing = tr("Stopped before it was done%1.").arg(what)
        + QStringLiteral(" ") + compute.arg(tr("Compute it again"));
      break;
    case SolveResult::SymbolicTooLarge:
      missing = tr("Too large for this computer's memory. The numeric "
        "result (Digit) and the plots are complete.");
      break;
    case SolveResult::SymbolicOff:
      missing = tr("Not computed: Settings > General > Solver asks for the "
        "numeric result only%1.").arg(what) + QStringLiteral(" ")
        + compute.arg(tr("Compute it now"));
      break;
    default:
      break;
    }
    missing = QStringLiteral("<p style='margin: 6px 0 0 0; color:%1;'>%2</p>")
      .arg(dimCss, missing);
  }
  auto plainHeading = [&](const Section& s) {
      return QStringLiteral(
          "<p style='margin: 12px 0 0 0;'><b>%1</b> "
          "<span style='color:%2;'>&nbsp;%3</span></p>")
        .arg(s.title, dimCss, s.note);
    };

  QStringList blocks;
  QList<int> widths;           // the whole section (side by side or not)
  QList<int> fractionWidths;   // the formula alone (cut short or not)
  QList<bool> absent;          // no formula (the symbolic part is not there)
  for(int i = 0; i < 3; ++i) {
    const Section& s = sections[i];
    const bool none = !symbolic && i < 2;
    absent << none;
    if(none) {
      // the note (once, under Result) wraps
      const QString block = plainHeading(s) + (i == 0 ? missing
        : QStringLiteral("<p style='margin: 6px 0 0 0; color:%1;'>%2</p>")
            .arg(dimCss, tr("Comes with Result.")));
      widths << qMin(idealWidth(block), 360);
      fractionWidths << 0;
      blocks << block;
      continue;
    }
    const QString fraction = fractionHtml(s.f, text) + cutNote(i);
    QString block = heading(s, QString()) + fraction;
    widths << idealWidth(block);
    fractionWidths << idealWidth(fraction);
    blocks << block;
  }

  // side by side when the three fit in the panel's width (wide screens),
  // otherwise one below the other
  const int gap = 32;
  int needed = 0;
  for(int w : widths)
    needed += w + gap;
  const int available = view_->viewport()->width()
    - 2 * int(view_->document()->documentMargin()) - 12;
  sideBySide_ = needed <= available;

  // one below the other: a section too wide for the panel is shown on
  // one line, cut short, with Expand (and Collapse when expanded)
  if(!sideBySide_) {
    const QString prefix = out->name + QLatin1Char('/');
    for(int i = 0; i < 3; ++i) {
      if(absent.at(i) || fractionWidths.at(i) <= available)
        continue;
      const Section& s = sections[i];
      if(expanded_.contains(prefix + s.key)) {
        blocks[i] = heading(s, QStringLiteral("collapse"))
          + fractionHtml(s.f, text) + cutNote(i);
      } else {
        // room for the sign, the input factor and the cell padding
        const int factor = htmlWidth(formula::renderFactor(s.f,
          formula::Style::Html), view_->font());
        const int width = qMax(120, available - factor - 80);
        blocks[i] = heading(s, QStringLiteral("expand"))
          + fractionHtml(s.f, text, width, view_->font()) + cutNote(i);
      }
    }
  }
  if(sideBySide_) {
    html += QStringLiteral("<table cellspacing='0' cellpadding='0'><tr>");
    for(int i = 0; i < blocks.size(); ++i)
      html += QStringLiteral("<td valign='top' style='padding-right: %1px;%3'>%2</td>")
        .arg(gap).arg(blocks.at(i))
        .arg(absent.at(i) ? QStringLiteral(" width: 360px;") : QString());
    html += QStringLiteral("</tr></table>");
  } else {
    html += blocks.join(QString());
  }

  html += QStringLiteral("</body></html>");

  // keep the reading position (e.g. when only the out-of-date state changes)
  const int scroll = view_->verticalScrollBar()->value();
  view_->setHtml(html);
  view_->verticalScrollBar()->setValue(scroll);
}


}
