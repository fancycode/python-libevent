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

#include <Python.h>
#include <structmember.h>

#include <event2/event.h>

#include "pybase.h"
#include "pyevent.h"

typedef struct _PyEventObject {
    PyObject_HEAD
    PyEventBaseObject *base;
    struct event *event;
    PyObject *callback;
    PyObject *userdata;
    PyObject *weakrefs;
    int fd;
} PyEventObject;

static void
pyevent_callback(evutil_socket_t fd, short what, void *userdata)
{
    PyEventObject *self = (PyEventObject *) userdata;
    START_BLOCK_THREADS
    PyObject *result = PyObject_CallFunction(self->callback, "OiiO", self, fd, what, self->userdata);
    if (result == NULL) {
        pybase_store_error(self->base);
    } else {
        Py_DECREF(result);
    }
    END_BLOCK_THREADS
}

static PyObject *
pyevent_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyEventObject *s = (PyEventObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->base = NULL;
        s->event = NULL;
        s->callback = NULL;
        s->userdata = NULL;
        s->weakrefs = NULL;
    }
    return (PyObject *)s;
}

static int
pyevent_init(PyEventObject *self, PyObject *args, PyObject *kwds)
{
    PyEventBaseObject *base;
    int fd;
    int event;
    PyObject *callback;
    PyObject *userdata = Py_None;
    if (!PyArg_ParseTuple(args, "O!iiO|O", &PyEventBase_Type, &base, &fd, &event, &callback, &userdata))
        return -1;

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "the callback must be callable");
        return -1;
    }

    self->event = event_new(base->base, fd, event, pyevent_callback, self);
    if (self->event == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    
    self->base = base;
    Py_INCREF(base);
    self->callback = callback;
    Py_INCREF(callback);
    self->userdata = userdata;
    Py_INCREF(userdata);
    self->fd = fd;
    return 0;
}

static int
pyevent_traverse(PyEventObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->callback);
    Py_VISIT(self->userdata);
    Py_VISIT(self->base);
    return 0;
}

static int
pyevent_clear(PyEventObject *self)
{
    Py_BEGIN_ALLOW_THREADS
    if (self->event != NULL) {
        event_free(self->event);
        self->event = NULL;
    }
    Py_END_ALLOW_THREADS
    Py_CLEAR(self->callback);
    Py_CLEAR(self->userdata);
    Py_CLEAR(self->base);
    return 0;
}

static void
pyevent_dealloc(PyEventObject *self)
{
    if (self->weakrefs != NULL) {
        PyObject_ClearWeakRefs((PyObject *) self);
    }
    pyevent_clear(self);
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(event_add_doc, "Add event.");

static PyObject *
pyevent_add(PyEventObject *self, PyObject *args)
{
    double time=-1;
    struct timeval tv;
    if (!PyArg_ParseTuple(args, "|d", &time))
        return NULL;
    
    timeval_init(&tv, time);
    Py_BEGIN_ALLOW_THREADS
    if (time <= 0) {
        event_add(self->event, NULL);
    } else {
        event_add(self->event, &tv);
    }
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(event_delete_doc, "Remove timer event.");

static PyObject *
pyevent_delete(PyEventObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    event_del(self->event);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(event_set_priority_doc, "Assign a priority to an event.");

static PyObject *
pyevent_set_priority(PyEventObject *self, PyObject *args)
{
    int priority;
    if (!PyArg_ParseTuple(args, "i", &priority))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    event_priority_set(self->event, priority);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyMethodDef
pyevent_methods[] = {
    {"add", (PyCFunction)pyevent_add, METH_VARARGS, event_add_doc},
    {"delete", (PyCFunction)pyevent_delete, METH_NOARGS, event_delete_doc},
    {"set_priority", (PyCFunction)pyevent_set_priority, METH_VARARGS, event_set_priority_doc},
    {NULL, NULL},
};

static PyMemberDef
pyevent_members[] = {
    {"base", T_OBJECT, offsetof(PyEventObject, base), READONLY, "the base this event is assigned to"},
    {"callback", T_OBJECT, offsetof(PyEventObject, callback), READONLY, "the callback to execute"},
    {"userdata", T_OBJECT, offsetof(PyEventObject, userdata), READONLY, "the userdata to pass to callback"},
    {"fd", T_INT, offsetof(PyEventObject, fd), READONLY, "the file descriptor this event is assigned to"},
    {NULL}
};

PyDoc_STRVAR(event_doc, "Event");

PyTypeObject
PyEvent_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.Event",         /* tp_name */
    sizeof(PyEventObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pyevent_dealloc, /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_HAVE_GC|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_WEAKREFS,   /* tp_flags */
    event_doc,            /* tp_doc */
    (traverseproc)pyevent_traverse, /* tp_traverse */
    (inquiry)pyevent_clear, /* tp_clear */
    0,                    /* tp_richcompare */
    offsetof(PyEventObject, weakrefs),  /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pyevent_methods,      /* tp_methods */
    pyevent_members,      /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pyevent_init, /* tp_init */
    0,                    /* tp_alloc */
    pyevent_new,            /* tp_new */
    0,                    /* tp_free */
};
