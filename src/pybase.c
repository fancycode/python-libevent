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
#include <event2/util.h>

#include "pybase.h"

#ifdef WIN32
  #define suseconds_t long
#endif

typedef struct _PyConfigObject {
    PyObject_HEAD
    struct event_config *config;
} PyConfigObject;

void
timeval_init(struct timeval *tv, double time)
{
    tv->tv_sec = (time_t) time;
    tv->tv_usec = (suseconds_t) ((time - tv->tv_sec) * 1000000);
}

void
pybase_store_error(PyEventBaseObject *self)
{
    if (self->error_type == NULL) {
        // Store exception for later reuse and signal loop to stop
        PyErr_Fetch(&self->error_type, &self->error_value, &self->error_traceback);
        Py_BEGIN_ALLOW_THREADS
        event_base_loopbreak(self->base);
        Py_END_ALLOW_THREADS
    }
}

static PyObject *
pybase_evalute_error_response(PyEventBaseObject *self)
{
    if (self->error_type != NULL) {
        // The loop was interrupted due to an error, re-raise
        PyErr_Restore(self->error_type, self->error_value, self->error_traceback);
        self->error_type = NULL;
        self->error_value = NULL;
        self->error_traceback = NULL;
        return NULL;
    }
    
    Py_RETURN_NONE;
}

static PyObject *
pybase_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyEventBaseObject *s;
    s = (PyEventBaseObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->base = NULL;
        s->method = NULL;
        s->features = 0;
        s->error_type = NULL;
        s->error_value = NULL;
        s->error_traceback = NULL;
    }
    return (PyObject *)s;
}

static int
pybase_init(PyEventBaseObject *self, PyObject *args, PyObject *kwds)
{
    PyConfigObject *cfg=NULL;
    if (!PyArg_ParseTuple(args, "|O!", &PyConfig_Type, &cfg))
        return -1;

    if (cfg == NULL) {
        self->base = event_base_new();
    } else {
        self->base = event_base_new_with_config(cfg->config);
    }
    if (self->base == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->method = PyString_FromString(event_base_get_method(self->base));
    self->features = event_base_get_features(self->base);
    return 0;
}

static void
pybase_dealloc(PyEventBaseObject *self)
{
    Py_XDECREF(self->error_type);
    Py_XDECREF(self->error_value);
    Py_XDECREF(self->error_traceback);
    Py_DECREF(self->method);
    if (self->base != NULL) {
        Py_BEGIN_ALLOW_THREADS
        event_base_free(self->base);
        Py_END_ALLOW_THREADS
    }
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pybase_reinit_doc, "Reinitialized the event base after a fork.");

static PyObject *
pybase_reinit(PyEventBaseObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    event_reinit(self->base);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybase_dispatch_doc, "Threadsafe event dispatching loop.");

static PyObject *
pybase_dispatch(PyEventBaseObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    event_base_dispatch(self->base);
    Py_END_ALLOW_THREADS
    return pybase_evalute_error_response(self);
}

PyDoc_STRVAR(pybase_loop_doc, "Handle events (threadsafe version).");

static PyObject *
pybase_loop(PyEventBaseObject *self, PyObject *args)
{
    int flags=0;
    if (!PyArg_ParseTuple(args, "|i", &flags))
        return NULL;
        
    Py_BEGIN_ALLOW_THREADS
    event_base_loop(self->base, flags);
    Py_END_ALLOW_THREADS
    return pybase_evalute_error_response(self);
}

PyDoc_STRVAR(pybase_loopexit_doc, "Exit the event loop after the specified time (threadsafe variant).");

static PyObject *
pybase_loopexit(PyEventBaseObject *self, PyObject *args)
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

PyDoc_STRVAR(pybase_loopbreak_doc, "Abort the active loop() immediately.");

static PyObject *
pybase_loopbreak(PyEventBaseObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    event_base_loopbreak(self->base);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybase_got_exit_doc, "Checks if the event loop was told to exit by loopexit().");

static PyObject *
pybase_got_exit(PyEventBaseObject *self, PyObject *args)
{
    int result;
    Py_BEGIN_ALLOW_THREADS
    result = event_base_got_exit(self->base);
    Py_END_ALLOW_THREADS;
    return PyBool_FromLong(result);
}

PyDoc_STRVAR(pybase_got_break_doc, "Checks if the event loop was told to abort immediately by loopbreak().");

static PyObject *
pybase_got_break(PyEventBaseObject *self, PyObject *args)
{
    int result;
    Py_BEGIN_ALLOW_THREADS
    result = event_base_got_break(self->base);
    Py_END_ALLOW_THREADS;
    return PyBool_FromLong(result);
}

PyDoc_STRVAR(pybase_priority_init_doc, "Set the number of different event priorities (threadsafe variant).");

static PyObject *
pybase_priority_init(PyEventBaseObject *self, PyObject *args)
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
pybase_methods[] = {
    {"reinit", (PyCFunction)pybase_reinit, METH_NOARGS, pybase_reinit_doc},
    {"dispatch", (PyCFunction)pybase_dispatch, METH_NOARGS, pybase_dispatch_doc},
    {"loop", (PyCFunction)pybase_loop, METH_VARARGS, pybase_loop_doc},
    {"loopexit", (PyCFunction)pybase_loopexit, METH_VARARGS, pybase_loopexit_doc},
    {"loopbreak", (PyCFunction)pybase_loopbreak, METH_NOARGS, pybase_loopbreak_doc},
    {"got_exit", (PyCFunction)pybase_got_exit, METH_NOARGS, pybase_got_exit_doc},
    {"got_break", (PyCFunction)pybase_got_break, METH_NOARGS, pybase_got_break_doc},
    {"priority_init", (PyCFunction)pybase_priority_init, METH_VARARGS, pybase_priority_init_doc},
    {NULL, NULL},
};

static PyMemberDef
pybase_members[] = {
    {"method", T_OBJECT, offsetof(PyEventBaseObject, method), READONLY, "kernel event notification mechanism"},
    {"features", T_INT, offsetof(PyEventBaseObject, features), READONLY, "bitmask of the features implemented"},
    {NULL}
};

PyDoc_STRVAR(pybase_doc, "Event base");

PyTypeObject
PyEventBase_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.Base",         /* tp_name */
    sizeof(PyEventBaseObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pybase_dealloc, /* tp_dealloc */
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
    pybase_doc,           /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pybase_methods,       /* tp_methods */
    pybase_members,       /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pybase_init,  /* tp_init */
    0,                    /* tp_alloc */
    pybase_new,           /* tp_new */
    0,                    /* tp_free */
};

static PyObject *
pyconfig_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyConfigObject *s = (PyConfigObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->config = NULL;
    }
    return (PyObject *)s;
}

