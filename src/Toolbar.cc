// Toolbar.cc for Openbox
// Copyright (c) 2001 Sean 'Shaleh' Perry <shaleh@debian.org>
// Copyright (c) 1997 - 2000 Brad Hughes (bhughes@tcac.net)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

// stupid macros needed to access some functions in version 2 of the GNU C
// library
#ifndef   _GNU_SOURCE
#define   _GNU_SOURCE
#endif // _GNU_SOURCE

#ifdef    HAVE_CONFIG_H
#  include "../config.h"
#endif // HAVE_CONFIG_H

#include "i18n.h"
#include "openbox.h"
#include "Clientmenu.h"
#include "Iconmenu.h"
#include "Rootmenu.h"
#include "Screen.h"
#include "Toolbar.h"
#include "Window.h"
#include "Workspace.h"
#include "Workspacemenu.h"

#include <X11/keysym.h>

#ifdef    HAVE_STRING_H
#  include <string.h>
#endif // HAVE_STRING_H

#ifdef    HAVE_STDIO_H
#  include <stdio.h>
#endif // HAVE_STDIO_H

#ifdef    TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else // !TIME_WITH_SYS_TIME
# ifdef    HAVE_SYS_TIME_H
#  include <sys/time.h>
# else // !HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif // TIME_WITH_SYS_TIME

#include <strstream>
#include <string>
using std::ends;

Toolbar::Toolbar(BScreen &scrn, Resource &conf) : screen(scrn),
  openbox(scrn.getOpenbox()), config(conf)
{
  load();

  // get the clock updating every minute
  clock_timer = new BTimer(openbox, *this);
  timeval now;
  gettimeofday(&now, 0);
  clock_timer->setTimeout((60 - (now.tv_sec % 60)) * 1000);
  clock_timer->start();

  hide_handler.toolbar = this;
  hide_timer = new BTimer(openbox, hide_handler);
  hide_timer->setTimeout(openbox.getAutoRaiseDelay());
  hide_timer->fireOnce(True);

  image_ctrl = screen.getImageControl();

  m_editing = False;
  new_workspace_name = (char *) 0;
  new_name_pos = 0;
  frame.grab_x = frame.grab_y = 0;

  toolbarmenu = new Toolbarmenu(*this);

  display = openbox.getXDisplay();
  XSetWindowAttributes attrib;
  unsigned long create_mask = CWBackPixmap | CWBackPixel | CWBorderPixel |
                              CWColormap | CWOverrideRedirect | CWEventMask;
  attrib.background_pixmap = None;
  attrib.background_pixel = attrib.border_pixel =
    screen.getBorderColor()->getPixel();
  attrib.colormap = screen.getColormap();
  attrib.override_redirect = True;
  attrib.event_mask = ButtonPressMask | ButtonReleaseMask |
                      EnterWindowMask | LeaveWindowMask;

  frame.window =
    XCreateWindow(display, screen.getRootWindow(), 0, 0, 1, 1, 0,
		  screen.getDepth(), InputOutput, screen.getVisual(),
		  create_mask, &attrib);
  openbox.saveToolbarSearch(frame.window, this);

  attrib.event_mask = ButtonPressMask | ButtonReleaseMask | ExposureMask |
                      KeyPressMask | EnterWindowMask;

  frame.workspace_label =
    XCreateWindow(display, frame.window, 0, 0, 1, 1, 0, screen.getDepth(),
		  InputOutput, screen.getVisual(), create_mask, &attrib);
  openbox.saveToolbarSearch(frame.workspace_label, this);

  frame.window_label =
    XCreateWindow(display, frame.window, 0, 0, 1, 1, 0, screen.getDepth(),
		  InputOutput, screen.getVisual(), create_mask, &attrib);
  openbox.saveToolbarSearch(frame.window_label, this);

  frame.clock =
    XCreateWindow(display, frame.window, 0, 0, 1, 1, 0, screen.getDepth(),
		  InputOutput, screen.getVisual(), create_mask, &attrib);
  openbox.saveToolbarSearch(frame.clock, this);

  frame.psbutton =
    XCreateWindow(display ,frame.window, 0, 0, 1, 1, 0, screen.getDepth(),
                  InputOutput, screen.getVisual(), create_mask, &attrib);
  openbox.saveToolbarSearch(frame.psbutton, this);

  frame.nsbutton =
    XCreateWindow(display ,frame.window, 0, 0, 1, 1, 0, screen.getDepth(),
                  InputOutput, screen.getVisual(), create_mask, &attrib);
  openbox.saveToolbarSearch(frame.nsbutton, this);

  frame.pwbutton =
    XCreateWindow(display ,frame.window, 0, 0, 1, 1, 0, screen.getDepth(),
                  InputOutput, screen.getVisual(), create_mask, &attrib);
  openbox.saveToolbarSearch(frame.pwbutton, this);

  frame.nwbutton =
    XCreateWindow(display ,frame.window, 0, 0, 1, 1, 0, screen.getDepth(),
                  InputOutput, screen.getVisual(), create_mask, &attrib);
  openbox.saveToolbarSearch(frame.nwbutton, this);

  frame.base = frame.label = frame.wlabel = frame.clk = frame.button =
    frame.pbutton = None;

  reconfigure();
  mapToolbar();
}

Rect Toolbar::area() const {
  int x = ((m_hidden) ? frame.x_hidden : frame.x);
  int y;
  if (screen.hideToolbar()) y = screen.size().h();
  else if (m_hidden) y = frame.y_hidden;
  else y = frame.y;
  return Rect(x, y, frame.width, frame.height);
}

unsigned int Toolbar::getExposedHeight() const {
  if (screen.hideToolbar()) return 0;
  else if (m_autohide) return frame.bevel_w;
  else return frame.height;
}

void Toolbar::mapToolbar(){
  if (!screen.hideToolbar()) {
    //not hidden, so windows should not maximize over the toolbar
    XMapSubwindows(display, frame.window);
    XMapWindow(display, frame.window);
  }
}

void Toolbar::unMapToolbar(){
  //hidden so we can maximize over the toolbar
  XUnmapWindow(display, frame.window);
}

