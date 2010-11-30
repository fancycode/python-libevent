/*
 * Python Bindings for libevent
 *
 * Copyright (c) 2010 by Joachim Bauch, mail@joachim-bauch.de
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

#include <Python.h>
#include <structmember.h>

#include <event2/event.h>
#include <event2/util.h>

#include "pybase.h"

void
timeval_init(struct timeval *tv, double time)
{
    tv->tv_sec = (time_t) time;
    tv->tv_usec = (suseconds_t) ((time - tv->tv_sec) * 1000000);
}

static PyObject *
base_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyBaseObject *s;
    s = (PyBaseObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->base = NULL;
        s->method = NULL;
        s->features = 0;
    }
    return (PyObject *)s;
}

static int
base_init(PyBaseObject *self, PyObject *args, PyObject *kwds)
{
    if (!PyArg_ParseTuple(args, "", args))
        return -1;

    self->base = event_base_new();
    if (self->base == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->method = PyString_FromString(event_base_get_method(self->base));
    self->features = event_base_get_features(self->base);
    return 0;
}

static void
base_dealloc(PyBaseObject *self)
{
    Py_DECREF(self->method);
    if (self->base != NULL) {
        Py_BEGIN_ALLOW_THREADS
        event_base_free(self->base);
        Py_END_ALLOW_THREADS
    }
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(base_reinit_doc, "Reinitialized the event base after a fork.");

static PyObject *
base_reinit(PyBaseObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    event_reinit(self->base);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(base_dispatch_doc, "Threadsafe event dispatching loop.");

static PyObject *
base_dispatch(PyBaseObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    event_base_dispatch(self->base);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(base_loop_doc, "Handle events (threadsafe version).");

static PyObject *
base_loop(PyBaseObject *self, PyObject *args)
{
    int flags=0;
    if (!PyArg_ParseTuple(args, "|i", &flags))
        return NULL;
        
    Py_BEGIN_ALLOW_THREADS
    event_base_loop(self->base, flags);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(base_loopexit_doc, "Exit the event loop after the specified time (threadsafe variant).");

static PyObject *
base_loopexit(PyBaseObject *self, PyObject *args)
{
    double duration;
    struct timeval tv;
    if (!PyArg_ParseTuple(args, "d", &duration))
        return NULL;

    timeval_init(&tv, duration);
    Py_BEGIN_ALLOW_THREADS
    event_base_loopexit(self->base, &tv);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(base_loopbreak_doc, "Abort the active loop() immediately.");

static PyObject *
base_loopbreak(PyBaseObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    event_base_loopbreak(self->base);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(base_got_exit_doc, "Checks if the event loop was told to exit by loopexit().");

static PyObject *
base_got_exit(PyBaseObject *self, PyObject *args)
{
    int result;
    Py_BEGIN_ALLOW_THREADS
    result = event_base_got_exit(self->base);
    Py_END_ALLOW_THREADS;
    return PyBool_FromLong(result);
}

PyDoc_STRVAR(base_got_break_doc, "Checks if the event loop was told to abort immediately by loopbreak().");

static PyObject *
base_got_break(PyBaseObject *self, PyObject *args)
{
    int result;
    Py_BEGIN_ALLOW_THREADS
    result = event_base_got_break(self->base);
    Py_END_ALLOW_THREADS;
    return PyBool_FromLong(result);
}

PyDoc_STRVAR(base_priority_init_doc, "Set the number of different event priorities (threadsafe variant).");

static PyObject *
base_priority_init(PyBaseObject *self, PyObject *args)
{
    int priorities;
    if (!PyArg_ParseTuple(args, "i", &priorities))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    event_base_priority_init(self->base, priorities);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyMethodDef
base_methods[] = {
    {"reinit", (PyCFunction)base_reinit, METH_NOARGS, base_reinit_doc},
    {"dispatch", (PyCFunction)base_dispatch, METH_NOARGS, base_dispatch_doc},
    {"loop", (PyCFunction)base_loop, METH_VARARGS, base_loop_doc},
    {"loopexit", (PyCFunction)base_loopexit, METH_VARARGS, base_loopexit_doc},
    {"loopbreak", (PyCFunction)base_loopbreak, METH_NOARGS, base_loopbreak_doc},
    {"got_exit", (PyCFunction)base_got_exit, METH_NOARGS, base_got_exit_doc},
    {"got_break", (PyCFunction)base_got_break, METH_NOARGS, base_got_break_doc},
    {"priority_init", (PyCFunction)base_priority_init, METH_VARARGS, base_priority_init_doc},
    {NULL, NULL},
};

static PyMemberDef
base_members[] = {
    {"method", T_OBJECT, offsetof(PyBaseObject, method), READONLY, "kernel event notification mechanism"},
    {"features", T_INT, offsetof(PyBaseObject, features), READONLY, "bitmask of the features implemented"},
    {NULL}
};

PyDoc_STRVAR(base_doc, "Event base");

PyTypeObject
PyBase_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.Base",         /* tp_name */
    sizeof(PyBaseObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)base_dealloc, /* tp_dealloc */
    0,                    /* tp_print */
    0,                    /* tp_getattr */
    0,                    /* tp_setattr */
    0,                    /* tp_compare */
    0,                    /* tp_repr */
    0,                    /* tp_as_number */
    0,                    /* tp_as_sequence */
    0,                    /* tp_as_mapping */
    0,                    /* tp_hash */
    0,                    /* tp_call */
    0,                    /* tp_str */
    0,                    /* tp_getattro */
    0,                    /* tp_setattro */
    0,                    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,   /* tp_flags */
    base_doc,             /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    base_methods,         /* tp_methods */
    base_members,         /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)base_init,  /* tp_init */
    0,                    /* tp_alloc */
    base_new,             /* tp_new */
    0,                    /* tp_free */
};
