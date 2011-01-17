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

#ifndef ___EVENT_PYEVENTBASE__H___
#define ___EVENT_PYEVENTBASE__H___

#include <Python.h>
#include <event2/event.h>

#if defined(WITH_THREAD)
#define START_BLOCK_THREADS \
    PyGILState_STATE __savestate = PyGILState_Ensure();
#define END_BLOCK_THREADS \
    PyGILState_Release(__savestate);
#else
#define START_BLOCK_THREADS
#define END_BLOCK_THREADS
#endif

#if !defined(Py_TYPE)
#define Py_TYPE(ob) (((PyObject*)(ob))->ob_type)
#endif

#if !defined(PyLong_FromSsize_t)
#define PyLong_FromSsize_t(v) PyLong_FromLong(v)
#endif

typedef struct _PyEventBaseObject {
    PyObject_HEAD
    struct event_base *base;
    PyObject *method;
    int features;
    PyObject *error_type;
    PyObject *error_value;
    PyObject *error_traceback;
} PyEventBaseObject;

extern PyTypeObject PyEventBase_Type;
extern PyTypeObject PyConfig_Type;

extern void timeval_init(struct timeval *tv, double time);
extern void pybase_store_error(PyEventBaseObject *self);

#define PyEventBase_Check(ob) ((ob)->ob_type == &PyEventBase_Type)

#endif
