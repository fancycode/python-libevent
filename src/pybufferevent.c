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

#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "pybase.h"
#include "pybuffer.h"
#include "pybufferevent.h"

typedef struct _PyBucketConfigObject {
    PyObject_HEAD
    struct ev_token_bucket_cfg *cfg;
    PyObject *read_rate;
    PyObject *read_burst;
	PyObject *write_rate;
    PyObject *write_burst;
    double tick_len;
} PyBucketConfigObject;

typedef struct _PyBufferEventObject {
    PyObject_HEAD
    struct bufferevent *buffer;
    PyEventBaseObject *base;
    PyBufferObject *input;
    PyBufferObject *output;
    PyBucketConfigObject *bucket;
    PyObject *readcb;
    PyObject *writecb;
    PyObject *eventcb;
    PyObject *cbdata;
    PyObject *weakrefs;
} PyBufferEventObject;

static void
_pybufferevent_readcb(struct bufferevent *bev, void *ctx)
{
    PyBufferEventObject *self = (PyBufferEventObject *) ctx;
    if (self->readcb != NULL) {
        START_BLOCK_THREADS
        PyObject *result = PyObject_CallFunction(self->readcb, "OO", self, self->cbdata);
        if (result == NULL) {
            pybase_store_error(self->base);
        } else {
            Py_DECREF(result);
        }
        END_BLOCK_THREADS
    }
}

static void
_pybufferevent_writecb(struct bufferevent *bev, void *ctx)
{
    PyBufferEventObject *self = (PyBufferEventObject *) ctx;
    if (self->writecb != NULL) {
        START_BLOCK_THREADS
        PyObject *result = PyObject_CallFunction(self->writecb, "OO", self, self->cbdata);
        if (result == NULL) {
            pybase_store_error(self->base);
        } else {
            Py_DECREF(result);
        }
        END_BLOCK_THREADS
    }
}

static void
_pybufferevent_eventcb(struct bufferevent *bev, short what, void *ctx)
{
    PyBufferEventObject *self = (PyBufferEventObject *) ctx;
    if (self->eventcb != NULL) {
        START_BLOCK_THREADS
        PyObject *result = PyObject_CallFunction(self->eventcb, "OiO", self, what, self->cbdata);
        if (result == NULL) {
            pybase_store_error(self->base);
        } else {
            Py_DECREF(result);
        }
        END_BLOCK_THREADS
    }
}

static PyObject *
pybufferevent_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyBufferEventObject *s;
    s = (PyBufferEventObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->buffer = NULL;
        s->base = NULL;
        s->input = NULL;
        s->output = NULL;
        s->bucket = NULL;
        s->readcb = NULL;
        s->writecb = NULL;
        s->eventcb = NULL;
        s->cbdata = NULL;
        s->weakrefs = NULL;
    }
    return (PyObject *)s;
}

static int
pybufferevent_init(PyBufferEventObject *self, PyObject *args, PyObject *kwds)
{
    PyEventBaseObject *base;
    int fd=-1;
    int options=0;
    
    if (!PyArg_ParseTuple(args, "O!|ii", &PyEventBase_Type, &base, &fd, &options))
        return -1;

    self->buffer = bufferevent_socket_new(base->base, fd, options);
    if (self->buffer == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    
    self->base = base;
    Py_INCREF(base);
    self->input = _pybuffer_create(bufferevent_get_input(self->buffer));
    self->output = _pybuffer_create(bufferevent_get_output(self->buffer));
    self->cbdata = Py_None;
    Py_INCREF(Py_None);
    return 0;
}

static int
pybufferevent_traverse(PyBufferEventObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->input);
    Py_VISIT(self->output);
    Py_VISIT(self->bucket);
    Py_VISIT(self->readcb);
    Py_VISIT(self->writecb);
    Py_VISIT(self->eventcb);
    Py_VISIT(self->cbdata);
    Py_VISIT(self->base);
    return 0;
}