Toolbar::~Toolbar() {
  unMapToolbar();
  if (frame.base) image_ctrl->removeImage(frame.base);
  if (frame.label) image_ctrl->removeImage(frame.label);
  if (frame.wlabel) image_ctrl->removeImage(frame.wlabel);
  if (frame.clk) image_ctrl->removeImage(frame.clk);
  if (frame.button) image_ctrl->removeImage(frame.button);
  if (frame.pbutton) image_ctrl->removeImage(frame.pbutton);

  openbox.removeToolbarSearch(frame.window);
  openbox.removeToolbarSearch(frame.workspace_label);
  openbox.removeToolbarSearch(frame.window_label);
  openbox.removeToolbarSearch(frame.clock);
  openbox.removeToolbarSearch(frame.psbutton);
  openbox.removeToolbarSearch(frame.nsbutton);
  openbox.removeToolbarSearch(frame.pwbutton);
  openbox.removeToolbarSearch(frame.nwbutton);

  XDestroyWindow(display, frame.workspace_label);
  XDestroyWindow(display, frame.window_label);
  XDestroyWindow(display, frame.clock);

  XDestroyWindow(display, frame.window);

  delete hide_timer;
  delete clock_timer;
  delete toolbarmenu;
}


void Toolbar::setOnTop(bool b) {
  m_ontop = b;
  std::ostrstream s;
  s << "session.screen" << screen.getScreenNumber() << ".toolbar.onTop" << ends;
  config.setValue(s.str(), m_ontop ? "True" : "False");
  s.rdbuf()->freeze(0);
}

void Toolbar::setAutoHide(bool b) {
  m_autohide = b;
  std::ostrstream s;
  s << "session.screen" << screen.getScreenNumber() << ".toolbar.autoHide"
    << ends;
  config.setValue(s.str(), m_autohide ? "True" : "False");
  s.rdbuf()->freeze(0);
}

void Toolbar::setWidthPercent(int w) {
  m_width_percent = w;
  std::ostrstream s;
  s << "session.screen" << screen.getScreenNumber() << ".toolbar.widthPercent"
    << ends;
  config.setValue(s.str(), m_width_percent);
  s.rdbuf()->freeze(0);
}

void Toolbar::setPlacement(int p) {
  m_placement = p;
  std::ostrstream s;
  s << "session.screen" << screen.getScreenNumber() << ".toolbar.placement"
    << ends;
  const char *placement;
  switch (m_placement) {
  case TopLeft: placement = "TopLeft"; break;
  case BottomLeft: placement = "BottomLeft"; break;
  case TopCenter: placement = "TopCenter"; break;
  case TopRight: placement = "TopRight"; break;
  case BottomRight: placement = "BottomRight"; break;
  case BottomCenter: default: placement = "BottomCenter"; break;
  }
  config.setValue(s.str(), placement);
  s.rdbuf()->freeze(0);
}

void Toolbar::save() {
  setOnTop(m_ontop);
  setAutoHide(m_autohide);
  setWidthPercent(m_width_percent);
  setPlacement(m_placement);
}

void Toolbar::load() {
  std::ostrstream rscreen, rname, rclass;
  std::string s;
  bool b;
  long l;
  rscreen << "session.screen" << screen.getScreenNumber() << '.' << ends;

  rname << rscreen.str() << "toolbar.widthPercent" << ends;
  rclass << rscreen.str() << "Toolbar.WidthPercent" << ends;
  if (config.getValue(rname.str(), rclass.str(), l) && (l > 0 && l <= 100))
    m_width_percent = l;
  else
    m_width_percent =66;

  rname.seekp(0); rclass.seekp(0);
  rname.rdbuf()->freeze(0); rclass.rdbuf()->freeze(0);
  rname << rscreen.str() << "toolbar.placement" << ends;
  rclass << rscreen.str() << "Toolbar.Placement" << ends;
  if (config.getValue(rname.str(), rclass.str(), s)) {
    if (0 == strncasecmp(s.c_str(), "TopLeft", s.length()))
      m_placement = TopLeft;
    else if (0 == strncasecmp(s.c_str(), "BottomLeft", s.length()))
      m_placement = BottomLeft;
    else if (0 == strncasecmp(s.c_str(), "TopCenter", s.length()))
      m_placement = TopCenter;
    else if (0 == strncasecmp(s.c_str(), "TopRight", s.length()))
      m_placement = TopRight;
    else if ( 0 == strncasecmp(s.c_str(), "BottomRight", s.length()))
      m_placement = BottomRight;
    else if ( 0 == strncasecmp(s.c_str(), "BottomCenter", s.length()))
      m_placement = BottomCenter;
  } else
    m_placement = BottomCenter;
  
  rname.seekp(0); rclass.seekp(0);
  rname.rdbuf()->freeze(0); rclass.rdbuf()->freeze(0);
  rname << rscreen.str() << "toolbar.onTop" << ends;
  rclass << rscreen.str() << "Toolbar.OnTop" << ends;
  if (config.getValue(rname.str(), rclass.str(), b))
    m_ontop = b;
  else
    m_ontop = false;

  rname.seekp(0); rclass.seekp(0);
  rname.rdbuf()->freeze(0); rclass.rdbuf()->freeze(0);
  rname << rscreen.str() << "toolbar.autoHide" << ends;
  rclass << rscreen.str() << "Toolbar.AutoHide" << ends;
  if (config.getValue(rname.str(), rclass.str(), b))
    m_hidden = m_autohide = b;
  else
    m_hidden = m_autohide = false;

  rscreen.rdbuf()->freeze(0);
  rname.rdbuf()->freeze(0); rclass.rdbuf()->freeze(0);
}

