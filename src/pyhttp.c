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
#include <event2/http.h>

#include "pyhttp.h"
#include "pybase.h"
#include "pybuffer.h"

typedef struct _PyHttpServerObject {
    PyObject_HEAD
    struct evhttp *http;
    PyEventBaseObject *base;
    PyObject *callbacks;
} PyHttpServerObject;

typedef struct _PyBoundSocketObject {
    PyObject_HEAD
    struct evhttp_bound_socket *socket;
    PyHttpServerObject *http;
} PyBoundSocketObject;

typedef struct _PyHttpCallbackObject {
    PyObject_HEAD
    PyHttpServerObject *http;
    PyObject *path;
    PyObject *callback;
    PyObject *userdata;
} PyHttpCallbackObject;

typedef struct _PyHttpRequestObject {
    PyObject_HEAD
    struct evhttp_request *request;
    PyHttpServerObject *http;
} PyHttpRequestObject;

static PyBoundSocketObject *
_pyhttp_new_bound_socket(PyHttpServerObject *self, struct evhttp_bound_socket *socket)
{
    PyBoundSocketObject *result = (PyBoundSocketObject *) PyBoundSocket_Type.tp_alloc(&PyBoundSocket_Type, 0);
    if (result == NULL) {
        Py_BEGIN_ALLOW_THREADS
        evhttp_del_accept_socket(self->http, socket);
        Py_END_ALLOW_THREADS
        return NULL;
    }
    
    result->socket = socket;
    result->http = self;
    Py_INCREF(self);
    return result;
}

static PyHttpCallbackObject *
_pyhttp_new_callback(PyHttpServerObject *self, char *path, PyObject *callback, PyObject *userdata)
{
    PyHttpCallbackObject *result = (PyHttpCallbackObject *) PyHttpCallback_Type.tp_alloc(&PyHttpCallback_Type, 0);
    if (result == NULL) {
        return NULL;
    }
    
    result->http = self;
    Py_INCREF(self);
    if (path != NULL) {
        result->path = PyString_FromString(path);
    } else {
        result->path = Py_None;
        Py_INCREF(Py_None);
    }
    result->callback = callback;
    Py_INCREF(callback);
    result->userdata = userdata;
    Py_INCREF(userdata);
    return result;
}

static PyHttpRequestObject *
_pyhttp_new_request(PyHttpServerObject *self, struct evhttp_request *request)
{
    PyHttpRequestObject *result = (PyHttpRequestObject *) PyHttpRequest_Type.tp_alloc(&PyHttpRequest_Type, 0);
    if (result == NULL) {
        return NULL;
    }
    
    result->http = self;
    Py_INCREF(self);
    result->request = request;
    return result;
}

static PyObject *
pyhttp_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyHttpServerObject *s;
    s = (PyHttpServerObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->http = NULL;
        s->base = NULL;
        s->callbacks = NULL;
    }
    return (PyObject *)s;
}

