/* 
 * Copyright © 2008-2013 Kristian Høgsberg
 * Copyright © 2013      Rafael Antognolli
 * Copyright © 2013      Jasper St. Pierre
 * Copyright © 2010-2013 Intel Corporation
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface xdg_popup_interface;
extern const struct wl_interface xdg_surface_interface;

static const struct wl_interface *types[] = {
  NULL,
  NULL,
  NULL,
  NULL,
  &xdg_surface_interface,
  &wl_surface_interface,
  &xdg_popup_interface,
  &wl_surface_interface,
  &wl_surface_interface,
  &wl_seat_interface,
  NULL,
  NULL,
  NULL,
  &xdg_surface_interface,
  &wl_seat_interface,
  NULL,
  NULL,
  NULL,
  &wl_seat_interface,
  NULL,
  &wl_seat_interface,
  NULL,
  NULL,
  &wl_output_interface,
};

static const struct wl_message xdg_shell_requests[] = {
  { "destroy", "", types + 0 },
  { "use_unstable_version", "i", types + 0 },
  { "get_xdg_surface", "no", types + 4 },
  { "get_xdg_popup", "nooouii", types + 6 },
  { "pong", "u", types + 0 },
};

static const struct wl_message xdg_shell_events[] = {
  { "ping", "u", types + 0 },
};

WL_EXPORT const struct wl_interface xdg_shell_interface = {
  "xdg_shell", 1,
  5, xdg_shell_requests,
  1, xdg_shell_events,
};

static const struct wl_message xdg_surface_requests[] = {
  { "destroy", "", types + 0 },
  { "set_parent", "?o", types + 13 },
  { "set_title", "s", types + 0 },
  { "set_app_id", "s", types + 0 },
  { "show_window_menu", "ouii", types + 14 },
  { "move", "ou", types + 18 },
  { "resize", "ouu", types + 20 },
  { "ack_configure", "u", types + 0 },
  { "set_window_geometry", "iiii", types + 0 },
  { "set_maximized", "", types + 0 },
  { "unset_maximized", "", types + 0 },
  { "set_fullscreen", "?o", types + 23 },
  { "unset_fullscreen", "", types + 0 },
  { "set_minimized", "", types + 0 },
};

static const struct wl_message xdg_surface_events[] = {
  { "configure", "iiau", types + 0 },
  { "close", "", types + 0 },
};

WL_EXPORT const struct wl_interface xdg_surface_interface = {
  "xdg_surface", 1,
  14, xdg_surface_requests,
  2, xdg_surface_events,
};

static const struct wl_message xdg_popup_requests[] = {
  { "destroy", "", types + 0 },
};

static const struct wl_message xdg_popup_events[] = {
  { "popup_done", "", types + 0 },
};

WL_EXPORT const struct wl_interface xdg_popup_interface = {
  "xdg_popup", 1,
  1, xdg_popup_requests,
  1, xdg_popup_events,
};