void Toolbar::reconfigure() {
  frame.bevel_w = screen.getBevelWidth();
  frame.width = screen.size().w() * m_width_percent / 100;
  
  if (i18n->multibyte())
    frame.height =
      screen.getToolbarStyle()->fontset_extents->max_ink_extent.height;
  else
    frame.height = screen.getToolbarStyle()->font->ascent +
		   screen.getToolbarStyle()->font->descent;
  frame.button_w = frame.height;
  frame.height += 2;
  frame.label_h = frame.height;
  frame.height += (frame.bevel_w * 2);
  
  switch (m_placement) {
  case TopLeft:
    frame.x = 0;
    frame.y = 0;
    frame.x_hidden = 0;
    frame.y_hidden = screen.getBevelWidth() - screen.getBorderWidth()
                     - frame.height;
    break;

  case BottomLeft:
    frame.x = 0;
    frame.y = screen.size().h() - frame.height
      - (screen.getBorderWidth() * 2);
    frame.x_hidden = 0;
    frame.y_hidden = screen.size().h() - screen.getBevelWidth()
                     - screen.getBorderWidth();
    break;

  case TopCenter:
    frame.x = (screen.size().w() - frame.width) / 2;
    frame.y = 0;
    frame.x_hidden = frame.x;
    frame.y_hidden = screen.getBevelWidth() - screen.getBorderWidth()
                     - frame.height;
    break;

  case BottomCenter:
  default:
    frame.x = (screen.size().w() - frame.width) / 2;
    frame.y = screen.size().h() - frame.height
      - (screen.getBorderWidth() * 2);
    frame.x_hidden = frame.x;
    frame.y_hidden = screen.size().h() - screen.getBevelWidth()
                     - screen.getBorderWidth();
    break;

  case TopRight:
    frame.x = screen.size().w() - frame.width
      - (screen.getBorderWidth() * 2);
    frame.y = 0;
    frame.x_hidden = frame.x;
    frame.y_hidden = screen.getBevelWidth() - screen.getBorderWidth()
                     - frame.height;
    break;

  case BottomRight:
    frame.x = screen.size().w() - frame.width
      - (screen.getBorderWidth() * 2);
    frame.y = screen.size().h() - frame.height
      - (screen.getBorderWidth() * 2);
    frame.x_hidden = frame.x;
    frame.y_hidden = screen.size().h() - screen.getBevelWidth()
                     - screen.getBorderWidth();
    break;
  }

#ifdef    HAVE_STRFTIME
  time_t ttmp = time(NULL);
  struct tm *tt = 0;

  if (ttmp != -1) {
    tt = localtime(&ttmp);
    if (tt) {
      char t[1025], *time_string = (char *) 0;
      int len = strftime(t, 1024, screen.strftimeFormat(), tt);
      t[len++] = 'A';   // add size to the string for padding
      t[len++] = 'A';   // add size to the string for padding
      t[len] = '\0';

      if (i18n->multibyte()) {
        XRectangle ink, logical;
        XmbTextExtents(screen.getToolbarStyle()->fontset, t, len, &ink,
                       &logical);
        frame.clock_w = logical.width;

        // ben's additional solution to pad some space beside the numbers
        //frame.clock_w +=
        //  screen.getToolbarStyle()->fontset_extents->max_logical_extent.width *
        //  4;

        // brad's solution, which is currently buggy, too big
        //frame.clock_w =
        //  screen.getToolbarStyle()->fontset_extents->max_logical_extent.width
        //  * len;
      } else {
        frame.clock_w = XTextWidth(screen.getToolbarStyle()->font, t, len);
        // ben's additional solution to pad some space beside the numbers
        //frame.clock_w += screen.getToolbarStyle()->font->max_bounds.width * 4;
        // brad's solution again, too big
        //frame.clock_w = screen.getToolbarStyle()->font->max_bounds.width * len;
      }
      frame.clock_w += (frame.bevel_w * 4);
      
      delete [] time_string;
    } else {
      frame.clock_w = 0;
    }
  } else {
    frame.clock_w = 0;
  }
#else // !HAVE_STRFTIME
  frame.clock_w =
    XTextWidth(screen.getToolbarStyle()->font,
	       i18n->getMessage(ToolbarSet, ToolbarNoStrftimeLength,
				"00:00000"),
	       strlen(i18n->getMessage(ToolbarSet, ToolbarNoStrftimeLength,
				       "00:00000"))) + (frame.bevel_w * 4);
#endif // HAVE_STRFTIME

  int i;
  unsigned int w = 0;
  frame.workspace_label_w = 0;

  for (i = 0; i < screen.getWorkspaceCount(); i++) {
    if (i18n->multibyte()) {
      XRectangle ink, logical;
      XmbTextExtents(screen.getToolbarStyle()->fontset,
		     screen.getWorkspace(i)->getName(),
		     strlen(screen.getWorkspace(i)->getName()),
		     &ink, &logical);
      w = logical.width;
    } else {
      w = XTextWidth(screen.getToolbarStyle()->font,
		     screen.getWorkspace(i)->getName(),
		     strlen(screen.getWorkspace(i)->getName()));
    }
    w += (frame.bevel_w * 4);

    if (w > frame.workspace_label_w) frame.workspace_label_w = w;
  }

  if (frame.workspace_label_w < frame.clock_w)
    frame.workspace_label_w = frame.clock_w;
  else if (frame.workspace_label_w > frame.clock_w)
    frame.clock_w = frame.workspace_label_w;

  frame.window_label_w =
    (frame.width - (frame.clock_w + (frame.button_w * 4) +
                    frame.workspace_label_w + (frame.bevel_w * 8) + 6));

  if (m_hidden) {
    XMoveResizeWindow(display, frame.window, frame.x_hidden, frame.y_hidden,
		      frame.width, frame.height);
  } else {
    XMoveResizeWindow(display, frame.window, frame.x, frame.y,
		      frame.width, frame.height);
  }

  XMoveResizeWindow(display, frame.workspace_label, frame.bevel_w,
		    frame.bevel_w, frame.workspace_label_w,
                    frame.label_h);
  XMoveResizeWindow(display, frame.psbutton, (frame.bevel_w * 2) +
                    frame.workspace_label_w + 1, frame.bevel_w + 1,
                    frame.button_w, frame.button_w);
  XMoveResizeWindow(display ,frame.nsbutton, (frame.bevel_w * 3) +
                    frame.workspace_label_w + frame.button_w + 2,
                    frame.bevel_w + 1, frame.button_w, frame.button_w);
  XMoveResizeWindow(display, frame.window_label, (frame.bevel_w * 4) +
                    (frame.button_w * 2) + frame.workspace_label_w + 3,
		    frame.bevel_w, frame.window_label_w, frame.label_h);
  XMoveResizeWindow(display, frame.pwbutton, (frame.bevel_w * 5) +
                    (frame.button_w * 2) + frame.workspace_label_w +
                    frame.window_label_w + 4, frame.bevel_w + 1,
                    frame.button_w, frame.button_w);
  XMoveResizeWindow(display, frame.nwbutton, (frame.bevel_w * 6) +
                    (frame.button_w * 3) + frame.workspace_label_w +
                    frame.window_label_w + 5, frame.bevel_w + 1,
                    frame.button_w, frame.button_w);
  XMoveResizeWindow(display, frame.clock, frame.width - frame.clock_w -
		    frame.bevel_w, frame.bevel_w, frame.clock_w,
		    frame.label_h);

  Pixmap tmp = frame.base;
  BTexture *texture = &(screen.getToolbarStyle()->toolbar);
  if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
    frame.base = None;
    XSetWindowBackground(display, frame.window,
			 texture->getColor()->getPixel());
  } else {
    frame.base =
      image_ctrl->renderImage(frame.width, frame.height, texture);
    XSetWindowBackgroundPixmap(display, frame.window, frame.base);
  }
  if (tmp) image_ctrl->removeImage(tmp);

  tmp = frame.label;
  texture = &(screen.getToolbarStyle()->window);
  if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
    frame.label = None;
    XSetWindowBackground(display, frame.window_label,
			 texture->getColor()->getPixel());
  } else {
    frame.label =
      image_ctrl->renderImage(frame.window_label_w, frame.label_h, texture);
    XSetWindowBackgroundPixmap(display, frame.window_label, frame.label);
  }
  if (tmp) image_ctrl->removeImage(tmp);

  tmp = frame.wlabel;
  texture = &(screen.getToolbarStyle()->label);
  if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
    frame.wlabel = None;
    XSetWindowBackground(display, frame.workspace_label,
			 texture->getColor()->getPixel());
  } else {
    frame.wlabel =
      image_ctrl->renderImage(frame.workspace_label_w, frame.label_h, texture);
    XSetWindowBackgroundPixmap(display, frame.workspace_label, frame.wlabel);
  }
  if (tmp) image_ctrl->removeImage(tmp);

  tmp = frame.clk;
  texture = &(screen.getToolbarStyle()->clock);
  if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
    frame.clk = None;
    XSetWindowBackground(display, frame.clock,
			 texture->getColor()->getPixel());
  } else {
    frame.clk =
      image_ctrl->renderImage(frame.clock_w, frame.label_h, texture);
    XSetWindowBackgroundPixmap(display, frame.clock, frame.clk);
  }
  if (tmp) image_ctrl->removeImage(tmp);

  tmp = frame.button;
  texture = &(screen.getToolbarStyle()->button);
  if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
    frame.button = None;

    frame.button_pixel = texture->getColor()->getPixel();
    XSetWindowBackground(display, frame.psbutton, frame.button_pixel);
    XSetWindowBackground(display, frame.nsbutton, frame.button_pixel);
    XSetWindowBackground(display, frame.pwbutton, frame.button_pixel);
    XSetWindowBackground(display, frame.nwbutton, frame.button_pixel);
  } else {
    frame.button =
      image_ctrl->renderImage(frame.button_w, frame.button_w, texture);

    XSetWindowBackgroundPixmap(display, frame.psbutton, frame.button);
    XSetWindowBackgroundPixmap(display, frame.nsbutton, frame.button);
    XSetWindowBackgroundPixmap(display, frame.pwbutton, frame.button);
    XSetWindowBackgroundPixmap(display, frame.nwbutton, frame.button);
  }
  if (tmp) image_ctrl->removeImage(tmp);

  tmp = frame.pbutton;
  texture = &(screen.getToolbarStyle()->pressed);
  if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
    frame.pbutton = None;
    frame.pbutton_pixel = texture->getColor()->getPixel();
  } else {
    frame.pbutton =
      image_ctrl->renderImage(frame.button_w, frame.button_w, texture);
  }
  if (tmp) image_ctrl->removeImage(tmp);

  XSetWindowBorder(display, frame.window,
		   screen.getBorderColor()->getPixel());
  XSetWindowBorderWidth(display, frame.window, screen.getBorderWidth());

  XClearWindow(display, frame.window);
  XClearWindow(display, frame.workspace_label);
  XClearWindow(display, frame.window_label);
  XClearWindow(display, frame.clock);
  XClearWindow(display, frame.psbutton);
  XClearWindow(display, frame.nsbutton);
  XClearWindow(display, frame.pwbutton);
  XClearWindow(display, frame.nwbutton);
  
  redrawWindowLabel();
  redrawWorkspaceLabel();
  redrawPrevWorkspaceButton();
  redrawNextWorkspaceButton();
  redrawPrevWindowButton();
  redrawNextWindowButton();
  checkClock(True);
  
  toolbarmenu->reconfigure();
}


