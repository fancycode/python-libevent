/*
 * Python Bindings for libevent
 *
 * Copyright (c) 2010-2011 by Joachim Bauch, mail@joachim-bauch.de
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ___EVENT_PYHTTP__H___
#define ___EVENT_PYHTTP__H___

extern PyTypeObject PyHttpServer_Type;
extern PyTypeObject PyBoundSocket_Type;
extern PyTypeObject PyHttpCallback_Type;
extern PyTypeObject PyHttpRequest_Type;

#define PyHttpServer_Check(ob) ((ob)->ob_type == &PyHttpServer_Type)
#define PyBoundSocket_Check(ob) ((ob)->ob_type == &PyBoundSocket_Type)
#define PyHttpCallback_Check(ob) ((ob)->ob_type == &PyHttpCallback_Type)
#define PyHttpRequest_Check(ob) ((ob)->ob_type == &PyHttpRequest_Type)

#endif