static int
pyconfig_init(PyConfigObject *self, PyObject *args, PyObject *kwds)
{
    if (!PyArg_ParseTuple(args, "", args))
        return -1;

    self->config = event_config_new();
    if (self->config == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    return 0;
}

static void
pyconfig_dealloc(PyConfigObject *self)
{
    if (self->config != NULL) {
        event_config_free(self->config);
    }
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pyconfig_avoid_method_doc, "Enters an event method that should be avoided into the configuration.");

static PyObject *
pyconfig_avoid_method(PyConfigObject *self, PyObject *args)
{
    char *method;
    
    if (!PyArg_ParseTuple(args, "s", &method))
        return NULL;
        
    event_config_avoid_method(self->config, method);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyconfig_require_features_doc, "Enters a required event method feature that the application demands.");

static PyObject *
pyconfig_require_features(PyConfigObject *self, PyObject *args)
{
    int features;
    
    if (!PyArg_ParseTuple(args, "i", &features))
        return NULL;
        
    event_config_require_features(self->config, features);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyconfig_set_flag_doc, "Sets one or more flags to configure what parts of the eventual event_base will be initialized, and how they'll work.");

static PyObject *
pyconfig_set_flag(PyConfigObject *self, PyObject *args)
{
    int flag;
    
    if (!PyArg_ParseTuple(args, "i", &flag))
        return NULL;
        
    event_config_set_flag(self->config, flag);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyconfig_set_num_cpus_hint_doc, "Records a hint for the number of CPUs in the system.");

static PyObject *
pyconfig_set_num_cpus_hint(PyConfigObject *self, PyObject *args)
{
    int cpus;
    
    if (!PyArg_ParseTuple(args, "i", &cpus))
        return NULL;
        
    event_config_set_num_cpus_hint(self->config, cpus);
    Py_RETURN_NONE;
}

static PyMethodDef
pyconfig_methods[] = {
    {"avoid_method", (PyCFunction)pyconfig_avoid_method, METH_VARARGS, pyconfig_avoid_method_doc},
    {"require_features", (PyCFunction)pyconfig_require_features, METH_VARARGS, pyconfig_require_features_doc},
    {"set_flag", (PyCFunction)pyconfig_set_flag, METH_VARARGS, pyconfig_set_flag_doc},
    {"set_num_cpus_hint", (PyCFunction)pyconfig_set_num_cpus_hint, METH_VARARGS, pyconfig_set_num_cpus_hint_doc},
    {NULL, NULL},
};

PyDoc_STRVAR(pyconfig_doc, "Configuration object");

PyTypeObject
PyConfig_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.Config",         /* tp_name */
    sizeof(PyConfigObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pyconfig_dealloc, /* tp_dealloc */
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
    pyconfig_doc,         /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pyconfig_methods,     /* tp_methods */
    0,                    /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pyconfig_init,  /* tp_init */
    0,                    /* tp_alloc */
    pyconfig_new,         /* tp_new */
    0,                    /* tp_free */
};
