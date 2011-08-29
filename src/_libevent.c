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

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/http.h>
#include <event2/listener.h>
#if defined(WITH_THREAD)
#include <event2/thread.h>
#endif

#include "pybase.h"
#include "pyevent.h"
#include "pybuffer.h"
#include "pybufferevent.h"
#include "pyhttp.h"
#include "pylistener.h"

#if !defined(PyModule_AddIntMacro)
#define PyModule_AddIntMacro(module, name)      PyModule_AddIntConstant(module, #name, name);
#endif
#if !defined(PyModule_AddStringMacro)
#define PyModule_AddStringMacro(module, name)   PyModule_AddStringConstant(module, #name, name);
#endif

static PyObject *pylog_callback;
static PyObject *pyfatal_callback;

static PyObject *
enable_debug_mode(PyObject *self, PyObject *args)
{
    event_enable_debug_mode();
    Py_RETURN_NONE;
}

static PyObject *
socket_get_error(PyObject *self, PyObject *args)
{
    evutil_socket_t sock;
    
    if (!PyArg_ParseTuple(args, "i", &sock))
        return NULL;
    
    return PyInt_FromLong(evutil_socket_geterror(sock));
}

static PyObject *
socket_error_to_string(PyObject *self, PyObject *args)
{
    int errorcode;
    
    if (!PyArg_ParseTuple(args, "i", &errorcode))
        return NULL;
    
    return PyString_FromString(evutil_socket_error_to_string(errorcode));
}

static void
_pylog_callback(int severity, const char *msg)
{
    START_BLOCK_THREADS
    if (pylog_callback != NULL) {
        PyObject *result = PyObject_CallFunction(pylog_callback, "is", severity, msg);
        if (result == NULL) {
            PyErr_Print();
        } else {
            Py_DECREF(result);
        }
    }
    END_BLOCK_THREADS
}

static PyObject *
set_log_callback(PyObject *self, PyObject *args)
{
    PyObject *cb;
    PyObject *tmp;
    
    if (!PyArg_ParseTuple(args, "O", &cb))
        return NULL;
    
    if (cb != Py_None && !PyCallable_Check(cb)) {
        PyErr_Format(PyExc_TypeError, "expected a callable or None, not %s", cb->ob_type->tp_name);
        return NULL;
    }
    
    if (cb == pylog_callback) {
        // No change
        Py_RETURN_NONE;
    }
    
    tmp = pylog_callback;
    pylog_callback = NULL;
    Py_XDECREF(tmp);
    if (cb == Py_None) {
        event_set_log_callback(NULL);
    } else {
        pylog_callback = cb;
        Py_INCREF(cb);
        event_set_log_callback(_pylog_callback);
    }
    
    Py_RETURN_NONE;
}

static void
_pyfatal_callback(int err)
{
    START_BLOCK_THREADS
    if (pyfatal_callback != NULL) {
        PyObject *result = PyObject_CallFunction(pyfatal_callback, "i", err);
        if (result == NULL) {
            PyErr_Print();
        } else {
            Py_DECREF(result);
        }
    }
    END_BLOCK_THREADS
}

static PyObject *
set_fatal_callback(PyObject *self, PyObject *args)
{
    PyObject *cb;
    PyObject *tmp;
    
    if (!PyArg_ParseTuple(args, "O", &cb))
        return NULL;
    
    if (cb != Py_None && !PyCallable_Check(cb)) {
        PyErr_Format(PyExc_TypeError, "expected a callable or None, not %s", cb->ob_type->tp_name);
        return NULL;
    }
    
    if (cb == pyfatal_callback) {
        // No change
        Py_RETURN_NONE;
    }
    
    tmp = pyfatal_callback;
    pyfatal_callback = NULL;
    Py_XDECREF(tmp);
    if (cb == Py_None) {
        event_set_fatal_callback(NULL);
    } else {
        pyfatal_callback = cb;
        Py_INCREF(cb);
        event_set_fatal_callback(_pyfatal_callback);
    }
    
    Py_RETURN_NONE;
}

PyMethodDef
methods[] = {
    // exported functions
    {"enable_debug_mode", (PyCFunction)enable_debug_mode, METH_NOARGS, NULL},
    {"socket_get_error", (PyCFunction)socket_get_error, METH_VARARGS, NULL},
    {"socket_error_to_string", (PyCFunction)socket_error_to_string, METH_VARARGS, NULL},
    {"set_log_callback", (PyCFunction)set_log_callback, METH_VARARGS, NULL},
    {"set_fatal_callback", (PyCFunction)set_fatal_callback, METH_VARARGS, NULL},
    {NULL, NULL},
};