static int
pyhttp_init(PyHttpServerObject *self, PyObject *args, PyObject *kwds)
{
    PyEventBaseObject *base;
    if (!PyArg_ParseTuple(args, "O!", &PyEventBase_Type, &base))
        return -1;

    self->http = evhttp_new(base->base);
    if (self->http == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    
    self->base = base;
    Py_INCREF(base);
    self->callbacks = PyDict_New();
    return 0;
}

static void
pyhttp_dealloc(PyHttpServerObject *self)
{
    Py_BEGIN_ALLOW_THREADS
    if (self->http != NULL) {
        evhttp_free(self->http);
    }
    Py_END_ALLOW_THREADS
    Py_XDECREF(self->base);
    Py_DECREF(self->callbacks);
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pyhttp_bind_doc, "Binds an HTTP server on the specified address and port.");

static PyObject *
pyhttp_bind(PyHttpServerObject *self, PyObject *args)
{
    char *hostname;
    int port;
    struct evhttp_bound_socket *sock;
    
    if (!PyArg_ParseTuple(args, "si", &hostname, &port))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    sock = evhttp_bind_socket_with_handle(self->http, hostname, port);
    Py_END_ALLOW_THREADS
    if (sock == NULL) {
        PyErr_SetString(PyExc_TypeError, "could not bind socket");
        return NULL;
    }

    return (PyObject *) _pyhttp_new_bound_socket(self, sock);
}

PyDoc_STRVAR(pyhttp_accept_doc, "Makes an HTTP server accept connections on the specified socket.");

static PyObject *
pyhttp_accept(PyHttpServerObject *self, PyObject *args)
{
    PyObject *pysocket;
    struct evhttp_bound_socket *sock;
    int fd;
    
    if (!PyArg_ParseTuple(args, "O", &pysocket))
        return NULL;
    
    if (PyBoundSocket_Check(pysocket)) {
        Py_BEGIN_ALLOW_THREADS
        sock = evhttp_accept_socket_with_handle(self->http, evhttp_bound_socket_get_fd(((PyBoundSocketObject *) pysocket)->socket));
        Py_END_ALLOW_THREADS
    } else {
        PyObject *pyfd = PyNumber_Int(pysocket);
        if (pyfd == NULL) {
            PyErr_Format(PyExc_TypeError, "expected a BoundSocket or a number, not '%s'", pysocket->ob_type->tp_name);
            return NULL;
        }
        fd = PyInt_AsLong(pyfd);
        Py_DECREF(pyfd);
        Py_BEGIN_ALLOW_THREADS
        sock = evhttp_accept_socket_with_handle(self->http, fd);
        Py_END_ALLOW_THREADS
    }
    if (sock == NULL) {
        PyErr_SetString(PyExc_TypeError, "could not accept on socket");
        return NULL;
    }

    return (PyObject *) _pyhttp_new_bound_socket(self, sock);
}

PyDoc_STRVAR(pyhttp_set_max_headers_size_doc, "Set the maximum allowed size for request headers.");

static PyObject *
pyhttp_set_max_headers_size(PyHttpServerObject *self, PyObject *args)
{
    Py_ssize_t size;
    
    if (!PyArg_ParseTuple(args, "n", &size))
        return NULL;
    
    evhttp_set_max_headers_size(self->http, size);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_set_max_body_size_doc, "Set the maximum allowed size for request body.");

static PyObject *
pyhttp_set_max_body_size(PyHttpServerObject *self, PyObject *args)
{
    Py_ssize_t size;
    
    if (!PyArg_ParseTuple(args, "n", &size))
        return NULL;
    
    evhttp_set_max_body_size(self->http, size);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_set_allowed_methods_doc, "Sets the what HTTP methods are supported in requests.");

static PyObject *
pyhttp_set_allowed_methods(PyHttpServerObject *self, PyObject *args)
{
    int methods;
    
    if (!PyArg_ParseTuple(args, "i", &methods))
        return NULL;
    
    evhttp_set_allowed_methods(self->http, methods);
    Py_RETURN_NONE;
}

static void
_pyhttp_invoke_callback(struct evhttp_request *req, void *userdata)
{
    PyHttpCallbackObject *cb = (PyHttpCallbackObject *) userdata;
    START_BLOCK_THREADS
    PyHttpRequestObject *request = _pyhttp_new_request(cb->http, req);
    if (request == NULL) {
        pybase_store_error(cb->http->base);
    } else {
        PyObject *result = PyObject_CallFunction(cb->callback, "OOO", cb->http, request, cb->userdata);
        if (result == NULL) {
            pybase_store_error(cb->http->base);
        } else {
            Py_DECREF(result);
        }
        Py_DECREF((PyObject *) request);
    }
    END_BLOCK_THREADS
}

PyDoc_STRVAR(pyhttp_set_callback_doc, "Set a callback for a specified URI.");

static PyObject *
pyhttp_set_callback(PyHttpServerObject *self, PyObject *args)
{
    char *path;
    PyObject *callback;
    PyObject *userdata=Py_None;
    PyHttpCallbackObject *cb;
    int result;
    
    if (!PyArg_ParseTuple(args, "sO|O", &path, &callback, &userdata))
        return NULL;
    
    cb = _pyhttp_new_callback(self, path, callback, userdata);
    if (cb == NULL) {
        return NULL;
    }
    
    Py_BEGIN_ALLOW_THREADS
    result = evhttp_set_cb(self->http, path, _pyhttp_invoke_callback, cb);
    if (result == -1) {
        evhttp_del_cb(self->http, path);
        result = evhttp_set_cb(self->http, path, _pyhttp_invoke_callback, cb);
    }
    Py_END_ALLOW_THREADS
    if (result != 0) {
        PyErr_SetString(PyExc_TypeError, "could not set callback");
        return NULL;
    }

    PyDict_SetItemString(self->callbacks, path, (PyObject *) cb);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_del_callback_doc, "Remove callback for a specified URI.");

static PyObject *
pyhttp_del_callback(PyHttpServerObject *self, PyObject *args)
{
    char *path;
    
    if (!PyArg_ParseTuple(args, "s", &path))
        return NULL;
    
    Py_BEGIN_ALLOW_THREADS
    evhttp_del_cb(self->http, path);
    Py_END_ALLOW_THREADS

    if (PyDict_DelItemString(self->callbacks, path) != 0) {
        PyErr_Clear();
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_set_generic_callback_doc, "Set a callback for all requests that are not caught by specific callbacks.");

static PyObject *
pyhttp_set_generic_callback(PyHttpServerObject *self, PyObject *args)
{
    PyObject *callback;
    PyObject *userdata=Py_None;
    PyHttpCallbackObject *cb;
    
    if (!PyArg_ParseTuple(args, "O|O", &callback, &userdata))
        return NULL;

    if (callback == Py_None) {
        Py_BEGIN_ALLOW_THREADS
        evhttp_set_gencb(self->http, NULL, NULL);
        Py_END_ALLOW_THREADS

        if (PyDict_DelItem(self->callbacks, Py_None) != 0) {
            PyErr_Clear();
        }
        Py_RETURN_NONE;
    }
    
    cb = _pyhttp_new_callback(self, NULL, callback, userdata);
    if (cb == NULL) {
        return NULL;
    }
    
    Py_BEGIN_ALLOW_THREADS
    evhttp_set_gencb(self->http, _pyhttp_invoke_callback, cb);
    Py_END_ALLOW_THREADS

    Py_INCREF(Py_None);
    PyDict_SetItem(self->callbacks, Py_None, (PyObject *) cb);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_set_timeout_doc, "Set the timeout for an HTTP request.");

static PyObject *
pyhttp_set_timeout(PyHttpServerObject *self, PyObject *args)
{
    int timeout;
    
    if (!PyArg_ParseTuple(args, "i", &timeout))
        return NULL;
    
    evhttp_set_timeout(self->http, timeout);
    Py_RETURN_NONE;
}

static PyMethodDef
pyhttp_methods[] = {
    {"bind", (PyCFunction)pyhttp_bind, METH_VARARGS, pyhttp_bind_doc},
    {"accept", (PyCFunction)pyhttp_accept, METH_VARARGS, pyhttp_accept_doc},
    {"set_max_headers_size", (PyCFunction)pyhttp_set_max_headers_size, METH_VARARGS, pyhttp_set_max_headers_size_doc},
    {"set_max_body_size", (PyCFunction)pyhttp_set_max_body_size, METH_VARARGS, pyhttp_set_max_body_size_doc},
    {"set_allowed_methods", (PyCFunction)pyhttp_set_allowed_methods, METH_VARARGS, pyhttp_set_allowed_methods_doc},
    {"set_callback", (PyCFunction)pyhttp_set_callback, METH_VARARGS, pyhttp_set_callback_doc},
    {"del_callback", (PyCFunction)pyhttp_del_callback, METH_VARARGS, pyhttp_del_callback_doc},
    {"set_generic_callback", (PyCFunction)pyhttp_set_generic_callback, METH_VARARGS, pyhttp_set_generic_callback_doc},
    {"set_timeout", (PyCFunction)pyhttp_set_timeout, METH_VARARGS, pyhttp_set_timeout_doc},
    {NULL, NULL},
};

PyDoc_STRVAR(pyhttp_doc, "A basic HTTP server");

PyTypeObject
PyHttpServer_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.HttpServer",       /* tp_name */
    sizeof(PyHttpServerObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pyhttp_dealloc, /* tp_dealloc */
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
    pyhttp_doc,           /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pyhttp_methods,       /* tp_methods */
    0,                    /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    (initproc)pyhttp_init, /* tp_init */
    0,                    /* tp_alloc */
    pyhttp_new,           /* tp_new */
    0,                    /* tp_free */
};

static PyObject *
pybound_socket_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyBoundSocketObject *s;
    s = (PyBoundSocketObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->socket = NULL;
        s->http = NULL;
    }
    return (PyObject *)s;
}

static void
pybound_socket_dealloc(PyBoundSocketObject *self)
{
    if (self->http != NULL) {
        Py_BEGIN_ALLOW_THREADS
        evhttp_del_accept_socket(self->http->http, self->socket);
        Py_END_ALLOW_THREADS
    }
    Py_XDECREF(self->http);
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pybound_socket_doc, "A bound HTTP socket");

PyTypeObject
PyBoundSocket_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.BoundSocket",       /* tp_name */
    sizeof(PyBoundSocketObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pybound_socket_dealloc, /* tp_dealloc */
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
    pybound_socket_doc,   /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    0,                    /* tp_methods */
    0,                    /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    0,                    /* tp_init */
    0,                    /* tp_alloc */
    pybound_socket_new,   /* tp_new */
    0,                    /* tp_free */
};

static PyObject *
pyhttp_callback_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyHttpCallbackObject *s;
    s = (PyHttpCallbackObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->http = NULL;
        s->path = NULL;
        s->callback = NULL;
        s->userdata = NULL;
    }
    return (PyObject *)s;
}