#ifdef    HAVE_STRFTIME
void Toolbar::checkClock(Bool redraw) {
#else // !HAVE_STRFTIME
void Toolbar::checkClock(Bool redraw, Bool date) {
#endif // HAVE_STRFTIME
  time_t tmp = 0;
  struct tm *tt = 0;

  if ((tmp = time(NULL)) != -1) {
    if (! (tt = localtime(&tmp))) return;
    if (tt->tm_min != frame.minute || tt->tm_hour != frame.hour) {
      frame.hour = tt->tm_hour;
      frame.minute = tt->tm_min;
      XClearWindow(display, frame.clock);
      redraw = True;
    }
  }

  if (redraw) {
#ifdef    HAVE_STRFTIME
    char t[1024];
    if (! strftime(t, 1024, screen.strftimeFormat(), tt))
      return;
#else // !HAVE_STRFTIME
    char t[9];
    if (date) {
      // format the date... with special consideration for y2k ;)
      if (screen.getDateFormat() == Openbox::B_EuropeanDate)
        sprintf(t, 18n->getMessage(ToolbarSet, ToolbarNoStrftimeDateFormatEu,
				   "%02d.%02d.%02d"),
		tt->tm_mday, tt->tm_mon + 1,
                (tt->tm_year >= 100) ? tt->tm_year - 100 : tt->tm_year);
      else
        sprintf(t, i18n->getMessage(ToolbarSet, ToolbarNoStrftimeDateFormat,
				    "%02d/%02d/%02d"),
		tt->tm_mon + 1, tt->tm_mday,
                (tt->tm_year >= 100) ? tt->tm_year - 100 : tt->tm_year);
    } else {
      if (screen.isClock24Hour())
	sprintf(t, i18n->getMessage(ToolbarSet, ToolbarNoStrftimeTimeFormat24,
				    "  %02d:%02d "),
		frame.hour, frame.minute);
      else
	sprintf(t, i18n->getMessage(ToolbarSet, ToolbarNoStrftimeTimeFormat12,
				    "%02d:%02d %sm"),
		((frame.hour > 12) ? frame.hour - 12 :
		 ((frame.hour == 0) ? 12 : frame.hour)), frame.minute,
		((frame.hour >= 12) ?
		 i18n->getMessage(ToolbarSet,
				  ToolbarNoStrftimeTimeFormatP, "p") :
		 i18n->getMessage(ToolbarSet,
				  ToolbarNoStrftimeTimeFormatA, "a")));
    }
#endif // HAVE_STRFTIME

    int dx = (frame.bevel_w * 2), dlen = strlen(t);
    unsigned int l;

    if (i18n->multibyte()) {
      XRectangle ink, logical;
      XmbTextExtents(screen.getToolbarStyle()->fontset,
		     t, dlen, &ink, &logical);
      l = logical.width;
    } else {
      l = XTextWidth(screen.getToolbarStyle()->font, t, dlen);
    }
    
    l += (frame.bevel_w * 4);
    
    if (l > frame.clock_w) {
      for (; dlen >= 0; dlen--) {
	if (i18n->multibyte()) {
	  XRectangle ink, logical;
	  XmbTextExtents(screen.getToolbarStyle()->fontset,
			 t, dlen, &ink, &logical);
	  l = logical.width;
	} else {
	  l = XTextWidth(screen.getToolbarStyle()->font, t, dlen);
	}
	l+= (frame.bevel_w * 4);
	
        if (l < frame.clock_w)
          break;
      }
    }
    switch (screen.getToolbarStyle()->justify) {
    case BScreen::RightJustify:
      dx += frame.clock_w - l;
      break;

    case BScreen::CenterJustify:
      dx += (frame.clock_w - l) / 2;
      break;
    }

    ToolbarStyle *style = screen.getToolbarStyle();
    if (i18n->multibyte())
      XmbDrawString(display, frame.clock, style->fontset, style->c_text_gc,
		    dx, (1 - style->fontset_extents->max_ink_extent.y),
		    t, dlen);
    else
      XDrawString(display, frame.clock, style->c_text_gc, dx,
		  (style->font->ascent + 1), t, dlen);
  }
}


void Toolbar::redrawWindowLabel(Bool redraw) {
  OpenboxWindow *foc = screen.getOpenbox().focusedWindow();
  if (foc != (OpenboxWindow *) 0) {
    if (redraw)
      XClearWindow(display, frame.window_label);

    if (foc->getScreen() != &screen) return;

    int dx = (frame.bevel_w * 2), dlen = strlen(*foc->getTitle());
    unsigned int l;

    if (i18n->multibyte()) {
      XRectangle ink, logical;
      XmbTextExtents(screen.getToolbarStyle()->fontset, *foc->getTitle(),
		     dlen, &ink, &logical);
      l = logical.width;
    } else {
      l = XTextWidth(screen.getToolbarStyle()->font, *foc->getTitle(), dlen);
    }
    l += (frame.bevel_w * 4);

    if (l > frame.window_label_w) {
      for (; dlen >= 0; dlen--) {
	if (i18n->multibyte()) {
	  XRectangle ink, logical;
	  XmbTextExtents(screen.getToolbarStyle()->fontset,
			 *foc->getTitle(), dlen, &ink, &logical);
	  l = logical.width;
	} else {
	  l = XTextWidth(screen.getToolbarStyle()->font,
			 *foc->getTitle(), dlen);
	}
	l += (frame.bevel_w * 4);
	
	if (l < frame.window_label_w)
          break;
      }
    }
    switch (screen.getToolbarStyle()->justify) {
    case BScreen::RightJustify:
      dx += frame.window_label_w - l;
      break;

    case BScreen::CenterJustify:
      dx += (frame.window_label_w - l) / 2;
      break;
    }

    ToolbarStyle *style = screen.getToolbarStyle();
    if (i18n->multibyte())
      XmbDrawString(display, frame.window_label, style->fontset,
		    style->w_text_gc, dx,
		    (1 - style->fontset_extents->max_ink_extent.y),
		    *foc->getTitle(), dlen);
    else
      XDrawString(display, frame.window_label, style->w_text_gc, dx,
		  (style->font->ascent + 1), *foc->getTitle(), dlen);
  } else {
    XClearWindow(display, frame.window_label);
  }
}
 
 
void Toolbar::redrawWorkspaceLabel(Bool redraw) {
  if (screen.getCurrentWorkspace()->getName()) {
    if (redraw)
      XClearWindow(display, frame.workspace_label);
    
    int dx = (frame.bevel_w * 2), dlen =
	     strlen(screen.getCurrentWorkspace()->getName());
    unsigned int l;
    
    if (i18n->multibyte()) {
      XRectangle ink, logical;
      XmbTextExtents(screen.getToolbarStyle()->fontset,
		     screen.getCurrentWorkspace()->getName(), dlen,
		     &ink, &logical);
      l = logical.width;
    } else {
      l = XTextWidth(screen.getToolbarStyle()->font,
		     screen.getCurrentWorkspace()->getName(), dlen);
    }
    l += (frame.bevel_w * 4);
    
    if (l > frame.workspace_label_w) {
      for (; dlen >= 0; dlen--) {
	if (i18n->multibyte()) {
	  XRectangle ink, logical;
	  XmbTextExtents(screen.getToolbarStyle()->fontset,
			 screen.getCurrentWorkspace()->getName(), dlen,
			 &ink, &logical);
	  l = logical.width;
	} else {
	  l = XTextWidth(screen.getWindowStyle()->font,
			 screen.getCurrentWorkspace()->getName(), dlen);
	}
	l += (frame.bevel_w * 4);
	
        if (l < frame.workspace_label_w)
          break;
      }
    }
    switch (screen.getToolbarStyle()->justify) {
    case BScreen::RightJustify:
      dx += frame.workspace_label_w - l;
      break;

    case BScreen::CenterJustify:
      dx += (frame.workspace_label_w - l) / 2;
      break;
    }

    ToolbarStyle *style = screen.getToolbarStyle();
    if (i18n->multibyte())
      XmbDrawString(display, frame.workspace_label, style->fontset,
		    style->l_text_gc, dx,
		    (1 - style->fontset_extents->max_ink_extent.y),
		    (char *) screen.getCurrentWorkspace()->getName(), dlen);
    else
      XDrawString(display, frame.workspace_label, style->l_text_gc, dx,
		  (style->font->ascent + 1),
		  (char *) screen.getCurrentWorkspace()->getName(), dlen);
  }
}


void Toolbar::redrawPrevWorkspaceButton(Bool pressed, Bool redraw) {
  if (redraw) {
    if (pressed) {
      if (frame.pbutton)
	XSetWindowBackgroundPixmap(display, frame.psbutton, frame.pbutton);
      else
	XSetWindowBackground(display, frame.psbutton, frame.pbutton_pixel);
    } else {
      if (frame.button)
        XSetWindowBackgroundPixmap(display, frame.psbutton, frame.button);
      else
	XSetWindowBackground(display, frame.psbutton, frame.button_pixel);
    }
    XClearWindow(display, frame.psbutton);
  }

  int hh = frame.button_w / 2, hw = frame.button_w / 2;

  XPoint pts[3];
  pts[0].x = hw - 2; pts[0].y = hh;
  pts[1].x = 4; pts[1].y = 2;
  pts[2].x = 0; pts[2].y = -4;

  XFillPolygon(display, frame.psbutton, screen.getToolbarStyle()->b_pic_gc,
               pts, 3, Convex, CoordModePrevious);
}


void Toolbar::redrawNextWorkspaceButton(Bool pressed, Bool redraw) {
  if (redraw) {
    if (pressed) {
      if (frame.pbutton)
	XSetWindowBackgroundPixmap(display, frame.nsbutton, frame.pbutton);
      else
	XSetWindowBackground(display, frame.nsbutton, frame.pbutton_pixel);
    } else {
      if (frame.button)
        XSetWindowBackgroundPixmap(display, frame.nsbutton, frame.button);
      else
	XSetWindowBackground(display, frame.nsbutton, frame.button_pixel);
    }
    XClearWindow(display, frame.nsbutton);
  }

  int hh = frame.button_w / 2, hw = frame.button_w / 2;

  XPoint pts[3];
  pts[0].x = hw - 2; pts[0].y = hh - 2;
  pts[1].x = 4; pts[1].y =  2;
  pts[2].x = -4; pts[2].y = 2;

  XFillPolygon(display, frame.nsbutton, screen.getToolbarStyle()->b_pic_gc,
               pts, 3, Convex, CoordModePrevious);
}


void Toolbar::redrawPrevWindowButton(Bool pressed, Bool redraw) {
  if (redraw) {
    if (pressed) {
      if (frame.pbutton)
	XSetWindowBackgroundPixmap(display, frame.pwbutton, frame.pbutton);
      else
	XSetWindowBackground(display, frame.pwbutton, frame.pbutton_pixel);
    } else {
      if (frame.button)
        XSetWindowBackgroundPixmap(display, frame.pwbutton, frame.button);
      else
	XSetWindowBackground(display, frame.pwbutton, frame.button_pixel);
    }
    XClearWindow(display, frame.pwbutton);
  }

  int hh = frame.button_w / 2, hw = frame.button_w / 2;

  XPoint pts[3];
  pts[0].x = hw - 2; pts[0].y = hh;
  pts[1].x = 4; pts[1].y = 2;
  pts[2].x = 0; pts[2].y = -4;

  XFillPolygon(display, frame.pwbutton, screen.getToolbarStyle()->b_pic_gc,
               pts, 3, Convex, CoordModePrevious);
}


void Toolbar::redrawNextWindowButton(Bool pressed, Bool redraw) {
  if (redraw) {
    if (pressed) {
      if (frame.pbutton)
	XSetWindowBackgroundPixmap(display, frame.nwbutton, frame.pbutton);
      else
	XSetWindowBackground(display, frame.nwbutton, frame.pbutton_pixel);
    } else {
      if (frame.button)
        XSetWindowBackgroundPixmap(display, frame.nwbutton, frame.button);
      else
	XSetWindowBackground(display, frame.nwbutton, frame.button_pixel);
    }
    XClearWindow(display, frame.nwbutton);
  }

  int hh = frame.button_w / 2, hw = frame.button_w / 2;

  XPoint pts[3];
  pts[0].x = hw - 2; pts[0].y = hh - 2;
  pts[1].x = 4; pts[1].y =  2;
  pts[2].x = -4; pts[2].y = 2;

  XFillPolygon(display, frame.nwbutton, screen.getToolbarStyle()->b_pic_gc,
               pts, 3, Convex, CoordModePrevious);
}


void Toolbar::edit() {
  Window window;
  int foo;

  m_editing = True;
  if (XGetInputFocus(display, &window, &foo) &&
      window == frame.workspace_label)
    return;

  XSetInputFocus(display, frame.workspace_label,
                 RevertToPointerRoot, CurrentTime);
  XClearWindow(display, frame.workspace_label);

  openbox.setNoFocus(True);
  if (openbox.focusedWindow())
    openbox.focusedWindow()->setFocusFlag(False);

  XDrawRectangle(display, frame.workspace_label,
                 screen.getWindowStyle()->l_text_focus_gc,
                 frame.workspace_label_w / 2, 0, 1,
                 frame.label_h - 1);
  
  // change the background of the window to that of an active window label
  Pixmap tmp = frame.wlabel;
  BTexture *texture = &(screen.getWindowStyle()->l_focus);
  if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
    frame.wlabel = None;
    XSetWindowBackground(display, frame.workspace_label,
			 texture->getColor()->getPixel());
  } else {
    frame.wlabel =
      image_ctrl->renderImage(frame.workspace_label_w, frame.label_h, texture);
    XSetWindowBackgroundPixmap(display, frame.workspace_label, frame.wlabel);
  }
  if (tmp) image_ctrl->removeImage(tmp);
}


