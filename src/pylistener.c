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

#include <event2/listener.h>

#include "pybase.h"
#include "pylistener.h"

typedef struct _PyListenerObject {
    PyObject_HEAD
    PyEventBaseObject *base;
    struct evconnlistener *listener;
    PyObject *callback;
    PyObject *userdata;
    PyObject *weakrefs;
    int fd;
} PyListenerObject;

static void
pylistener_callback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int socklen, void *userdata)
{
    PyListenerObject *self = (PyListenerObject *) userdata;
    START_BLOCK_THREADS
    PyObject *result = PyObject_CallFunction(self->callback, "OiO", self, fd, self->userdata);
    if (result == NULL) {
        pybase_store_error(self->base);
    } else {
        Py_DECREF(result);
    }
    END_BLOCK_THREADS
}

static PyObject *
pylistener_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyListenerObject *s = (PyListenerObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->base = NULL;
        s->listener = NULL;
        s->callback = NULL;
        s->userdata = NULL;
        s->weakrefs = NULL;
    }
    return (PyObject *)s;
}

static int
pylistener_init(PyListenerObject *self, PyObject *args, PyObject *kwds)
{
    PyEventBaseObject *base;
    int flags;
    int backlog;
    int fd;
    PyObject *callback;
    PyObject *userdata = Py_None;
    if (!PyArg_ParseTuple(args, "O!Oiii|O", &PyEventBase_Type, &base, &callback, &flags, &backlog, &fd, &userdata))
        return -1;

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "the callback must be callable");
        return -1;
    }

    self->listener = evconnlistener_new(base->base, pylistener_callback, self, flags, backlog, fd);
    if (self->listener == NULL) {
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
pylistener_traverse(PyListenerObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->callback);
    Py_VISIT(self->userdata);
    Py_VISIT(self->base);
    return 0;
}

static int
pylistener_clear(PyListenerObject *self)
{
    Py_BEGIN_ALLOW_THREADS
    if (self->listener != NULL) {
        evconnlistener_free(self->listener);
        self->listener = NULL;
    }
    Py_END_ALLOW_THREADS
    Py_CLEAR(self->callback);
    Py_CLEAR(self->userdata);
    Py_CLEAR(self->base);
    return 0;
}

static void
pylistener_dealloc(PyListenerObject *self)
{
    if (self->weakrefs != NULL) {
        PyObject_ClearWeakRefs((PyObject *) self);
    }
    pylistener_clear(self);
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pylistener_enable_doc, "Re-enable an evconnlistener that has been disabled.");

static PyObject *
pylistener_enable(PyListenerObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    evconnlistener_enable(self->listener);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pylistener_disable_doc, "Stop listening for connections on an evconnlistener.");

static PyObject *
pylistener_disable(PyListenerObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    evconnlistener_disable(self->listener);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyMethodDef
pylistener_methods[] = {
    {"enable", (PyCFunction)pylistener_enable, METH_NOARGS, pylistener_enable_doc},
    {"disable", (PyCFunction)pylistener_disable, METH_NOARGS, pylistener_disable_doc},
    {NULL, NULL},
};

static PyMemberDef
pylistener_members[] = {
    {"base", T_OBJECT, offsetof(PyListenerObject, base), READONLY, "the base this listener is assigned to"},
    {"callback", T_OBJECT, offsetof(PyListenerObject, callback), READONLY, "the callback to execute"},
    {"userdata", T_OBJECT, offsetof(PyListenerObject, userdata), READONLY, "the userdata to pass to callback"},
    {"fd", T_INT, offsetof(PyListenerObject, fd), READONLY, "the file descriptor this listener is assigned to"},
    {NULL}
};

PyDoc_STRVAR(pylistener_doc, "Listener");

PyTypeObject
PyListener_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.Listener",     /* tp_name */
    sizeof(PyListenerObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pylistener_dealloc, /* tp_dealloc */
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
    pylistener_doc,       /* tp_doc */
    (traverseproc)pylistener_traverse, /* tp_traverse */
    (inquiry)pylistener_clear, /* tp_clear */
    0,                    /* tp_richcompare */
    offsetof(PyListenerObject, weakrefs),  /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pylistener_methods,   /* tp_methods */
    pylistener_members,   /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pylistener_init, /* tp_init */
    0,                    /* tp_alloc */
    pylistener_new,       /* tp_new */
    0,                    /* tp_free */
};