static void
pyhttp_callback_dealloc(PyHttpCallbackObject *self)
{
    if (self->path != NULL) {
        Py_BEGIN_ALLOW_THREADS
        evhttp_del_cb(self->http->http, PyString_AS_STRING(self->path));
        Py_END_ALLOW_THREADS
    }
    Py_XDECREF(self->http);
    Py_XDECREF(self->path);
    Py_XDECREF(self->callback);
    Py_XDECREF(self->userdata);
    Py_TYPE(self)->tp_free(self);
}

PyTypeObject
PyHttpCallback_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.HttpCallback", /* tp_name */
    sizeof(PyHttpCallbackObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pyhttp_callback_dealloc, /* tp_dealloc */
    0,                    /* tp_print */
    0,                    /* tp_getattr */
    0,                    /* tp_setattr */
    0,                    /* tp_compare */
    0,                    /* tp_repr */
    0,                    /* tp_as_number */
    0,                    /* tp_as_sequence */
    0,                    /* tp_as_mappping */
    0,                    /* tp_hash */
    0,                    /* tp_call */
    0,                    /* tp_str */
    0,                    /* tp_getattro */
    0,                    /* tp_setattro */
    0,                    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,   /* tp_flags */
    0,                    /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    0,                    /* tp_methods */
    0,                    /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    0,                    /* tp_init */
    0,                    /* tp_alloc */
    pyhttp_callback_new,  /* tp_new */
    0,                    /* tp_free */
};