void Toolbar::buttonPressEvent(XButtonEvent *be) {
  if (be->button == 1) {
    if (be->window == frame.psbutton)
      redrawPrevWorkspaceButton(True, True);
    else if (be->window == frame.nsbutton)
      redrawNextWorkspaceButton(True, True);
    else if (be->window == frame.pwbutton)
      redrawPrevWindowButton(True, True);
    else if (be->window == frame.nwbutton)
      redrawNextWindowButton(True, True);
#ifndef   HAVE_STRFTIME
    else if (be->window == frame.clock) {
      XClearWindow(display, frame.clock);
      checkClock(True, True);
    }
#endif // HAVE_STRFTIME
    else if (! m_ontop) {
      Window w[1] = { frame.window };
      screen.raiseWindows(w, 1);
    }
  } else if (be->button == 2 && (! m_ontop)) {
    XLowerWindow(display, frame.window);
  } else if (be->button == 3) {
    if (! toolbarmenu->isVisible()) {
      int x, y;

      x = be->x_root - (toolbarmenu->getWidth() / 2);
      y = be->y_root - (toolbarmenu->getHeight() / 2);

      if (x < 0)
        x = 0;
      else if (x + toolbarmenu->getWidth() > screen.size().w())
        x = screen.size().w() - toolbarmenu->getWidth();

      if (y < 0)
        y = 0;
      else if (y + toolbarmenu->getHeight() > screen.size().h())
        y = screen.size().h() - toolbarmenu->getHeight();

      toolbarmenu->move(x, y);
      toolbarmenu->show();
    } else
      toolbarmenu->hide();
  }
}