static int
pybufferevent_clear(PyBufferEventObject *self)
{
    if (self->input != NULL) {
        self->input->buffer = NULL;
        Py_CLEAR(self->input);
    }
    if (self->output != NULL) {
        self->output->buffer = NULL;
        Py_CLEAR(self->output);
    }
    Py_BEGIN_ALLOW_THREADS
    if (self->buffer != NULL) {
        bufferevent_free(self->buffer);
        self->buffer = NULL;
    }
    Py_END_ALLOW_THREADS
    Py_CLEAR(self->bucket);
    Py_CLEAR(self->readcb);
    Py_CLEAR(self->writecb);
    Py_CLEAR(self->eventcb);
    Py_CLEAR(self->cbdata);
    Py_CLEAR(self->base);
    return 0;
}

static void
pybufferevent_dealloc(PyBufferEventObject *self)
{
    if (self->weakrefs != NULL) {
        PyObject_ClearWeakRefs((PyObject *) self);
    }
    pybufferevent_clear(self);
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pybufferevent_lock_doc, "Acquire the lock on a bufferevent.");

static PyObject *
pybufferevent_lock(PyBufferEventObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    bufferevent_lock(self->buffer);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_unlock_doc, "Release the lock on a bufferevent.");

static PyObject *
pybufferevent_unlock(PyBufferEventObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    bufferevent_unlock(self->buffer);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_setcb_doc, "Change the callbacks for a bufferevent.");

static PyObject *
pybufferevent_setcb(PyBufferEventObject *self, PyObject *args)
{
    PyObject *readcb;
    PyObject *writecb;
    PyObject *eventcb;
    PyObject *cbdata=Py_None;
    
    if (!PyArg_ParseTuple(args, "OOO|O", &readcb, &writecb, &eventcb, &cbdata))
        return NULL;
    
    // make changes atomic
    Py_BEGIN_ALLOW_THREADS
    bufferevent_lock(self->buffer);
    Py_END_ALLOW_THREADS
    
    if (self->readcb != readcb) {
        PyObject *old = self->readcb;
        self->readcb = (readcb == Py_None ? NULL : readcb);
        Py_XINCREF(self->readcb);
        Py_XDECREF(old);
    }
    if (self->writecb != writecb) {
        PyObject *old = self->writecb;
        self->writecb = (writecb == Py_None ? NULL : writecb);
        Py_XINCREF(self->writecb);
        Py_XDECREF(old);
    }
    if (self->eventcb != eventcb) {
        PyObject *old = self->eventcb;
        self->eventcb = (eventcb == Py_None ? NULL : eventcb);
        Py_XINCREF(self->eventcb);
        Py_XDECREF(old);
    }
    if (self->cbdata != cbdata) {
        PyObject *old = self->cbdata;
        self->cbdata = cbdata;
        Py_INCREF(cbdata);
        Py_XDECREF(old);
    }
    
    Py_BEGIN_ALLOW_THREADS
    bufferevent_setcb(self->buffer,
        readcb == Py_None ? NULL : _pybufferevent_readcb,
        writecb == Py_None ? NULL : _pybufferevent_writecb,
        eventcb == Py_None ? NULL : _pybufferevent_eventcb,
        self);
    bufferevent_unlock(self->buffer);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_write_doc, "Write data to a bufferevent buffer.");

static void
_pybufferevent_decref(const void *data, size_t datalen, void *extra)
{
    PyObject *obj = (PyObject *) extra;
    START_BLOCK_THREADS
    Py_DECREF(obj);
    END_BLOCK_THREADS
}