static PyObject *
pyhttp_request_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyHttpRequestObject *s;
    s = (PyHttpRequestObject *)type->tp_alloc(type, 0);
    if (s != NULL) {
        s->http = NULL;
        s->request = NULL;
    }
    return (PyObject *)s;
}

static void
pyhttp_request_dealloc(PyHttpRequestObject *self)
{
    if (self->request != NULL) {
        Py_BEGIN_ALLOW_THREADS
        evhttp_request_free(self->request);
        Py_END_ALLOW_THREADS
    }
    Py_XDECREF(self->http);
    Py_TYPE(self)->tp_free(self);
}

PyDoc_STRVAR(pyhttp_request_send_error_doc, "Send an HTML error message to the client.");

static PyObject *
pyhttp_request_send_error(PyHttpRequestObject *self, PyObject *args)
{
    int code;
    char *reason=NULL;
    
    if (!PyArg_ParseTuple(args, "i|s", &code, &reason))
        return NULL;
    
    if (self->request == NULL) {
        PyErr_SetString(PyExc_TypeError, "request already completed");
        return NULL;
    }
    
    Py_BEGIN_ALLOW_THREADS
    evhttp_send_error(self->request, code, reason);
    Py_END_ALLOW_THREADS
    self->request = NULL;
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_request_send_reply_doc, "Send an HTML reply to the client.");

static PyObject *
pyhttp_request_send_reply(PyHttpRequestObject *self, PyObject *args)
{
    int code;
    char *reason;
    PyObject *pydata;
    char *data;
    Py_ssize_t length;
    
    if (!PyArg_ParseTuple(args, "isO", &code, &reason, &pydata))
        return NULL;
    
    if (self->request == NULL) {
        PyErr_SetString(PyExc_TypeError, "request already completed");
        return NULL;
    }
    
    if (PyEventBuffer_Check(pydata)) {
        Py_BEGIN_ALLOW_THREADS
        evhttp_send_reply(self->request, code, reason, ((PyBufferObject *) data)->buffer);
        Py_END_ALLOW_THREADS
    } else {
        struct evbuffer *buffer = NULL;
        if (PyObject_AsReadBuffer(pydata, (const void **) &data, &length) != 0) {
            return NULL;
        }
        
        buffer = evbuffer_new();
        if (buffer == NULL) {
            return PyErr_NoMemory();
        }
        evbuffer_add(buffer, data, length);
        Py_BEGIN_ALLOW_THREADS
        evhttp_send_reply(self->request, code, reason, buffer);
        Py_END_ALLOW_THREADS
        evbuffer_free(buffer);
    }
    
    self->request = NULL;
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_request_send_reply_start_doc, "Initiate a reply that uses Transfer-Encoding chunked.");