void Toolbar::buttonReleaseEvent(XButtonEvent *re) {
  if (re->button == 1) {
    if (re->window == frame.psbutton) {
      redrawPrevWorkspaceButton(False, True);

      if (re->x >= 0 && re->x < (signed) frame.button_w &&
          re->y >= 0 && re->y < (signed) frame.button_w)
       if (screen.getCurrentWorkspace()->getWorkspaceID() > 0)
          screen.changeWorkspaceID(screen.getCurrentWorkspace()->
                                    getWorkspaceID() - 1);
        else
          screen.changeWorkspaceID(screen.getWorkspaceCount() - 1);
    } else if (re->window == frame.nsbutton) {
      redrawNextWorkspaceButton(False, True);

      if (re->x >= 0 && re->x < (signed) frame.button_w &&
          re->y >= 0 && re->y < (signed) frame.button_w)
        if (screen.getCurrentWorkspace()->getWorkspaceID() <
            screen.getWorkspaceCount() - 1)
          screen.changeWorkspaceID(screen.getCurrentWorkspace()->
                                    getWorkspaceID() + 1);
        else
          screen.changeWorkspaceID(0);
    } else if (re->window == frame.pwbutton) {
      redrawPrevWindowButton(False, True);

      if (re->x >= 0 && re->x < (signed) frame.button_w &&
          re->y >= 0 && re->y < (signed) frame.button_w)
        screen.prevFocus();
    } else if (re->window == frame.nwbutton) {
      redrawNextWindowButton(False, True);

      if (re->x >= 0 && re->x < (signed) frame.button_w &&
          re->y >= 0 && re->y < (signed) frame.button_w)
        screen.nextFocus();
    } else if (re->window == frame.window_label)
      screen.raiseFocus();
#ifndef   HAVE_STRFTIME
    else if (re->window == frame.clock) {
      XClearWindow(display, frame.clock);
      checkClock(True);
    }
#endif // HAVE_STRFTIME
  }
}


