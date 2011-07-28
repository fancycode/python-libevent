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

#include "pybase.h"
#include "pybuffer.h"

static PyObject *
pybuffer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyBufferObject *s;
    s = (PyBufferObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->buffer = NULL;
        s->base = NULL;
        s->owned = 0;
    }
    return (PyObject *)s;
}

PyBufferObject *
_pybuffer_create(struct evbuffer *buffer)
{
    PyBufferObject *result = (PyBufferObject *)PyEventBuffer_Type.tp_alloc(&PyEventBuffer_Type, 0);
    if (result == NULL) {
        return NULL;
    }
    
    result->buffer = buffer;
    result->base = NULL;
    result->owned = 0;
    return result;
}

static int
pybuffer_init(PyBufferObject *self, PyObject *args, PyObject *kwds)
{
    self->buffer = evbuffer_new();
    if (self->buffer == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    
    self->owned = 1;
    return 0;
}

static void
pybuffer_dealloc(PyBufferObject *self)
{
    Py_BEGIN_ALLOW_THREADS
    if (self->owned && self->buffer != NULL) {
        evbuffer_free(self->buffer);
    }
    Py_END_ALLOW_THREADS
    Py_XDECREF(self->base);
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(buffer_enable_locking_doc, "Enable locking on an evbuffer.");

static PyObject *
pybuffer_enable_locking(PyBufferObject *self, PyObject *args)
{
#if defined(WITH_THREAD)
    Py_BEGIN_ALLOW_THREADS
    evbuffer_enable_locking(self->buffer, NULL);
    Py_END_ALLOW_THREADS
#endif
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_lock_doc, "Acquire the lock on an evbuffer.");

static PyObject *
pybuffer_lock(PyBufferObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    evbuffer_lock(self->buffer);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_unlock_doc, "Release the lock on an evbuffer.");

static PyObject *
pybuffer_unlock(PyBufferObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS
    evbuffer_unlock(self->buffer);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_get_contiguous_space_doc, "Returns the number of contiguous available bytes in the first buffer chain.");

static PyObject *
pybuffer_get_contiguous_space(PyBufferObject *self, PyObject *args)
{
    size_t result;
    Py_BEGIN_ALLOW_THREADS
    result = evbuffer_get_contiguous_space(self->buffer);
    Py_END_ALLOW_THREADS
    return PyLong_FromUnsignedLong(result);
}

PyDoc_STRVAR(buffer_expand_doc, "Expands the available space in an event buffer.");

static PyObject *
pybuffer_expand(PyBufferObject *self, PyObject *args)
{
    Py_ssize_t size;
    
    if (!PyArg_ParseTuple(args, "n", &size))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    evbuffer_expand(self->buffer, size);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_add_doc, "Append data to the end of an evbuffer.");

static void
_pybuffer_decref(const void *data, size_t datalen, void *extra)
{
    PyObject *obj = (PyObject *) extra;
    START_BLOCK_THREADS
    Py_DECREF(obj);
    END_BLOCK_THREADS
}

static PyObject *
pybuffer_add(PyBufferObject *self, PyObject *args)
{
    PyObject *pydata;
    char *data;
    Py_ssize_t length;
    int result;
    
    if (!PyArg_ParseTuple(args, "O", &pydata))
        return NULL;

    if (PyEventBuffer_Check(pydata)) {
        Py_BEGIN_ALLOW_THREADS
        result = evbuffer_add_buffer(self->buffer, ((PyBufferObject *) pydata)->buffer);
        Py_END_ALLOW_THREADS
    } else {
        if (PyObject_AsReadBuffer(pydata, (const void **) &data, &length) != 0) {
            return NULL;
        }
        
        Py_BEGIN_ALLOW_THREADS
        evbuffer_lock(self->buffer);
        result = evbuffer_add_reference(self->buffer, data, length, _pybuffer_decref, pydata);
        Py_END_ALLOW_THREADS
        if (result >= 0) {
            Py_INCREF(pydata);
        }
        Py_BEGIN_ALLOW_THREADS
        evbuffer_unlock(self->buffer);
        Py_END_ALLOW_THREADS
    }
    if (result < 0) {
        PyErr_SetString(PyExc_TypeError, "could not add data to buffer");
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_remove_doc, "Read data from an event buffer and drain the bytes read.");

static PyObject *
pybuffer_remove(PyBufferObject *self, PyObject *args)
{
    char *data;
    Py_ssize_t size;
    Py_ssize_t length=-1;
    PyObject *result;
    int unlock=0;
    
    if (!PyArg_ParseTuple(args, "|n", &length))
        return NULL;
    
    if (length == -1) {
        Py_BEGIN_ALLOW_THREADS
        evbuffer_lock(self->buffer);
        length = evbuffer_get_length(self->buffer);
        if (length == 0) {
            evbuffer_unlock(self->buffer);
        } else {
            unlock = 1;
        }
        Py_END_ALLOW_THREADS
    } else if (length < 0) {
        PyErr_Format(PyExc_TypeError, "can't read %d bytes from the buffer", (int) length);
        return NULL;
    }
    
    if (length == 0) {
        return PyString_FromString("");
    }
    
    result = PyString_FromStringAndSize(NULL, length);
    if (result == NULL) {
        if (unlock) {
            Py_BEGIN_ALLOW_THREADS
            evbuffer_unlock(self->buffer);
            Py_END_ALLOW_THREADS
        }
        return PyErr_NoMemory();
    }
    
    data = PyString_AS_STRING(result);
    Py_BEGIN_ALLOW_THREADS
    size = evbuffer_remove(self->buffer, data, length);
    if (unlock) {
        evbuffer_unlock(self->buffer);
    }
    Py_END_ALLOW_THREADS
    if (size < 0) {
        Py_DECREF(result);
        PyErr_SetString(PyExc_TypeError, "could not remove data from buffer");
        result = NULL;
    } else if (size == 0) {
        Py_DECREF(result);
        result = PyString_FromString("");
    } else if (size != length) {
        _PyString_Resize(&result, size);
    }
    return result;
}

PyDoc_STRVAR(buffer_copyout_doc, "Read data from an event buffer, and leave the buffer unchanged.");

static PyObject *
pybuffer_copyout(PyBufferObject *self, PyObject *args)
{
    char *data;
    ev_ssize_t size;
    Py_ssize_t length=-1;
    PyObject *result;
    int unlock=0;
    
    if (!PyArg_ParseTuple(args, "|n", &length))
        return NULL;
    
    if (length == -1) {
        Py_BEGIN_ALLOW_THREADS
        evbuffer_lock(self->buffer);
        length = evbuffer_get_length(self->buffer);
        if (length == 0) {
            evbuffer_unlock(self->buffer);
        } else {
            unlock = 1;
        }
        Py_END_ALLOW_THREADS
    } else if (length < 0) {
        PyErr_Format(PyExc_TypeError, "can't read %d bytes from the buffer", (int) length);
        return NULL;
    }
    
    if (length == 0) {
        return PyString_FromString("");
    }
    
    result = PyString_FromStringAndSize(NULL, length);
    if (result == NULL) {
        if (unlock) {
            Py_BEGIN_ALLOW_THREADS
            evbuffer_unlock(self->buffer);
            Py_END_ALLOW_THREADS
        }
        return PyErr_NoMemory();
    }
    
    data = PyString_AS_STRING(result);
    Py_BEGIN_ALLOW_THREADS
    size = evbuffer_copyout(self->buffer, data, length);
    if (unlock) {
        evbuffer_unlock(self->buffer);
    }
    Py_END_ALLOW_THREADS
    if (size < 0) {
        Py_DECREF(result);
        PyErr_SetString(PyExc_TypeError, "could not copy data from buffer");
        result = NULL;
    } else if (size == 0) {
        Py_DECREF(result);
        result = PyString_FromString("");
    } else if (size != length) {
        _PyString_Resize(&result, size);
    }
    return result;
}

PyDoc_STRVAR(buffer_remove_buffer_doc, "Read data from an event buffer into another event buffer draining the bytes from the src buffer read.");

static PyObject *
pybuffer_remove_buffer(PyBufferObject *self, PyObject *args)
{
    PyBufferObject *dst;
    ev_ssize_t size;
    Py_ssize_t length;
    
    if (!PyArg_ParseTuple(args, "O!n", &PyBuffer_Type, &dst, &length))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    size = evbuffer_remove_buffer(self->buffer, dst->buffer, length);
    Py_END_ALLOW_THREADS
    if (size < 0) {
        PyErr_SetString(PyExc_TypeError, "could not remove data from buffer");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_readln_doc, "Read a single line from an event buffer.");

static PyObject *
pybuffer_readln(PyBufferObject *self, PyObject *args)
{
    int flags=EVBUFFER_EOL_ANY;
    char *data;
    size_t size;
    PyObject *result;
    
    if (!PyArg_ParseTuple(args, "|i", &flags))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    data = evbuffer_readln(self->buffer, &size, flags);
    Py_END_ALLOW_THREADS
    if (data == NULL) {
        result = PyString_FromString("");
    } else {
        result = PyString_FromStringAndSize(data, size);
        free(data);
    }
    return result;
}

PyDoc_STRVAR(buffer_add_file_doc, "Move data from a file into the evbuffer for writing to a socket.");

static PyObject *
pybuffer_add_file(PyBufferObject *self, PyObject *args)
{
    int fd;
    Py_ssize_t offset;
    Py_ssize_t length;
    int result;
    
    if (!PyArg_ParseTuple(args, "inn", &fd, &offset, &length))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    result = evbuffer_add_file(self->buffer, fd, offset, length);
    Py_END_ALLOW_THREADS
    if (result < 0) {
        PyErr_SetString(PyExc_TypeError, "could not add data from file to the buffer");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_drain_doc, "Remove a specified number of bytes data from the beginning of a buffer.");

static PyObject *
pybuffer_drain(PyBufferObject *self, PyObject *args)
{
    Py_ssize_t length;
    int result;
    
    if (!PyArg_ParseTuple(args, "n", &length))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    result = evbuffer_drain(self->buffer, length);
    Py_END_ALLOW_THREADS
    if (result < 0) {
        PyErr_SetString(PyExc_TypeError, "could not drain data from the buffer");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_write_doc, "Write the contents of an evbuffer to a file descriptor.");

static PyObject *
pybuffer_write(PyBufferObject *self, PyObject *args)
{
    int fd;
    int result;
    int length=-1;
    
    if (!PyArg_ParseTuple(args, "i|i", &fd, &length))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    if (length < 0) {
        result = evbuffer_write(self->buffer, fd);
    } else {
        result = evbuffer_write_atmost(self->buffer, fd, length);
    }
    Py_END_ALLOW_THREADS
    if (result < 0) {
        PyErr_SetString(PyExc_TypeError, "could not write buffer to file descriptor");
        return NULL;
    }
    return PyLong_FromLong(result);
}

PyDoc_STRVAR(buffer_read_doc, "Read from a file descriptor and store the result in an evbuffer.");

static PyObject *
pybuffer_read(PyBufferObject *self, PyObject *args)
{
    int fd;
    int result;
    int length;
    
    if (!PyArg_ParseTuple(args, "ii", &fd, &length))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    result = evbuffer_read(self->buffer, fd, length);
    Py_END_ALLOW_THREADS
    if (result < 0) {
        PyErr_SetString(PyExc_TypeError, "could not read buffer from file descriptor");
        return NULL;
    }
    return PyLong_FromLong(result);
}

PyDoc_STRVAR(buffer_pullup_doc, "Makes the data at the begining of an evbuffer contiguous.");

static PyObject *
pybuffer_pullup(PyBufferObject *self, PyObject *args)
{
    Py_ssize_t length=-1;
    
    if (!PyArg_ParseTuple(args, "|n", &length))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    evbuffer_pullup(self->buffer, length);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_prepend_doc, "Prepends data to the beginning of the evbuffer.");

static PyObject *
pybuffer_prepend(PyBufferObject *self, PyObject *args)
{
    PyObject *pydata;
    char *data;
    Py_ssize_t length;
    int result;
    
    if (!PyArg_ParseTuple(args, "O", &pydata))
        return NULL;
    
    if (PyEventBuffer_Check(pydata)) {
        Py_BEGIN_ALLOW_THREADS
        result = evbuffer_prepend_buffer(self->buffer, ((PyBufferObject *) pydata)->buffer);
        Py_END_ALLOW_THREADS
    } else {
        if (PyObject_AsReadBuffer(pydata, (const void **) &data, &length) != 0) {
            return NULL;
        }
        
        Py_BEGIN_ALLOW_THREADS
        result = evbuffer_prepend(self->buffer, data, length);
        Py_END_ALLOW_THREADS
    }
    if (result < 0) {
        PyErr_SetString(PyExc_TypeError, "could not prepend data to buffer");
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_freeze_doc, "Prevent calls that modify an evbuffer from succeeding.");

static PyObject *
pybuffer_freeze(PyBufferObject *self, PyObject *args)
{
    int at_front;
    
    if (!PyArg_ParseTuple(args, "i", &at_front))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    evbuffer_freeze(self->buffer, at_front);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_unfreeze_doc, "Re-enable calls that modify an evbuffer.");

static PyObject *
pybuffer_unfreeze(PyBufferObject *self, PyObject *args)
{
    int at_front;
    
    if (!PyArg_ParseTuple(args, "i", &at_front))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    evbuffer_unfreeze(self->buffer, at_front);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_defer_callbacks_doc, "Serialize callbacks to given base.");

static PyObject *
pybuffer_defer_callbacks(PyBufferObject *self, PyObject *args)
{
    PyObject *base;
    
    if (!PyArg_ParseTuple(args, "O", &base))
        return NULL;
    
    if (base != Py_None && !PyEventBase_Check(base)) {
        PyErr_Format(PyExc_TypeError, "expected None or a base but got a %s", base->ob_type->tp_name);
        return NULL;
    }
    
    Py_BEGIN_ALLOW_THREADS
    if (base == Py_None) {
        evbuffer_defer_callbacks(self->buffer, NULL);
    } else {
        evbuffer_defer_callbacks(self->buffer, ((PyEventBaseObject *) base)->base);
    }
    Py_END_ALLOW_THREADS
    Py_XDECREF(self->base);
    if (base == Py_None) {
        self->base = NULL;
    } else {
        self->base = (PyEventBaseObject *) base;
        Py_INCREF(self->base);
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(buffer_search_doc, "Search for a string within an evbuffer.");

static PyObject *
pybuffer_search(PyBufferObject *self, PyObject *args)
{
    char *data;
    int length;
    Py_ssize_t start=0;
    struct evbuffer_ptr start_pos;
    struct evbuffer_ptr pos;
    
    if (!PyArg_ParseTuple(args, "s#|n", &data, &length, &start))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    if (start > 0) {
        evbuffer_ptr_set(self->buffer, &start_pos, start, EVBUFFER_PTR_SET);
        pos = evbuffer_search(self->buffer, data, length, &start_pos);
    } else {
        pos = evbuffer_search(self->buffer, data, length, NULL);
    }
    Py_END_ALLOW_THREADS
    
    return PyLong_FromSsize_t(pos.pos);
}

static PyMethodDef
pybuffer_methods[] = {
    {"enable_locking", (PyCFunction)pybuffer_enable_locking, METH_NOARGS, buffer_enable_locking_doc},
    {"lock", (PyCFunction)pybuffer_lock, METH_NOARGS, buffer_lock_doc},
    {"unlock", (PyCFunction)pybuffer_unlock, METH_NOARGS, buffer_unlock_doc},
    {"__enter__", (PyCFunction)pybuffer_lock, METH_VARARGS, buffer_lock_doc},
    {"__exit__", (PyCFunction)pybuffer_unlock, METH_VARARGS, buffer_unlock_doc},
    {"get_contiguous_space", (PyCFunction)pybuffer_get_contiguous_space, METH_NOARGS, buffer_get_contiguous_space_doc},
    {"expand", (PyCFunction)pybuffer_expand, METH_VARARGS, buffer_expand_doc},
    {"add", (PyCFunction)pybuffer_add, METH_VARARGS, buffer_add_doc},
    {"remove", (PyCFunction)pybuffer_remove, METH_VARARGS, buffer_remove_doc},
    {"copyout", (PyCFunction)pybuffer_copyout, METH_VARARGS, buffer_copyout_doc},
    {"remove_buffer", (PyCFunction)pybuffer_remove_buffer, METH_VARARGS, buffer_remove_buffer_doc},
    {"readln", (PyCFunction)pybuffer_readln, METH_VARARGS, buffer_readln_doc},
    {"add_file", (PyCFunction)pybuffer_add_file, METH_VARARGS, buffer_add_file_doc},
    {"drain", (PyCFunction)pybuffer_drain, METH_VARARGS, buffer_drain_doc},
    {"write", (PyCFunction)pybuffer_write, METH_VARARGS, buffer_write_doc},
    {"read", (PyCFunction)pybuffer_read, METH_VARARGS, buffer_read_doc},
    {"pullup", (PyCFunction)pybuffer_pullup, METH_VARARGS, buffer_pullup_doc},
    {"prepend", (PyCFunction)pybuffer_prepend, METH_VARARGS, buffer_prepend_doc},
    {"freeze", (PyCFunction)pybuffer_freeze, METH_VARARGS, buffer_freeze_doc},
    {"unfreeze", (PyCFunction)pybuffer_unfreeze, METH_VARARGS, buffer_unfreeze_doc},
    {"defer_callbacks", (PyCFunction)pybuffer_defer_callbacks, METH_VARARGS, buffer_defer_callbacks_doc},
    {"search", (PyCFunction)pybuffer_search, METH_VARARGS, buffer_search_doc},
    {NULL, NULL},
};

static Py_ssize_t
pybuffer_length(PyBufferObject *self)
{
    Py_ssize_t result;
    Py_BEGIN_ALLOW_THREADS
    result = evbuffer_get_length(self->buffer);
    Py_END_ALLOW_THREADS
    return result;
}

static int
pybuffer_contains(PyBufferObject *self, PyObject *value)
{
    struct evbuffer_ptr pos;
    if (!PyString_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "can only check for strings");
        return -1;
    }
    
    Py_BEGIN_ALLOW_THREADS
    pos = evbuffer_search(self->buffer, PyString_AS_STRING(value), PyString_GET_SIZE(value), NULL);
    Py_END_ALLOW_THREADS
    return (pos.pos == -1 ? 0 : 1);
}

static PySequenceMethods
pybuffer_as_seq = {
    (lenfunc)pybuffer_length,  /*sq_length*/
    NULL,  /*sq_concat*/
    NULL,  /*sq_repeat*/
    NULL,  /*sq_item*/
    NULL,  /*sq_slice*/
    NULL,  /*sq_ass_item*/
    NULL,  /*sq_ass_slice*/
    (objobjproc)pybuffer_contains,  /*sq_contains*/
    NULL,  /*sq_inplace_concat*/
    NULL   /*sq_inplace_repeat*/
};

static PyMappingMethods
pybuffer_as_mapping = {
	(lenfunc)pybuffer_length,  /*mp_length*/
	NULL,  /*mp_subscript*/
	NULL,  /*mp_ass_subscript*/
};
PyDoc_STRVAR(buffer_doc, "Buffer");

PyTypeObject
PyEventBuffer_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.Buffer",       /* tp_name */
    sizeof(PyBufferObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pybuffer_dealloc, /* tp_dealloc */
    0,                    /* tp_print */
    0,                    /* tp_getattr */
    0,                    /* tp_setattr */
    0,                    /* tp_compare */
    0,                    /* tp_repr */
    0,                    /* tp_as_number */
    &pybuffer_as_seq,     /* tp_as_sequence */
    &pybuffer_as_mapping, /* tp_as_mapping */
    0,                    /* tp_hash */
    0,                    /* tp_call */
    0,                    /* tp_str */
    0,                    /* tp_getattro */
    0,                    /* tp_setattro */
    0,                    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,   /* tp_flags */
    buffer_doc,           /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pybuffer_methods,     /* tp_methods */
    0,                    /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pybuffer_init, /* tp_init */
    0,                    /* tp_alloc */
    pybuffer_new,         /* tp_new */
    0,                    /* tp_free */
};