PyMODINIT_FUNC
init_libevent(void)
{
    PyObject *m;
    PyObject *base_methods;
    const char **evmethods;

    pylog_callback = NULL;
    pyfatal_callback = NULL;

#if defined(WITH_THREAD)
    // enable thread support in Python
    PyEval_InitThreads();
    
    // enable thread support in libevent
#if defined(EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED)
    evthread_use_windows_threads();
#endif
#if defined(EVTHREAD_USE_PTHREADS_IMPLEMENTED)
    evthread_use_pthreads();
#endif
#endif

    PyEventBase_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyEventBase_Type) < 0)
        return;

    PyConfig_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyConfig_Type) < 0)
        return;

    PyEvent_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyEvent_Type) < 0)
        return;

    PyEventBuffer_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyEventBuffer_Type) < 0)
        return;

    PyBufferEvent_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyBufferEvent_Type) < 0)
        return;

    PyBucketConfig_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyBucketConfig_Type) < 0)
        return;

    PyHttpServer_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyHttpServer_Type) < 0)
        return;

    PyBoundSocket_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyBoundSocket_Type) < 0)
        return;

    PyHttpCallback_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyHttpCallback_Type) < 0)
        return;

    PyHttpRequest_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyHttpRequest_Type) < 0)
        return;

    PyListener_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyListener_Type) < 0)
        return;

    base_methods = PyTuple_New(0);
    if (base_methods == NULL) {
        return;
    }
    evmethods = event_get_supported_methods();
    while (*evmethods != NULL) {
        _PyTuple_Resize(&base_methods, PyTuple_GET_SIZE(base_methods)+1);
        if (base_methods == NULL) {
            return;
        }
        
        PyTuple_SET_ITEM(base_methods, PyTuple_GET_SIZE(base_methods)-1, PyString_FromString(*evmethods));
        evmethods++;
    }

    m = Py_InitModule("_libevent", methods);

    PyModule_AddObject(m, "METHODS", base_methods);

    Py_INCREF(&PyEventBase_Type);
    PyModule_AddObject(m, "Base", (PyObject *)&PyEventBase_Type);
    Py_INCREF(&PyConfig_Type);
    PyModule_AddObject(m, "Config", (PyObject *)&PyConfig_Type);
    Py_INCREF(&PyEvent_Type);
    PyModule_AddObject(m, "Event", (PyObject *)&PyEvent_Type);
    Py_INCREF(&PyEventBuffer_Type);
    PyModule_AddObject(m, "Buffer", (PyObject *)&PyEventBuffer_Type);
    Py_INCREF(&PyBufferEvent_Type);
    PyModule_AddObject(m, "BufferEvent", (PyObject *)&PyBufferEvent_Type);
    Py_INCREF(&PyBucketConfig_Type);
    PyModule_AddObject(m, "BucketConfig", (PyObject *)&PyBucketConfig_Type);
    Py_INCREF(&PyHttpServer_Type);
    PyModule_AddObject(m, "HttpServer", (PyObject *)&PyHttpServer_Type);
    Py_INCREF(&PyBoundSocket_Type);
    PyModule_AddObject(m, "BoundSocket", (PyObject *)&PyBoundSocket_Type);
    Py_INCREF(&PyHttpRequest_Type);
    PyModule_AddObject(m, "HttpRequest", (PyObject *)&PyHttpRequest_Type);
    Py_INCREF(&PyListener_Type);
    PyModule_AddObject(m, "Listener", (PyObject *)&PyListener_Type);

    // event.h flags
    PyModule_AddIntMacro(m, EV_FEATURE_ET);
    PyModule_AddIntMacro(m, EV_FEATURE_O1);
    PyModule_AddIntMacro(m, EV_FEATURE_FDS);
    
    PyModule_AddIntMacro(m, EVENT_BASE_FLAG_NOLOCK);
    PyModule_AddIntMacro(m, EVENT_BASE_FLAG_IGNORE_ENV);
    PyModule_AddIntMacro(m, EVENT_BASE_FLAG_STARTUP_IOCP);
    PyModule_AddIntMacro(m, EVENT_BASE_FLAG_NO_CACHE_TIME);
    PyModule_AddIntMacro(m, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);

    PyModule_AddIntMacro(m, EVLOOP_ONCE);
    PyModule_AddIntMacro(m, EVLOOP_NONBLOCK);

    PyModule_AddIntMacro(m, EV_TIMEOUT);
    PyModule_AddIntMacro(m, EV_READ);
    PyModule_AddIntMacro(m, EV_WRITE);
    PyModule_AddIntMacro(m, EV_SIGNAL);
    PyModule_AddIntMacro(m, EV_PERSIST);
    PyModule_AddIntMacro(m, EV_ET);

    PyModule_AddStringMacro(m, LIBEVENT_VERSION);
    PyModule_AddIntMacro(m, LIBEVENT_VERSION_NUMBER);
    PyModule_AddIntMacro(m, EVENT_MAX_PRIORITIES);
    
    PyModule_AddIntConstant(m, "EVENT_LOG_DEBUG", _EVENT_LOG_DEBUG);
    PyModule_AddIntConstant(m, "EVENT_LOG_MSG", _EVENT_LOG_MSG);
    PyModule_AddIntConstant(m, "EVENT_LOG_WARN", _EVENT_LOG_WARN);
    PyModule_AddIntConstant(m, "EVENT_LOG_ERR", _EVENT_LOG_ERR);
    
    // buffer.h flags
    PyModule_AddIntMacro(m, EVBUFFER_EOL_ANY);
    PyModule_AddIntMacro(m, EVBUFFER_EOL_CRLF);
    PyModule_AddIntMacro(m, EVBUFFER_EOL_CRLF_STRICT);
    PyModule_AddIntMacro(m, EVBUFFER_EOL_LF);

    PyModule_AddIntMacro(m, EVBUFFER_PTR_SET);
    PyModule_AddIntMacro(m, EVBUFFER_PTR_ADD);
    
    // bufferevent.h flags
    PyModule_AddIntMacro(m, BEV_EVENT_READING);
    PyModule_AddIntMacro(m, BEV_EVENT_WRITING);
    PyModule_AddIntMacro(m, BEV_EVENT_EOF);
    PyModule_AddIntMacro(m, BEV_EVENT_ERROR);
    PyModule_AddIntMacro(m, BEV_EVENT_TIMEOUT);
    PyModule_AddIntMacro(m, BEV_EVENT_CONNECTED);

    PyModule_AddIntMacro(m, BEV_OPT_CLOSE_ON_FREE);
    PyModule_AddIntMacro(m, BEV_OPT_THREADSAFE);
    PyModule_AddIntMacro(m, BEV_OPT_DEFER_CALLBACKS);
    PyModule_AddIntMacro(m, BEV_OPT_UNLOCK_CALLBACKS);
    
    PyModule_AddIntMacro(m, EV_RATE_LIMIT_MAX);
    
    // http.h flags
    PyModule_AddIntMacro(m, HTTP_OK);
    PyModule_AddIntMacro(m, HTTP_NOCONTENT);
    PyModule_AddIntMacro(m, HTTP_MOVEPERM);
    PyModule_AddIntMacro(m, HTTP_MOVETEMP);
    PyModule_AddIntMacro(m, HTTP_NOTMODIFIED);
    PyModule_AddIntMacro(m, HTTP_BADREQUEST);
    PyModule_AddIntMacro(m, HTTP_NOTFOUND);
    PyModule_AddIntMacro(m, HTTP_BADMETHOD);
    PyModule_AddIntMacro(m, HTTP_INTERNAL);
    PyModule_AddIntMacro(m, HTTP_BADMETHOD);
    PyModule_AddIntMacro(m, HTTP_NOTIMPLEMENTED);
    PyModule_AddIntMacro(m, HTTP_SERVUNAVAIL);
    
    PyModule_AddIntMacro(m, EVHTTP_REQ_GET);
    PyModule_AddIntMacro(m, EVHTTP_REQ_POST);
    PyModule_AddIntMacro(m, EVHTTP_REQ_HEAD);
    PyModule_AddIntMacro(m, EVHTTP_REQ_PUT);
    PyModule_AddIntMacro(m, EVHTTP_REQ_DELETE);
    PyModule_AddIntMacro(m, EVHTTP_REQ_OPTIONS);
    PyModule_AddIntMacro(m, EVHTTP_REQ_TRACE);
    PyModule_AddIntMacro(m, EVHTTP_REQ_CONNECT);
    PyModule_AddIntMacro(m, EVHTTP_REQ_PATCH);

    // listener.h flags
    PyModule_AddIntMacro(m, LEV_OPT_LEAVE_SOCKETS_BLOCKING);
    PyModule_AddIntMacro(m, LEV_OPT_CLOSE_ON_FREE);
    PyModule_AddIntMacro(m, LEV_OPT_CLOSE_ON_EXEC);
    PyModule_AddIntMacro(m, LEV_OPT_REUSEABLE);
    PyModule_AddIntMacro(m, LEV_OPT_THREADSAFE);
}