void Toolbar::enterNotifyEvent(XCrossingEvent *) {
  if (! m_autohide)
    return;

  if (m_hidden) {
    if (! hide_timer->isTiming()) hide_timer->start();
  } else {
    if (hide_timer->isTiming()) hide_timer->stop();
  }
}

void Toolbar::leaveNotifyEvent(XCrossingEvent *) {
  if (! m_autohide)
    return;

  if (m_hidden) {
    if (hide_timer->isTiming()) hide_timer->stop();
  } else if (! toolbarmenu->isVisible()) {
    if (! hide_timer->isTiming()) hide_timer->start();
  }
}


void Toolbar::exposeEvent(XExposeEvent *ee) {
  if (ee->window == frame.clock) checkClock(True);
  else if (ee->window == frame.workspace_label && (! m_editing))
    redrawWorkspaceLabel();
  else if (ee->window == frame.window_label) redrawWindowLabel();
  else if (ee->window == frame.psbutton) redrawPrevWorkspaceButton();
  else if (ee->window == frame.nsbutton) redrawNextWorkspaceButton();
  else if (ee->window == frame.pwbutton) redrawPrevWindowButton();
  else if (ee->window == frame.nwbutton) redrawNextWindowButton();
}


void Toolbar::keyPressEvent(XKeyEvent *ke) {
  if (ke->window == frame.workspace_label && m_editing) {
    openbox.grab();

    if (! new_workspace_name) {
      new_workspace_name = new char[128];
      new_name_pos = 0;

      if (! new_workspace_name) return;
    }

    KeySym ks;
    char keychar[1];
    XLookupString(ke, keychar, 1, &ks, 0);

    // either we are told to end with a return or we hit the end of the buffer
    if (ks == XK_Return || new_name_pos == 127) {
      *(new_workspace_name + new_name_pos) = 0;

      m_editing = False;

      openbox.setNoFocus(False);
      if (openbox.focusedWindow()) {
        openbox.focusedWindow()->setInputFocus();
        openbox.focusedWindow()->setFocusFlag(True);
      } else
        openbox.focusWindow((OpenboxWindow *) 0);

      // check to make sure that new_name[0] != 0... otherwise we have a null
      // workspace name which causes serious problems, especially for the
      // Openbox::LoadRC() method.
      if (*new_workspace_name) {
 	screen.getCurrentWorkspace()->setName(new_workspace_name);
	screen.getCurrentWorkspace()->getMenu()->hide();
	screen.getWorkspacemenu()->
	  remove(screen.getCurrentWorkspace()->getWorkspaceID() + 2);
	screen.getWorkspacemenu()->
	  insert(screen.getCurrentWorkspace()->getName(),
		 screen.getCurrentWorkspace()->getMenu(),
		 screen.getCurrentWorkspace()->getWorkspaceID() + 2);
	screen.getWorkspacemenu()->update();
      }

      delete [] new_workspace_name;
      new_workspace_name = (char *) 0;
      new_name_pos = 0;

      // reset the background to that of the workspace label (its normal
      // setting)
      Pixmap tmp = frame.wlabel;
      BTexture *texture = &(screen.getToolbarStyle()->label);
      if (texture->getTexture() == (BImage_Flat | BImage_Solid)) {
        frame.wlabel = None;
        XSetWindowBackground(display, frame.workspace_label,
                            texture->getColor()->getPixel());
      } else {
        frame.wlabel =
          image_ctrl->renderImage(frame.workspace_label_w, frame.label_h, texture);
        XSetWindowBackgroundPixmap(display, frame.workspace_label, frame.wlabel);
      }
      if (tmp) image_ctrl->removeImage(tmp);
  
      reconfigure();
    } else if (! (ks == XK_Shift_L || ks == XK_Shift_R ||
		  ks == XK_Control_L || ks == XK_Control_R ||
		  ks == XK_Caps_Lock || ks == XK_Shift_Lock ||
		  ks == XK_Meta_L || ks == XK_Meta_R ||
		  ks == XK_Alt_L || ks == XK_Alt_R ||
		  ks == XK_Super_L || ks == XK_Super_R ||
		  ks == XK_Hyper_L || ks == XK_Hyper_R)) {
      if (ks == XK_BackSpace) {
	if (new_name_pos > 0) {
	  --new_name_pos;
	  *(new_workspace_name + new_name_pos) = '\0';
	} else {
	  *new_workspace_name = '\0';
	}
      } else {
	*(new_workspace_name + new_name_pos) = *keychar;
	++new_name_pos;
	*(new_workspace_name + new_name_pos) = '\0';
      }

      XClearWindow(display, frame.workspace_label);
      int l = strlen(new_workspace_name), tw, x;

      if (i18n->multibyte()) {
	XRectangle ink, logical;
	XmbTextExtents(screen.getToolbarStyle()->fontset,
		       new_workspace_name, l, &ink, &logical);
	tw = logical.width;
      } else {
	tw = XTextWidth(screen.getToolbarStyle()->font,
			new_workspace_name, l);
      }
      x = (frame.workspace_label_w - tw) / 2;

      if (x < (signed) frame.bevel_w) x = frame.bevel_w;

      WindowStyle *style = screen.getWindowStyle();
      if (i18n->multibyte())
	XmbDrawString(display, frame.workspace_label, style->fontset,
		      style->l_text_focus_gc, x,
		      (1 - style->fontset_extents->max_ink_extent.y),
		      new_workspace_name, l);
      else
	XDrawString(display, frame.workspace_label, style->l_text_focus_gc, x,
		    (style->font->ascent + 1),
		    new_workspace_name, l);
      
      XDrawRectangle(display, frame.workspace_label,
		     screen.getWindowStyle()->l_text_focus_gc, x + tw, 0, 1,
		     frame.label_h - 1);
    }
    
    openbox.ungrab();
  }
}


