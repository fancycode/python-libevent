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

#ifndef ___EVENT_PYBUFFER__H___
#define ___EVENT_PYBUFFER__H___

typedef struct _PyBufferObject {
    PyObject_HEAD
    struct evbuffer *buffer;
    PyEventBaseObject *base;
    int owned;
} PyBufferObject;

extern PyTypeObject PyEventBuffer_Type;
extern PyBufferObject *_pybuffer_create(struct evbuffer *buffer);

#define PyEventBuffer_Check(ob) ((ob)->ob_type == &PyEventBuffer_Type)

#endif