static PyObject *
pybufferevent_write(PyBufferEventObject *self, PyObject *args)
{
    PyObject *pydata;
    char *data;
    Py_ssize_t length;
    int result;
    
    if (!PyArg_ParseTuple(args, "O", &pydata))
        return NULL;

    if (PyEventBuffer_Check(pydata)) {
        Py_BEGIN_ALLOW_THREADS
        result = bufferevent_write_buffer(self->buffer, ((PyBufferObject *) pydata)->buffer);
        Py_END_ALLOW_THREADS
    } else {
        if (PyObject_AsReadBuffer(pydata, (const void **) &data, &length) != 0) {
            return NULL;
        }
        
        Py_BEGIN_ALLOW_THREADS
        evbuffer_lock(self->output->buffer);
        result = evbuffer_add_reference(self->output->buffer, data, length, _pybufferevent_decref, pydata);
        Py_END_ALLOW_THREADS
        if (result >= 0) {
            Py_INCREF(pydata);
        }
        Py_BEGIN_ALLOW_THREADS
        evbuffer_unlock(self->output->buffer);
        Py_END_ALLOW_THREADS
    }
    if (result < 0) {
        PyErr_SetString(PyExc_TypeError, "could not write data to buffer");
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_read_doc, "Read data from a bufferevent buffer.");

static PyObject *
pybufferevent_read(PyBufferEventObject *self, PyObject *args)
{
    PyObject *pydest=Py_None;
    PyObject *pysize;
    char *data;
    Py_ssize_t size;
    Py_ssize_t length;
    PyObject *result;
    int unlock=0;
    
    if (!PyArg_ParseTuple(args, "|O", &pydest))
        return NULL;
    
    if (PyEventBuffer_Check(pydest)) {
        Py_BEGIN_ALLOW_THREADS
        size = bufferevent_read_buffer(self->buffer, ((PyBufferObject *) pydest)->buffer);
        Py_END_ALLOW_THREADS
        if (size != 0) {
            PyErr_SetString(PyExc_TypeError, "could not read data from buffer");
            return NULL;
        }
        Py_INCREF(Py_None);
        return Py_None;
    } else if (pydest == Py_None) {
        // read complete buffer
        Py_BEGIN_ALLOW_THREADS
        bufferevent_lock(self->buffer);
        length = evbuffer_get_length(bufferevent_get_input(self->buffer));
        if (length == 0) {
            bufferevent_unlock(self->buffer);
        }
        unlock = 1;
        Py_END_ALLOW_THREADS
        if (length == 0) {
            return PyString_FromString("");
        }
    } else {
        // read given amount
        pysize = PyNumber_Int(pydest);
        if (pysize == NULL) {
            PyErr_Format(PyExc_TypeError, "expected a Buffer or a number, not '%s'", pydest->ob_type->tp_name);
            return NULL;
        }
        
        length = PyInt_AsLong(pysize);
        Py_DECREF(pysize);
    }
    result = PyString_FromStringAndSize(NULL, length);
    if (result == NULL) {
        if (unlock) {
            Py_BEGIN_ALLOW_THREADS
            bufferevent_unlock(self->buffer);
            Py_END_ALLOW_THREADS
        }
        return PyErr_NoMemory();
    }
    
    data = PyString_AS_STRING(result);
    Py_BEGIN_ALLOW_THREADS
    size = bufferevent_read(self->buffer, data, length);
    if (unlock) {
        bufferevent_unlock(self->buffer);
    }
    Py_END_ALLOW_THREADS
    if (size < 0) {
        Py_DECREF(result);
        PyErr_SetString(PyExc_TypeError, "could not read data from buffer");
        result = NULL;
    } else if (size == 0) {
        Py_DECREF(result);
        result = PyString_FromString("");
    } else if (size != length) {
        _PyString_Resize(&result, size);
    }
    return result;
}

PyDoc_STRVAR(pybufferevent_enable_doc, "Enable a bufferevent.");

static PyObject *
pybufferevent_enable(PyBufferEventObject *self, PyObject *args)
{
    int what;
    
    if (!PyArg_ParseTuple(args, "i", &what))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    bufferevent_enable(self->buffer, what);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_disable_doc, "Disable a bufferevent.");

static PyObject *
pybufferevent_disable(PyBufferEventObject *self, PyObject *args)
{
    int what;
    
    if (!PyArg_ParseTuple(args, "i", &what))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    bufferevent_disable(self->buffer, what);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_set_timeouts_doc, "Set the read and write timeout for a buffered event.");

static PyObject *
pybufferevent_set_timeouts(PyBufferEventObject *self, PyObject *args)
{
    double read;
    double write;
    struct timeval read_tv;
    struct timeval write_tv;
    
    if (!PyArg_ParseTuple(args, "dd", &read, &write))
        return NULL;

    timeval_init(&read_tv, read);
    timeval_init(&write_tv, write);

    Py_BEGIN_ALLOW_THREADS
    bufferevent_set_timeouts(self->buffer,
        read <= 0 ? NULL : &read_tv,
        write <= 0 ? NULL : &write_tv);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_set_watermark_doc, "Sets the watermarks for read and write events.");

static PyObject *
pybufferevent_set_watermark(PyBufferEventObject *self, PyObject *args)
{
    int what;
    Py_ssize_t lowmark;
    Py_ssize_t highmark;
    
    if (!PyArg_ParseTuple(args, "inn", &what, &lowmark, &highmark))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    bufferevent_setwatermark(self->buffer, what, lowmark, highmark);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pybufferevent_set_ratelimit_doc, "Set the rate-limit of the bufferevent.");

static PyObject *
pybufferevent_set_ratelimit(PyBufferEventObject *self, PyObject *args)
{
    PyObject *limit;
    
    if (!PyArg_ParseTuple(args, "O", &limit))
        return NULL;
    
    if (limit != Py_None && !PyBucketConfig_Check(limit)) {
        PyErr_Format(PyExc_TypeError, "expected a BucketConfig object or None, but got %s", limit->ob_type->tp_name);
        return NULL;
    }
    
    if (limit == (PyObject *) self->bucket || (limit == Py_None && self->bucket == NULL)) {
        // No change
        Py_RETURN_NONE;
    }
    
    Py_BEGIN_ALLOW_THREADS
    if (limit == Py_None) {
        bufferevent_set_rate_limit(self->buffer, NULL);
    } else {
        bufferevent_set_rate_limit(self->buffer, ((PyBucketConfigObject *) limit)->cfg);
    }
    Py_END_ALLOW_THREADS
    Py_XDECREF(self->bucket);
    if (limit == Py_None) {
        self->bucket = NULL;
    } else {
        self->bucket = (PyBucketConfigObject *) limit;
        Py_INCREF(limit);
    }
    Py_RETURN_NONE;
}

static PyMethodDef
pybufferevent_methods[] = {
    {"lock", (PyCFunction)pybufferevent_lock, METH_NOARGS, pybufferevent_lock_doc},
    {"unlock", (PyCFunction)pybufferevent_unlock, METH_NOARGS, pybufferevent_unlock_doc},
    {"__enter__", (PyCFunction)pybufferevent_lock, METH_VARARGS, pybufferevent_lock_doc},
    {"__exit__", (PyCFunction)pybufferevent_unlock, METH_VARARGS, pybufferevent_unlock_doc},
    {"set_callbacks", (PyCFunction)pybufferevent_setcb, METH_VARARGS, pybufferevent_setcb_doc},
    {"write", (PyCFunction)pybufferevent_write, METH_VARARGS, pybufferevent_write_doc},
    {"read", (PyCFunction)pybufferevent_read, METH_VARARGS, pybufferevent_read_doc},
    {"enable", (PyCFunction)pybufferevent_enable, METH_VARARGS, pybufferevent_enable_doc},
    {"disable", (PyCFunction)pybufferevent_disable, METH_VARARGS, pybufferevent_disable_doc},
    {"set_timeouts", (PyCFunction)pybufferevent_set_timeouts, METH_VARARGS, pybufferevent_set_timeouts_doc},
    {"set_watermark", (PyCFunction)pybufferevent_set_watermark, METH_VARARGS, pybufferevent_set_watermark_doc},
    {"set_ratelimit", (PyCFunction)pybufferevent_set_ratelimit, METH_VARARGS, pybufferevent_set_ratelimit_doc},
    {NULL, NULL},
};

static PyMemberDef
pybufferevent_members[] = {
    {"base", T_OBJECT, offsetof(PyBufferEventObject, base), READONLY, "the base this event is assigned to"},
    {"input", T_OBJECT, offsetof(PyBufferEventObject, input), READONLY, "the input buffer"},
    {"output", T_OBJECT, offsetof(PyBufferEventObject, output), READONLY, "the output buffer"},
    {"bucket", T_OBJECT, offsetof(PyBufferEventObject, bucket), READONLY, "the rate limit"},
    {NULL}
};

PyDoc_STRVAR(pybufferevent_doc, "Bufferevent");

PyTypeObject
PyBufferEvent_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.BufferEvent",       /* tp_name */
    sizeof(PyBufferEventObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pybufferevent_dealloc, /* tp_dealloc */
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
    pybufferevent_doc,    /* tp_doc */
    (traverseproc)pybufferevent_traverse, /* tp_traverse */
    (inquiry)pybufferevent_clear, /* tp_clear */
    0,                    /* tp_richcompare */
    offsetof(PyBufferEventObject, weakrefs),  /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pybufferevent_methods, /* tp_methods */
    pybufferevent_members, /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pybufferevent_init, /* tp_init */
    0,                    /* tp_alloc */
    pybufferevent_new,    /* tp_new */
    0,                    /* tp_free */
};

static PyObject *
pybucketconfig_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyBucketConfigObject *s = (PyBucketConfigObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->cfg = NULL;
        s->read_rate = NULL;
        s->read_burst = NULL;
        s->write_rate = NULL;
        s->write_burst = NULL;
    }
    return (PyObject *)s;
}