void Toolbar::timeout() {
  checkClock(True);

  timeval now;
  gettimeofday(&now, 0);
  clock_timer->setTimeout((60 - (now.tv_sec % 60)) * 1000);
}


void Toolbar::HideHandler::timeout() {
  toolbar->m_hidden = !toolbar->m_hidden;
  if (toolbar->m_hidden)
    XMoveWindow(toolbar->display, toolbar->frame.window,
		toolbar->frame.x_hidden, toolbar->frame.y_hidden);
  else
    XMoveWindow(toolbar->display, toolbar->frame.window,
		toolbar->frame.x, toolbar->frame.y);
}


Toolbarmenu::Toolbarmenu(Toolbar &tb) : Basemenu(tb.screen), toolbar(tb) {
  setLabel(i18n->getMessage(ToolbarSet, ToolbarToolbarTitle, "Toolbar"));
  setInternalMenu();

  placementmenu = new Placementmenu(*this);

  insert(i18n->getMessage(CommonSet, CommonPlacementTitle, "Placement"),
	 placementmenu);
  insert(i18n->getMessage(CommonSet, CommonAlwaysOnTop, "Always on top"), 1);
  insert(i18n->getMessage(CommonSet, CommonAutoHide, "Auto hide"), 2);
  insert(i18n->getMessage(ToolbarSet, ToolbarEditWkspcName,
			  "Edit current workspace name"), 3);

  update();
 
  setValues();
}

void Toolbarmenu::setValues() {
  setItemSelected(1, toolbar.onTop());
  setItemSelected(2, toolbar.autoHide());
}


Toolbarmenu::~Toolbarmenu() {
  delete placementmenu;
}


void Toolbarmenu::itemSelected(int button, int index) {
  if (button != 1)
    return;

  BasemenuItem *item = find(index);
  if (! item) return;

  switch (item->function()) {
  case 1: { // always on top
    Bool change = ((toolbar.onTop()) ? False : True);
    toolbar.setOnTop(change);
    setItemSelected(1, change);

    if (toolbar.onTop()) toolbar.screen.raiseWindows((Window *) 0, 0);
    break;
  }

  case 2: { // auto hide
    Bool change = ((toolbar.autoHide()) ?  False : True);
    toolbar.setAutoHide(change);
    setItemSelected(2, change);

#ifdef    SLIT
    toolbar.screen.getSlit()->reposition();
#endif // SLIT
    break;
  }

  case 3: { // edit current workspace name
    toolbar.edit();
    hide();

    break;
  }
  } // switch
}


void Toolbarmenu::internal_hide() {
  Basemenu::internal_hide();
  if (toolbar.autoHide() && ! toolbar.isEditing())
    toolbar.hide_handler.timeout();
}


void Toolbarmenu::reconfigure() {
  setValues();
  placementmenu->reconfigure();

  Basemenu::reconfigure();
}


Toolbarmenu::Placementmenu::Placementmenu(Toolbarmenu &tm)
  : Basemenu(tm.toolbar.screen), toolbarmenu(tm) {
  setLabel(i18n->getMessage(ToolbarSet, ToolbarToolbarPlacement,
			    "Toolbar Placement"));
  setInternalMenu();
  setMinimumSublevels(3);

  insert(i18n->getMessage(CommonSet, CommonPlacementTopLeft,
			  "Top Left"), Toolbar::TopLeft);
  insert(i18n->getMessage(CommonSet, CommonPlacementBottomLeft,
			  "Bottom Left"), Toolbar::BottomLeft);
  insert(i18n->getMessage(CommonSet, CommonPlacementTopCenter,
			  "Top Center"), Toolbar::TopCenter);
  insert(i18n->getMessage(CommonSet, CommonPlacementBottomCenter,
			  "Bottom Center"), Toolbar::BottomCenter);
  insert(i18n->getMessage(CommonSet, CommonPlacementTopRight,
			  "Top Right"), Toolbar::TopRight);
  insert(i18n->getMessage(CommonSet, CommonPlacementBottomRight,
			  "Bottom Right"), Toolbar::BottomRight);
  update();
}

void Toolbarmenu::Placementmenu::itemSelected(int button, int index) {
  if (button != 1)
    return;

  BasemenuItem *item = find(index);
  if (! item) return;

  toolbarmenu.toolbar.setPlacement(item->function());
  hide();
  toolbarmenu.toolbar.reconfigure();

#ifdef    SLIT
  // reposition the slit as well to make sure it doesn't intersect the
  // toolbar
  toolbarmenu.toolbar.screen.getSlit()->reposition();
#endif // SLIT
}