static PyObject *
pyhttp_request_send_reply_start(PyHttpRequestObject *self, PyObject *args)
{
    int code;
    char *reason=NULL;
    
    if (!PyArg_ParseTuple(args, "i|s", &code, &reason))
        return NULL;
    
    if (self->request == NULL) {
        PyErr_SetString(PyExc_TypeError, "request already completed");
        return NULL;
    }
    
    Py_BEGIN_ALLOW_THREADS
    evhttp_send_reply_start(self->request, code, reason);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_request_send_reply_chunk_doc, "Send another data chunk as part of an ongoing chunked reply.");

static PyObject *
pyhttp_request_send_reply_chunk(PyHttpRequestObject *self, PyObject *args)
{
    PyObject *pydata;
    char *data;
    Py_ssize_t length;
    
    if (!PyArg_ParseTuple(args, "O", &pydata))
        return NULL;
    
    if (self->request == NULL) {
        PyErr_SetString(PyExc_TypeError, "request already completed");
        return NULL;
    }
    
    if (PyEventBuffer_Check(pydata)) {
        Py_BEGIN_ALLOW_THREADS
        evhttp_send_reply_chunk(self->request, ((PyBufferObject *) data)->buffer);
        Py_END_ALLOW_THREADS
    } else {
        struct evbuffer *buffer;
        if (PyObject_AsReadBuffer(pydata, (const void **) &data, &length) != 0) {
            return NULL;
        }
        
        buffer = evbuffer_new();
        if (buffer == NULL) {
            return PyErr_NoMemory();
        }
        evbuffer_add(buffer, data, length);
        Py_BEGIN_ALLOW_THREADS
        evhttp_send_reply_chunk(self->request, buffer);
        Py_END_ALLOW_THREADS
        evbuffer_free(buffer);
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(pyhttp_request_send_reply_end_doc, "Complete a chunked reply.");

static PyObject *
pyhttp_request_send_reply_end(PyHttpRequestObject *self, PyObject *args)
{
    if (self->request == NULL) {
        PyErr_SetString(PyExc_TypeError, "request already completed");
        return NULL;
    }
    
    Py_BEGIN_ALLOW_THREADS
    evhttp_send_reply_end(self->request);
    Py_END_ALLOW_THREADS
    self->request = NULL;
    Py_RETURN_NONE;
}

static PyMethodDef
pyhttp_request_methods[] = {
    {"send_error", (PyCFunction)pyhttp_request_send_error, METH_VARARGS, pyhttp_request_send_error_doc},
    {"send_reply", (PyCFunction)pyhttp_request_send_reply, METH_VARARGS, pyhttp_request_send_reply_doc},
    {"send_reply_start", (PyCFunction)pyhttp_request_send_reply_start, METH_VARARGS, pyhttp_request_send_reply_start_doc},
    {"send_reply_chunk", (PyCFunction)pyhttp_request_send_reply_chunk, METH_VARARGS, pyhttp_request_send_reply_chunk_doc},
    {"send_reply_end", (PyCFunction)pyhttp_request_send_reply_end, METH_NOARGS, pyhttp_request_send_reply_end_doc},
    {NULL, NULL},
};

PyDoc_STRVAR(pyhttp_request_doc, "A HTTP request");

PyTypeObject
PyHttpRequest_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                    /* tp_internal */
    "event.HttpRequest",  /* tp_name */
    sizeof(PyHttpRequestObject), /* tp_basicsize */
    0,                    /* tp_itemsize */
    (destructor)pyhttp_request_dealloc, /* tp_dealloc */
    0,                    /* tp_print */
    0,                    /* tp_getattr */
    0,                    /* tp_setattr */
    0,                    /* tp_compare */
    0,                    /* tp_repr */
    0,                    /* tp_as_number */
    0,                    /* tp_as_sequence */
    0,                    /* tp_as_mappping */
    0,                    /* tp_hash */
    0,                    /* tp_call */
    0,                    /* tp_str */
    0,                    /* tp_getattro */
    0,                    /* tp_setattro */
    0,                    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,   /* tp_flags */
    pyhttp_request_doc,   /* tp_doc */
    0,                    /* tp_traverse */
    0,                    /* tp_clear */
    0,                    /* tp_richcompare */
    0,                    /* tp_weaklistoffset */
    0,                    /* tp_iter */
    0,                    /* tp_iternext */
    pyhttp_request_methods, /* tp_methods */
    0,                    /* tp_members */
    0,                    /* tp_getset */
    0,                    /* tp_base */
    0,                    /* tp_dict */
    0,                    /* tp_descr_get */
    0,                    /* tp_descr_set */
    0,                    /* tp_dictoffset */
    0,                    /* tp_init */
    0,                    /* tp_alloc */
    pyhttp_request_new,   /* tp_new */
    0,                    /* tp_free */
};