static int
pybucketconfig_init(PyBucketConfigObject *self, PyObject *args, PyObject *kwds)
{
    Py_ssize_t read_rate;
    Py_ssize_t read_burst;
    Py_ssize_t write_rate;
    Py_ssize_t write_burst;
    double tick_len=1;
    struct timeval tv;
    
    if (!PyArg_ParseTuple(args, "nnnn|d", &read_rate, &read_burst, &write_rate, &write_burst, &tick_len))
        return -1;

    timeval_init(&tv, tick_len);
    self->cfg = ev_token_bucket_cfg_new(read_rate, read_burst, write_rate, write_burst, &tv);
    if (self->cfg == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    
    self->read_rate = PyLong_FromSsize_t(read_rate);
    self->read_burst = PyLong_FromSsize_t(read_burst);
    self->write_rate = PyLong_FromSsize_t(write_rate);
    self->write_burst = PyLong_FromSsize_t(write_burst);
    self->tick_len = tick_len;
    return 0;
}

static void
pybucketconfig_dealloc(PyBucketConfigObject *self)
{
    Py_BEGIN_ALLOW_THREADS
    if (self->cfg != NULL) {
        ev_token_bucket_cfg_free(self->cfg);
    }
    Py_END_ALLOW_THREADS
    Py_XDECREF(self->read_rate);
    Py_XDECREF(self->read_burst);
    Py_XDECREF(self->write_rate);
    Py_XDECREF(self->write_burst);
    Py_TYPE(self)->tp_free(self);
}

static PyMemberDef
pybucketconfig_members[] = {
    {"read_rate", T_OBJECT, offsetof(PyBucketConfigObject, read_rate), READONLY, "The maximum number of bytes to read per tick on average."},
    {"read_burst", T_OBJECT, offsetof(PyBucketConfigObject, read_burst), READONLY, "The maximum number of bytes to read in any single tick."},
    {"write_rate", T_OBJECT, offsetof(PyBucketConfigObject, write_rate), READONLY, "The maximum number of bytes to write per tick on average."},
    {"write_burst", T_OBJECT, offsetof(PyBucketConfigObject, write_burst), READONLY, "The maximum number of bytes to write in any single tick."},
    {"tick_len", T_DOUBLE, offsetof(PyBucketConfigObject, tick_len), READONLY, "The length of a single tick."},
    {NULL}
};

PyDoc_STRVAR(pybucketconfig_doc, "BucketConfig");

PyTypeObject
PyBucketConfig_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.BucketConfig",       /* tp_name */
    sizeof(PyBucketConfigObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pybucketconfig_dealloc, /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,   /* tp_flags */
    pybucketconfig_doc,    /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    0,                    /* tp_methods */
    pybucketconfig_members, /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pybucketconfig_init, /* tp_init */
    0,                    /* tp_alloc */
    pybucketconfig_new,    /* tp_new */
    0,                    /* tp_free */
};
