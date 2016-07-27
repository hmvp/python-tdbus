/*
 * This file is part of python-tdbus. Python-tdbus is free software
 * available under the terms of the MIT license. See the file "LICENSE" that
 * was provided together with this source file for the licensing terms.
 *
 * Copyright (c) 2012 the python-tdbus authors. See the file "AUTHORS" for a
 * complete list.
 *
 * This file implements the "_tdbus" module. It exposes parts of the libdbus
 * API to Python. This module should probably not be used directly in most
 * cases. Instead, use the "tdbus" module that provides a higher level API.
 */

#include <Python.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#include <dbus/dbus.h>

#if PY_MAJOR_VERSION >= 3
    #define PyInt_AsLong PyLong_AsLong
    #define PyInt_FromLong PyLong_FromLong
    #define PyInt_FromString PyLong_FromString
    #define PyInt_AsUnsignedLongMask PyLong_AsUnsignedLongMask
    #define PyInt_AsUnsignedLongLongMask PyLong_AsUnsignedLongLongMask

    #define PyString_FromString PyUnicode_FromString
#endif

/*
 * Some macros to make Python extensions in C less verbose.
 */

#define RETURN_ERROR(fmt, ...) \
    do { \
        if ((fmt) != NULL) PyErr_Format(GETSTATE()->error, fmt, ## __VA_ARGS__); \
        goto error; \
    } while (0)

#define RETURN_MEMORY_ERROR(err) \
    do { PyErr_NoMemory(); goto error; } while (0)

#define RETURN_DBUS_ERROR(err) \
    do { \
        if (dbus_error_is_set(&err)) RETURN_ERROR("%s", err.message); \
        else RETURN_ERROR("unknown error"); \
    } while (0)

#define CHECK_ERROR(cond, fmt, ...) \
    do { if (cond) { RETURN_ERROR(fmt, ## __VA_ARGS__); } } while (0)

#define CHECK_PYTHON_ERROR(cond) \
    CHECK_ERROR(cond, NULL)

#define CHECK_MEMORY_ERROR(cond) \
    do { if (cond) { RETURN_MEMORY_ERROR(); } } while (0)

#define MALLOC(var, size) \
    do { CHECK_MEMORY_ERROR((var = malloc(size)) == NULL); } while (0)

#define ASSERT(cond) \
    do { if (!(cond)) { \
        PyErr_SetString(PyExc_AssertionError, "assertion failed " #cond); \
        RETURN_ERROR(NULL); \
    } } while (0)


struct module_state {
    PyObject *error;
    int app_slot;
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef;
#define GETMSTATE(m) ((struct module_state*)PyModule_GetState(m))
#define GETSTATE() GETMSTATE(PyState_FindModule(&moduledef))
#else
#define GETMSTATE(m) (&_state)
#define GETSTATE() (&_state)
static struct module_state _state;
#endif

void _tdbus_decref(void *data)
{
    Py_DECREF((PyObject *) data);
}


/*
 * Watch object: used with event loop integration
 */

typedef struct
{
    PyObject_HEAD
    DBusWatch *watch;
    PyObject *data;
} PyTDBusWatchObject;

static PyTypeObject PyTDBusWatchType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_tdbus.Watch",
    sizeof(PyTDBusWatchObject)
};

static void
tdbus_watch_dealloc(PyTDBusWatchObject *self)
{
    if (self->data) {
        Py_DECREF(self->data);
        self->data = NULL;
    }
    PyObject_Del(self);
}

static PyObject *
tdbus_watch_get_fd(PyTDBusWatchObject *self, PyObject *args)
{
    long fd;
    PyObject *Pfd;

    if (!PyArg_ParseTuple(args, ":get_fd"))
        return NULL;

    fd = dbus_watch_get_unix_fd(self->watch);
    if (fd == -1)
        fd = dbus_watch_get_socket(self->watch);
    Pfd = PyInt_FromLong(fd);
    CHECK_PYTHON_ERROR(Pfd == NULL);
    return Pfd;

error:
    return NULL;
}

static PyObject *
tdbus_watch_get_flags(PyTDBusWatchObject *self, PyObject *args)
{
    int flags;
    PyObject *Pflags;

    if (!PyArg_ParseTuple(args, ":get_flags"))
        return NULL;

    flags = dbus_watch_get_flags(self->watch);
    Pflags = PyInt_FromLong(flags);
    CHECK_PYTHON_ERROR(Pflags == NULL);
    return Pflags;

error:
    return NULL;
}

static PyObject *
tdbus_watch_get_enabled(PyTDBusWatchObject *self, PyObject *args)
{
    int enabled;
    PyObject *Penabled;

    if (!PyArg_ParseTuple(args, ":get_enabled"))
        return NULL;

    enabled = dbus_watch_get_enabled(self->watch);
    Penabled = PyBool_FromLong(enabled);
    CHECK_PYTHON_ERROR(Penabled == NULL);
    return Penabled;

error:
    return NULL;
}

static PyObject *
tdbus_watch_get_data(PyTDBusWatchObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_data"))
        return NULL;

    if (self->data == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return self->data;
}

static PyObject *
tdbus_watch_set_data(PyTDBusWatchObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O:set_data", &data))
        return NULL;

    if (self->data != NULL)
        Py_DECREF(self->data);
    if (data == Py_None) {
        self->data = NULL;
    } else {
        Py_INCREF(data);
        self->data = data;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
tdbus_watch_handle(PyTDBusWatchObject *self, PyObject *args)
{
    int flags, ret;

    if (!PyArg_ParseTuple(args, "i:handle", &flags))
        return NULL;

    ret = dbus_watch_handle(self->watch, flags);
    CHECK_MEMORY_ERROR(ret == FALSE);
    Py_INCREF(Py_None);
    return Py_None;

error:
    return NULL;
}

static PyMethodDef tdbus_watch_methods[] = \
{
    { "get_fd", (PyCFunction) tdbus_watch_get_fd, METH_VARARGS },
    { "get_flags", (PyCFunction) tdbus_watch_get_flags, METH_VARARGS },
    { "get_enabled", (PyCFunction) tdbus_watch_get_enabled, METH_VARARGS },
    { "get_data", (PyCFunction) tdbus_watch_get_data, METH_VARARGS },
    { "set_data", (PyCFunction) tdbus_watch_set_data, METH_VARARGS },
    { "handle", (PyCFunction) tdbus_watch_handle, METH_VARARGS },
    { NULL }
};


/*
 * Timeout object: used with event loop integration
 */

typedef struct
{
    PyObject_HEAD
    DBusTimeout *timeout;
    PyObject *data;
} PyTDBusTimeoutObject;

static PyTypeObject PyTDBusTimeoutType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_tdbus.Timeout",
    sizeof(PyTDBusTimeoutObject)
};

static void
tdbus_timeout_dealloc(PyTDBusTimeoutObject *self)
{
    if (self->data) {
        Py_DECREF(self->data);
        self->data = NULL;
    }
    PyObject_Del(self);
}

static PyObject *
tdbus_timeout_get_interval(PyTDBusTimeoutObject *self, PyObject *args)
{
    int timeout;
    PyObject *Ptimeout;

    if (!PyArg_ParseTuple(args, ":get_interval"))
        return NULL;

    timeout = dbus_timeout_get_interval(self->timeout);
    Ptimeout = PyInt_FromLong(timeout);
    CHECK_PYTHON_ERROR(Ptimeout == NULL);
    return Ptimeout;

error:
    return NULL;
}

static PyObject *
tdbus_timeout_get_enabled(PyTDBusTimeoutObject *self, PyObject *args)
{
    int enabled;
    PyObject *Penabled;

    if (!PyArg_ParseTuple(args, ":get_enabled"))
        return NULL;

    enabled = dbus_timeout_get_enabled(self->timeout);
    Penabled = PyBool_FromLong(enabled);
    CHECK_PYTHON_ERROR(Penabled == NULL);
    return Penabled;

error:
    return NULL;
}

static PyObject *
tdbus_timeout_get_data(PyTDBusTimeoutObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_data"))
        return NULL;

    if (self->data == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return self->data;
}

static PyObject *
tdbus_timeout_set_data(PyTDBusTimeoutObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O:set_data", &data))
        return NULL;

    if (self->data != NULL)
        Py_DECREF(self->data);
    if (data == Py_None) {
        self->data = NULL;
    } else {
        Py_INCREF(data);
        self->data = data;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
tdbus_timeout_handle(PyTDBusTimeoutObject *self, PyObject *args)
{
    int ret;

    if (!PyArg_ParseTuple(args, ":handle"))
        return NULL;

    ret = dbus_timeout_handle(self->timeout);
    CHECK_MEMORY_ERROR(ret == FALSE);
    Py_INCREF(Py_None);
    return Py_None;

error:
    return NULL;
}

PyMethodDef tdbus_timeout_methods[] = \
{
    { "get_interval", (PyCFunction) tdbus_timeout_get_interval, METH_VARARGS },
    { "get_enabled", (PyCFunction) tdbus_timeout_get_enabled, METH_VARARGS },
    { "get_data", (PyCFunction) tdbus_timeout_get_data, METH_VARARGS },
    { "set_data", (PyCFunction) tdbus_timeout_set_data, METH_VARARGS },
    { "handle", (PyCFunction) tdbus_timeout_handle, METH_VARARGS },
    { NULL }
};


/*
 * Message objects
 */

typedef struct
{
    PyObject_HEAD
    DBusMessage *message;
} PyTDBusMessageObject;

PyTypeObject PyTDBusMessageType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_tdbus.Message",
    sizeof(PyTDBusMessageObject)
};

static int
_tdbus_check_path(const char *path)
{
    int i=0;

    if (path[i] != '/')
        return 0;
    for (i++; path[i] != '\000'; i++) {
        if (!(isalnum(path[i]) || path[i] == '_' || (path[i] == '/'
                        && path[i-1] != '/' && path[i+1] != '\000')))
            return 0;
    }
    return 1;
}

static int
_tdbus_check_interface(const char *interface)
{
    int i=0, ndots=0;

    if (!(isalpha(interface[i]) || interface[i] == '_'))
        return 0;
    for (i++; interface[i] != '\000'; i++) {
        if (!(isalnum(interface[i]) || interface[i] == '_' || (interface[i] == '.'
                        && interface[i-1] != '.' && interface[i+1] != '\000')))
            return 0;
        if (interface[i] == '.') ndots++;
    }
    return (i <= DBUS_MAXIMUM_NAME_LENGTH) && (ndots > 0);
}

static int
_tdbus_check_member(const char *member)
{
    int i=0;
    
    if (!(isalpha(member[i]) || member[i] == '_'))
        return 0;
    for (i++; member[i] != '\000'; i++) {
        if (!(isalnum(member[i]) || member[i] == '_'))
            return 0;
    }
    return (i <= DBUS_MAXIMUM_NAME_LENGTH);
}

static int
_tdbus_check_bus_name(const char *name)
{
    int i=0, ndots=0;

    if (name[i] == ':') {
        if (name[++i] == '\000')
            return 0;
    }
    if (!(isalnum(name[i]) || name[i] == '_' || name[i] == '-'))
        return 0;
    for (i++; name[i] != '\000'; i++) {
        if (!(isalnum(name[i]) || name[i] == '_' || name[i] == '-' ||
                    (name[i] == '.' && name[i-1] != '.' && name[i+1] != '\000')))
            return 0;
        if (name[i] == '.') ndots++;
    }
    return (i <= DBUS_MAXIMUM_NAME_LENGTH) && (ndots > 0);
}

static int
tdbus_message_init(PyTDBusMessageObject *self, PyObject *args, PyObject *kwargs)
{
    int type, no_reply=0, auto_start=0;
    unsigned int reply_serial = 0;
    char *path=NULL, *interface=NULL, *member=NULL, *error_name=NULL, *destination=NULL;
    static char *kwlist[] = { "type", "no_reply", "auto_start", "path",
            "interface", "member", "error_name", "reply_serial", "destination", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|iissssIs", kwlist, &type, &no_reply,
                &auto_start, &path, &interface, &member, &error_name, &reply_serial, &destination))
        return -1;

    self->message = dbus_message_new(type);
    CHECK_MEMORY_ERROR(self->message == NULL);
    dbus_message_set_no_reply(self->message, no_reply);
    dbus_message_set_auto_start(self->message, auto_start);
    if (path) {
        if (!_tdbus_check_path(path))
            RETURN_ERROR("invalid path: %s", path);
        if (!dbus_message_set_path(self->message, path))
            RETURN_MEMORY_ERROR();
    }
    if (interface) {
        if (!_tdbus_check_interface(interface))
            RETURN_ERROR("invalid interface: %s", interface);
        if (!dbus_message_set_interface(self->message, interface))
            RETURN_MEMORY_ERROR();
    }
    if (member) {
        if (!_tdbus_check_member(member))
            RETURN_ERROR("invalid member: %s", member);
        if (!dbus_message_set_member(self->message, member))
            RETURN_MEMORY_ERROR();
    }
    if (error_name) {
        if (!_tdbus_check_interface(error_name))
            RETURN_ERROR("invalid error name: %s", error_name);
        if (!dbus_message_set_error_name(self->message, error_name))
            RETURN_MEMORY_ERROR();
    }
    if (reply_serial && !dbus_message_set_reply_serial(self->message, reply_serial))
        RETURN_MEMORY_ERROR();
    if (destination) {
        if (!_tdbus_check_bus_name(destination))
            RETURN_ERROR("illegal destination: %s", destination);
        if (!dbus_message_set_destination(self->message, destination))
            RETURN_MEMORY_ERROR();
    }

    return 0;

error:
    return -1;
}

static void
tdbus_message_dealloc(PyTDBusMessageObject *self)
{
    if (self->message) {
        dbus_message_unref(self->message);
        self->message = NULL;
    }
    PyObject_Del(self);
}

typedef union 
{
    dbus_bool_t bl;
    uint8_t u8;
    int16_t i16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    int64_t i64;
    uint64_t u64;
    char *str;
    double dbl;
} _tdbus_basic_value;

#define DEFINE_MESSAGE_GETTER(name, ctype, py_convert, none_value) \
    static PyObject *tdbus_message_get_ ## name(PyTDBusMessageObject *self, \
                                               PyObject *args) \
    { \
        ctype value; PyObject *Pvalue; \
        if (self->message == NULL) RETURN_ERROR("uninitialized object"); \
        if (!PyArg_ParseTuple(args, ":get_" #name)) return NULL; \
        value = dbus_message_get_ ## name(self->message); \
        if (value == none_value) { Py_INCREF(Py_None); return Py_None; } \
        if ((Pvalue = py_convert(value)) == NULL) RETURN_ERROR(NULL); \
        return Pvalue; \
        error: return NULL; \
    }

#define DEFINE_MESSAGE_SETTER(name, ctype, cformat) \
    static PyObject *tdbus_message_set_ ## name(PyTDBusMessageObject *self, \
                                                PyObject *args) \
    { \
        ctype value; \
        if (self->message == NULL) RETURN_ERROR("uninitialized object"); \
        if (!PyArg_ParseTuple(args, cformat ":set_" #name, &value)) \
            return NULL; \
        dbus_message_set_ ## name(self->message, value); \
        Py_INCREF(Py_None); return Py_None; \
        error: return NULL; \
    }

#define DEFINE_MESSAGE_SETTER_CHECK(name, ctype, cformat, check) \
    static PyObject *tdbus_message_set_ ## name(PyTDBusMessageObject *self, PyObject *args) \
    { \
        ctype value; \
        if (self->message == NULL) RETURN_ERROR("uninitialized object"); \
        if (!PyArg_ParseTuple(args, cformat ":set_" #name, &value)) return NULL; \
        if (!check(value)) RETURN_ERROR("illegal value for " #name ": %s", value); \
        if (!dbus_message_set_ ## name(self->message, value)) RETURN_MEMORY_ERROR(); \
        Py_INCREF(Py_None); return Py_None; \
        error: return NULL; \
    }

DEFINE_MESSAGE_GETTER(type, int, PyInt_FromLong, -1)
DEFINE_MESSAGE_GETTER(no_reply, int, PyBool_FromLong, -1)
DEFINE_MESSAGE_SETTER(no_reply, int, "i")
DEFINE_MESSAGE_GETTER(auto_start, int, PyBool_FromLong, -1)
DEFINE_MESSAGE_SETTER(auto_start, int, "i")
DEFINE_MESSAGE_GETTER(serial, long, PyInt_FromLong, -1)
DEFINE_MESSAGE_GETTER(path, const char *, PyString_FromString, NULL)
DEFINE_MESSAGE_SETTER_CHECK(path, const char *, "s", _tdbus_check_path)
DEFINE_MESSAGE_GETTER(interface, const char *, PyString_FromString, NULL)
DEFINE_MESSAGE_SETTER_CHECK(interface, const char *, "s", _tdbus_check_interface)
DEFINE_MESSAGE_GETTER(member, const char *, PyString_FromString, NULL)
DEFINE_MESSAGE_SETTER_CHECK(member, const char *, "s", _tdbus_check_member)
DEFINE_MESSAGE_GETTER(error_name, const char *, PyString_FromString, NULL)
DEFINE_MESSAGE_SETTER_CHECK(error_name, const char *, "s", _tdbus_check_interface)
DEFINE_MESSAGE_GETTER(reply_serial, unsigned long, PyLong_FromUnsignedLong, 0)
DEFINE_MESSAGE_SETTER(reply_serial, unsigned long, "l");
DEFINE_MESSAGE_GETTER(destination, const char *, PyString_FromString, NULL)
DEFINE_MESSAGE_SETTER_CHECK(destination, const char *, "s", _tdbus_check_bus_name)
DEFINE_MESSAGE_GETTER(sender, const char *, PyString_FromString, NULL)
DEFINE_MESSAGE_GETTER(signature, const char *, PyString_FromString, NULL)


static PyObject * _tdbus_message_read_args(DBusMessageIter *, int);

static PyObject *
_tdbus_message_read_arg(DBusMessageIter *iter, int depth)
{
    int type, subtype, ret, size;
    char *sig = NULL, *ptr;
    PyObject *Parg = NULL, *Pitem = NULL, *Pkey = NULL, *Pvalue = NULL;
    _tdbus_basic_value value;
    DBusMessageIter subiter;

    type = dbus_message_iter_get_arg_type(iter);
    switch (type) {
    case DBUS_TYPE_BYTE:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyInt_FromLong(value.u8);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_BOOLEAN:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyBool_FromLong(value.bl);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_INT16:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyInt_FromLong(value.i16);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_UINT16:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyInt_FromLong(value.u16);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_INT32:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyInt_FromLong(value.i32);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_UINT32:
        dbus_message_iter_get_basic(iter, &value);
        if (sizeof(long) == 8)
            Parg = PyInt_FromLong(value.u32);
        else
            Parg = PyLong_FromUnsignedLong(value.u32);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_INT64:
        dbus_message_iter_get_basic(iter, &value);
        if (sizeof(long) == 8)
            Parg = PyInt_FromLong(value.i64);
        else
            Parg = PyLong_FromLongLong(value.i64);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_UINT64:
        dbus_message_iter_get_basic(iter, &value);
        if (sizeof(long) == 8)
            Parg = PyLong_FromUnsignedLong(value.u64);
        else
            Parg = PyLong_FromUnsignedLongLong(value.u64);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_DOUBLE:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyFloat_FromDouble(value.dbl);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_STRING:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyUnicode_DecodeUTF8(value.str, strlen(value.str), NULL);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE:
        dbus_message_iter_get_basic(iter, &value);
        Parg = PyUnicode_FromString(value.str);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_STRUCT:
        dbus_message_iter_recurse(iter, &subiter);
        Parg = _tdbus_message_read_args(&subiter, depth+1);
        CHECK_PYTHON_ERROR(Parg == NULL);
        break;
    case DBUS_TYPE_ARRAY:
        subtype = dbus_message_iter_get_element_type(iter);
        dbus_message_iter_recurse(iter, &subiter);
        if (subtype == DBUS_TYPE_BYTE) {
            dbus_message_iter_get_fixed_array(&subiter, &ptr, &size);
            Parg = PyBytes_FromStringAndSize(ptr, size);
            CHECK_PYTHON_ERROR(Parg == NULL);
        } else {
            if (subtype == DBUS_TYPE_DICT_ENTRY)
                Parg = PyDict_New();
            else
                Parg = PyList_New(0);
            CHECK_PYTHON_ERROR(Parg == NULL);
            while (dbus_message_iter_get_arg_type(&subiter) != DBUS_TYPE_INVALID) {
                if ((Pitem = _tdbus_message_read_arg(&subiter, depth+1)) == NULL)
                    RETURN_ERROR(NULL);
                if (PyDict_Check(Parg)) {
                    ASSERT(PyTuple_Check(Pitem));
                    ASSERT(PyTuple_Size(Pitem) == 2);
                    ret = PyDict_SetItem(Parg, PyTuple_GET_ITEM(Pitem, 0),
                                         PyTuple_GET_ITEM(Pitem, 1));
                    CHECK_PYTHON_ERROR(ret < 0);
                } else
                    PyList_Append(Parg, Pitem);
                Py_DECREF(Pitem); Pitem = NULL;
                dbus_message_iter_next(&subiter);
            }
        }
        break;
    case DBUS_TYPE_DICT_ENTRY:
        dbus_message_iter_recurse(iter, &subiter);
        if ((Pkey = _tdbus_message_read_arg(&subiter, depth+1)) == NULL)
            RETURN_ERROR(NULL);
        if (!dbus_message_iter_next(&subiter))
            RETURN_ERROR("illegal dict_entry");
        if ((Pvalue = _tdbus_message_read_arg(&subiter, depth+1)) == NULL)
            RETURN_ERROR(NULL);
        Parg = PyTuple_New(2);
        CHECK_PYTHON_ERROR(Parg == NULL);
        PyTuple_SET_ITEM(Parg, 0, Pkey);
        PyTuple_SET_ITEM(Parg, 1, Pvalue);
        break;
    case DBUS_TYPE_VARIANT:
        dbus_message_iter_recurse(iter, &subiter);
        if ((sig = dbus_message_iter_get_signature(&subiter)) == NULL)
            RETURN_MEMORY_ERROR();
        Pkey = PyUnicode_FromString(sig);
        CHECK_PYTHON_ERROR(Pkey == NULL);
        if ((Pvalue = _tdbus_message_read_arg(&subiter, depth+1)) == NULL)
            RETURN_ERROR(NULL);
        Parg = PyTuple_New(2);
        CHECK_PYTHON_ERROR(Parg == NULL);
        PyTuple_SET_ITEM(Parg, 0, Pkey);
        PyTuple_SET_ITEM(Parg, 1, Pvalue);
        dbus_free(sig); sig = NULL;
        break;
    }

    return Parg;

error:
    if (Parg != NULL) Py_DECREF(Parg);
    if (Pitem != NULL) Py_DECREF(Pitem);
    if (Pkey != NULL) Py_DECREF(Pkey);
    if (Pvalue != NULL) Py_DECREF(Pvalue);
    if (sig != NULL) dbus_free(sig);
    return NULL;
}

static PyObject *
_tdbus_message_read_args(DBusMessageIter *iter, int depth)
{
    PyObject *Plist = NULL, *Pargs = NULL, *Parg = NULL;

    Plist = PyList_New(0);
    while (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {
        if ((Parg = _tdbus_message_read_arg(iter, depth)) == NULL)
            RETURN_ERROR(NULL);
        if (PyList_Append(Plist, Parg) < 0)
            RETURN_ERROR(NULL);
        Py_DECREF(Parg); Parg = NULL;
        dbus_message_iter_next(iter);
    }

    Pargs = PyList_AsTuple(Plist);
    CHECK_PYTHON_ERROR(Pargs == NULL);
    Py_DECREF(Plist);
    return Pargs;

error:
    if (Parg != NULL) Py_DECREF(Parg);
    if (Plist != NULL) Py_DECREF(Plist);
    if (Pargs != NULL) Py_DECREF(Pargs);
    return NULL;
}

static PyObject *
tdbus_message_get_args(PyTDBusMessageObject *self, PyObject *args)
{
    PyObject *Pargs;
    DBusMessageIter iter;
    
    if (!PyArg_ParseTuple(args, ":get_args"))
        return NULL;
    if (self->message == NULL)
        RETURN_ERROR("uninitialized object");

    if (dbus_message_iter_init(self->message, &iter))
        Pargs = _tdbus_message_read_args(&iter, 0);
    else
        Pargs = PyTuple_New(0);
    CHECK_PYTHON_ERROR(Pargs == NULL);
    return Pargs;

error:
    return NULL;
}


static PyObject **_tdbus_check_number_cache = NULL;
static char *_tdbus_check_numbers[11] = {
    "0", "0xff", "0xffff",
    "0xffffffff", "0xffffffffffffffff",
    "-0x8000", "0x7fff",
    "-0x80000000", "0x7fffffff",
    "-0x8000000000000000", "0x7fffffffffffffff"
};

static int
_tdbus_init_check_number_cache(void)
{
    int i;
    PyObject *Pnumber;

    MALLOC(_tdbus_check_number_cache, 11 * sizeof(PyObject *));
    for (i=0; i<11; i++) {
        if ((Pnumber = PyInt_FromString(_tdbus_check_numbers[i],
                    NULL, 0)) == NULL)
            RETURN_ERROR(NULL);
        _tdbus_check_number_cache[i] = Pnumber;
    }
    return 1;

error:
    if (_tdbus_check_number_cache != NULL) free(_tdbus_check_number_cache);
    return 0;
}

static int
_tdbus_check_number(PyObject *number, int type)
{
    PyObject *Pmin,  *Pmax;

    if (!PyNumber_Check(number))
        RETURN_ERROR("expecting integer argument for `%c' format", type);

    switch (type) {
    case DBUS_TYPE_BYTE:
        Pmin = _tdbus_check_number_cache[0];
        Pmax = _tdbus_check_number_cache[1];
        break;
    case DBUS_TYPE_UINT16:
        Pmin = _tdbus_check_number_cache[0];
        Pmax = _tdbus_check_number_cache[2];
        break;
    case DBUS_TYPE_UINT32:
        Pmin = _tdbus_check_number_cache[0];
        Pmax = _tdbus_check_number_cache[3];
        break;
    case DBUS_TYPE_UINT64:
        Pmin = _tdbus_check_number_cache[0];
        Pmax = _tdbus_check_number_cache[4];
        break;
    case DBUS_TYPE_INT16:
        Pmin = _tdbus_check_number_cache[5];
        Pmax = _tdbus_check_number_cache[6];
        break;
    case DBUS_TYPE_INT32:
        Pmin = _tdbus_check_number_cache[7];
        Pmax = _tdbus_check_number_cache[8];
        break;
    case DBUS_TYPE_INT64:
        Pmin = _tdbus_check_number_cache[9];
        Pmax = _tdbus_check_number_cache[10];
        break;
    default:
        return 0;
    }

    if (PyObject_RichCompareBool(number, Pmin, Py_LT) ||
                PyObject_RichCompareBool(number, Pmax, Py_GT))
        RETURN_ERROR("value out of range for `%c' format", type);
    return 1;

error:
    return 0;
}

static char *
_tdbus_get_one_full_type(char *format)
{
    int depth;
    char *end, endtype = '\000';

    switch (*format) {
    case DBUS_TYPE_ARRAY:
        end = _tdbus_get_one_full_type(format+1);
        break;
    case DBUS_STRUCT_BEGIN_CHAR:
        endtype = DBUS_STRUCT_END_CHAR;
        break;
    case DBUS_DICT_ENTRY_BEGIN_CHAR:
        endtype = DBUS_DICT_ENTRY_END_CHAR;
        break;
    case '\000':
        end = NULL;
        break;
    default:
        end = format+1;
        break;
    }

    if (endtype != '\000') {
        depth = 1; end = format;
        while (*++end != '\000' && depth > 0) {
            if (*end == *format) depth++;
            else if (*end == endtype) depth--;
        }
        if (depth)
            RETURN_ERROR("unbalanced `%c' format", *format);
    }
    return end;

error:
    return NULL;
}

static int
_tdbus_check_signature(char *format, int arraydepth, int structdepth)
{
    char *end, *start, store;
    int isarray, isstruct;

    start = end = format;
    while (*format != '\000') {
        end = _tdbus_get_one_full_type(format);
        if (end == NULL) return 0;
        store = *end; *end = '\000';
        if (end - format == 1) {
            if (!strchr("ybnqiuxtdsogvh", *format)) return 0;
        } else {
            isarray = isstruct = 0;
            if (*format == DBUS_TYPE_ARRAY) {
                if (arraydepth >= 32) return 0;
                isarray = 1;
            } else if (*format == DBUS_STRUCT_BEGIN_CHAR ||
                        *format == DBUS_DICT_ENTRY_BEGIN_CHAR) {
                if (structdepth >= 32) return 0;
                isstruct = 1;
            }
            if (!_tdbus_check_signature(format+1, arraydepth+isarray, structdepth+isstruct))
                return 0;
        }
        *(format = end) = store;
        if (*format == DBUS_STRUCT_END_CHAR ||
                    *format == DBUS_DICT_ENTRY_END_CHAR)
            format++;
    }
    if (end - start > 255)
        return 0;
    return 1;
}

static int
_tdbus_message_append_args(DBusMessageIter *, char *, PyObject *, int);

static int
_tdbus_message_append_arg(DBusMessageIter *iter, char *format,
                          PyObject *arg, int depth)
{
    int i, size; long l;
    char *subtype = NULL, *end, *ptr;
    PyObject *Parray, *Putf8, *Pitem = NULL, *Ptype = NULL, *Pvalue = NULL;
    _tdbus_basic_value value;
    DBusMessageIter subiter;

    switch (*format) {
    case DBUS_TYPE_BYTE:
        if (!_tdbus_check_number(arg, *format))
            RETURN_ERROR(NULL);
        value.u8 = PyInt_AsLong(arg);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_BOOLEAN:
        if ((l = PyObject_IsTrue(arg)) == -1)
            RETURN_ERROR(NULL);
        value.bl = l;
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_INT16:
        if (!_tdbus_check_number(arg, *format))
            RETURN_ERROR(NULL);
        value.i16 = PyInt_AsLong(arg);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_UINT16:
        if (!_tdbus_check_number(arg, *format))
            RETURN_ERROR(NULL);
        value.u16 = PyInt_AsLong(arg);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_INT32:
        if (!_tdbus_check_number(arg, *format))
            RETURN_ERROR(NULL);
        value.i32 = PyInt_AsLong(arg);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_UINT32:
        if (!_tdbus_check_number(arg, *format))
            RETURN_ERROR(NULL);
        if (sizeof(long) == 8)
            value.u32 = PyInt_AsLong(arg);
        else
            value.u32 = PyInt_AsUnsignedLongMask(arg);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_INT64:
        if (!_tdbus_check_number(arg, *format))
            RETURN_ERROR(NULL);
        if (sizeof(long) == 8)
            value.i64 = PyInt_AsLong(arg);
        else
            value.i64 = PyInt_AsUnsignedLongLongMask(arg);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_UINT64:
        if (!_tdbus_check_number(arg, *format))
            RETURN_ERROR(NULL);
        if (sizeof(long) == 8)
            value.u64 = PyInt_AsUnsignedLongMask(arg);
        else
            value.u64 = PyInt_AsUnsignedLongLongMask(arg);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_DOUBLE:
        value.dbl = PyFloat_AsDouble(arg);
        if (PyErr_Occurred())
            RETURN_ERROR(NULL);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_OBJECT_PATH:
#if PY_MAJOR_VERSION < 3
        if (PyString_Check(arg)) {
            if ((value.str = PyString_AsString(arg)) == NULL)
                RETURN_ERROR(NULL);
        } else
#endif
        if (PyUnicode_Check(arg)) {
            Putf8 = PyUnicode_AsUTF8String(arg);
            CHECK_PYTHON_ERROR(Putf8 == NULL);
            value.str = PyBytes_AsString(Putf8);
            Py_CLEAR(Putf8);
            CHECK_PYTHON_ERROR(value.str == NULL);
        } else
            RETURN_ERROR("expecting str for `%c' format", *format);
        if (!_tdbus_check_path(value.str))
            RETURN_ERROR("invalid object path argument");
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_SIGNATURE:
#if PY_MAJOR_VERSION < 3
        if (PyString_Check(arg)) {
            subtype = strdup(PyString_AsString(arg));
        } else
#endif
        if (PyUnicode_Check(arg)) {
            Putf8 = PyUnicode_AsUTF8String(arg);
            CHECK_PYTHON_ERROR(Putf8 == NULL);
            subtype = strdup(PyBytes_AsString(Putf8));
            Py_CLEAR(Putf8);
        } else
            RETURN_ERROR("expecting str for `%c' format", *format);
        CHECK_MEMORY_ERROR(subtype == NULL);
        if (!_tdbus_check_signature(subtype, 0, 0))
            RETURN_ERROR("invalid signature");
        value.str = subtype;
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        free(subtype); subtype = NULL;
        break;
    case DBUS_TYPE_STRING:
        if (PyUnicode_Check(arg)) {
            Putf8 = PyUnicode_AsUTF8String(arg);
            CHECK_PYTHON_ERROR(Putf8 == NULL);
            value.str = PyBytes_AsString(Putf8);
            Py_CLEAR(Putf8);
        } else if (PyBytes_Check(arg)) {
            value.str = PyBytes_AsString(arg);
        } else
            RETURN_ERROR("expecting str or unicode for '%c' format", *format);
        if (!dbus_message_iter_append_basic(iter, *format, &value))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_STRUCT_BEGIN_CHAR:
        if (!dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT,
                    NULL, &subiter))
            RETURN_MEMORY_ERROR();
        if (!PySequence_Check(arg))
            RETURN_ERROR("expecting sequence argument for struct format");
        if (!_tdbus_message_append_args(&subiter, format+1, arg, depth+1))
            RETURN_ERROR(NULL);
        if (!dbus_message_iter_close_container(iter, &subiter))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_ARRAY:
        if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                    format+1, &subiter))
            RETURN_MEMORY_ERROR();
        if (format[1] == DBUS_TYPE_BYTE) {
            if (!PyBytes_Check(arg))
                RETURN_ERROR("expecting bytes argument for array of byte");
            ptr = PyBytes_AS_STRING(arg);
            size = PyBytes_GET_SIZE(arg);
            if (!dbus_message_iter_append_fixed_array(&subiter, format[1], &ptr, size))
                RETURN_MEMORY_ERROR();
        } else {
            if (format[1] == DBUS_DICT_ENTRY_BEGIN_CHAR) {
                if (!PyDict_Check(arg))
                    RETURN_ERROR("expecting dict argument for array of dict_entry");
                Parray = PyDict_Items(arg);
            } else {
                if (!PySequence_Check(arg))
                    RETURN_ERROR("expecting sequence argument for array format");
                Parray = arg;
            }
            for (i=0; i<PySequence_Size(Parray); i++) {
                Pitem = PySequence_GetItem(Parray, i);
                if (!_tdbus_message_append_arg(&subiter, format+1, Pitem, depth+1))
                    RETURN_ERROR(NULL);
                Py_DECREF(Pitem); Pitem = NULL;
            }
            if (Parray != arg)
                Py_DECREF(Parray);
        }
        if (!dbus_message_iter_close_container(iter, &subiter))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_DICT_ENTRY_BEGIN_CHAR:
        if (!dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY,
                    NULL, &subiter))
            RETURN_MEMORY_ERROR();
        if (!PySequence_Check(arg))
            RETURN_ERROR("expecting sequence argument for dict_entry format");
        if (!_tdbus_message_append_args(&subiter, format+1, arg, depth+1))
            RETURN_ERROR(NULL);
        if (!dbus_message_iter_close_container(iter, &subiter))
            RETURN_MEMORY_ERROR();
        break;
    case DBUS_TYPE_VARIANT:
        if (!PySequence_Check(arg) || PySequence_Size(arg) != 2)
            RETURN_ERROR("expecting a sequence argument of length 2 for variant");
        Ptype = PySequence_GetItem(arg, 0);
        Pvalue = PySequence_GetItem(arg, 1);

        if (PyUnicode_Check(Ptype)) {
            Putf8 = PyUnicode_AsUTF8String(Ptype);
            CHECK_PYTHON_ERROR(Putf8 == NULL);
            subtype = strdup(PyBytes_AsString(Putf8));
            Py_CLEAR(Putf8);
        } else if (PyBytes_Check(Ptype)) {
            subtype = strdup(PyBytes_AsString(Ptype));
        } else
            RETURN_ERROR("first item in sequence argument must be string");

        CHECK_MEMORY_ERROR(subtype == NULL);
        if (!_tdbus_check_signature(subtype, 0, 0))
            RETURN_ERROR("invalid signature for variant");
        end = _tdbus_get_one_full_type(subtype);
        if (end == NULL || *end != '\000')
            RETURN_ERROR("variant signature must be exactly one full type");
        if (!dbus_message_iter_open_container(iter, *format, subtype, &subiter))
            RETURN_MEMORY_ERROR();
        if (!_tdbus_message_append_arg(&subiter, subtype, Pvalue, depth+1))
            RETURN_ERROR(NULL);
        if (!dbus_message_iter_close_container(iter, &subiter))
            RETURN_MEMORY_ERROR();
        Py_DECREF(Ptype); Ptype = NULL;
        Py_DECREF(Pvalue); Pvalue = NULL;
        free(subtype); subtype = NULL;
        break;
    default:
        RETURN_ERROR("unknown format character `%c'", *format);
    }
    return 1;

error:
    if (Pitem != NULL) Py_DECREF(Pitem);
    if (Ptype != NULL) Py_DECREF(Ptype);
    if (Pvalue != NULL) Py_DECREF(Pvalue);
    if (subtype != NULL) free(subtype);
    return 0;
}

static int
_tdbus_message_append_args(DBusMessageIter *iter, char *format,
                           PyObject *args, int depth)
{
    int curarg = 0;
    char *end, store;
    PyObject *Parg = NULL;

    while (*format != '\000') {
        if ((end = _tdbus_get_one_full_type(format)) == NULL)
            RETURN_ERROR(NULL);
        if (curarg == PySequence_Size(args))
            RETURN_ERROR("too few arguments for format string");
        store = *end; *end = '\000';
        Parg = PySequence_GetItem(args, curarg++);
        if (!_tdbus_message_append_arg(iter, format, Parg, depth))
            RETURN_ERROR(NULL);
        Py_DECREF(Parg); Parg = NULL;
        *(format = end) = store;
        if (*format == DBUS_STRUCT_END_CHAR || *format == DBUS_DICT_ENTRY_END_CHAR)
            format++;
    }
    if (curarg != PySequence_Size(args))
        RETURN_ERROR("too many arguments for format string");
    return 1;

error:
    if (Parg != NULL) Py_DECREF(Parg);
    return 0;
}

static PyObject *
tdbus_message_set_args(PyTDBusMessageObject *self, PyObject *args)
{
    char *format, *ptr = NULL;
    DBusMessageIter iter;
    PyObject *Pargs;

    if (self->message == NULL)
        RETURN_ERROR("uninitialized object");
    if (!PyArg_ParseTuple(args, "sO:set_args", &format, &Pargs))
        return NULL;
    if (!PySequence_Check(Pargs))
        RETURN_ERROR("expecting a sequence for the arguments");
    ptr = strdup(format);
    CHECK_MEMORY_ERROR(ptr == NULL);
    if (!_tdbus_check_signature(ptr, 0, 0))
        RETURN_ERROR("illegal signature");

    dbus_message_iter_init_append(self->message, &iter);
    if (!_tdbus_message_append_args(&iter, ptr, Pargs, 0))
        RETURN_ERROR(NULL);

    free(ptr);
    Py_INCREF(Py_None);
    return Py_None;

error:
    if (ptr != NULL) free(ptr);
    return NULL;
}

PyMethodDef tdbus_message_methods[] = \
{
    { "get_type", (PyCFunction) tdbus_message_get_type, METH_VARARGS },
    { "get_no_reply", (PyCFunction) tdbus_message_get_no_reply, METH_VARARGS },
    { "set_no_reply", (PyCFunction) tdbus_message_set_no_reply, METH_VARARGS },
    { "get_auto_start", (PyCFunction) tdbus_message_get_auto_start, METH_VARARGS },
    { "set_auto_start", (PyCFunction) tdbus_message_set_auto_start, METH_VARARGS },
    { "get_serial", (PyCFunction) tdbus_message_get_serial, METH_VARARGS },
    { "get_path", (PyCFunction) tdbus_message_get_path, METH_VARARGS },
    { "set_path", (PyCFunction) tdbus_message_set_path, METH_VARARGS },
    { "get_interface", (PyCFunction) tdbus_message_get_interface, METH_VARARGS },
    { "set_interface", (PyCFunction) tdbus_message_set_interface, METH_VARARGS },
    { "get_member", (PyCFunction) tdbus_message_get_member, METH_VARARGS },
    { "set_member", (PyCFunction) tdbus_message_set_member, METH_VARARGS },
    { "get_error_name", (PyCFunction) tdbus_message_get_error_name, METH_VARARGS },
    { "set_error_name", (PyCFunction) tdbus_message_set_error_name, METH_VARARGS },
    { "get_reply_serial", (PyCFunction) tdbus_message_get_reply_serial, METH_VARARGS },
    { "set_reply_serial", (PyCFunction) tdbus_message_set_reply_serial, METH_VARARGS },
    { "get_destination", (PyCFunction) tdbus_message_get_destination, METH_VARARGS },
    { "set_destination", (PyCFunction) tdbus_message_set_destination, METH_VARARGS },
    { "get_sender", (PyCFunction) tdbus_message_get_sender, METH_VARARGS },
    { "get_signature", (PyCFunction) tdbus_message_get_signature, METH_VARARGS },
    { "get_args", (PyCFunction ) tdbus_message_get_args, METH_VARARGS },
    { "set_args", (PyCFunction ) tdbus_message_set_args, METH_VARARGS },
    { NULL }
};


/*
 * PendingCall object: used for method call callbacks
 */

typedef struct
{
    PyObject_HEAD
    DBusPendingCall *pending_call;
} PyTDBusPendingCallObject;

static PyTypeObject PyTDBusPendingCallType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_tdbus.PendingCall",
    sizeof(PyTDBusPendingCallObject)
};

static void
tdbus_pending_call_dealloc(PyTDBusPendingCallObject *self)
{
    if (self->pending_call) {
        dbus_pending_call_unref(self->pending_call);
        self->pending_call = NULL;
    }
    PyObject_Del(self);
}

static void
_tdbus_pending_call_notify_callback(DBusPendingCall *pending, void *data)
{
    PyTDBusMessageObject *Pmessage;

    if ((Pmessage = PyObject_New(PyTDBusMessageObject, &PyTDBusMessageType)) == NULL)
        return;
    Pmessage->message = dbus_pending_call_steal_reply(pending);
    if (Pmessage->message == NULL)
        return;
    PyObject_CallFunction((PyObject *) data, "O", Pmessage);
    if (PyErr_Occurred())
        PyErr_Clear();
    Py_DECREF(Pmessage);
}

static PyObject *
tdbus_pending_call_set_notify(PyTDBusPendingCallObject *self, PyObject *args)
{
    PyObject *notify;

    if (!PyArg_ParseTuple(args, "O:set_notify", &notify))
        return NULL;
    if (!PyCallable_Check(notify))
        RETURN_ERROR("expecing a Python callable");
    Py_INCREF(notify);
    if (!dbus_pending_call_set_notify(self->pending_call,
                _tdbus_pending_call_notify_callback, notify, _tdbus_decref))
        RETURN_ERROR("dbus_pending_call_set_notify() failed");

    Py_INCREF(Py_None);
    return Py_None;

error:
    return NULL;
}

PyMethodDef tdbus_pending_call_methods[] = \
{
    { "set_notify", (PyCFunction) tdbus_pending_call_set_notify, METH_VARARGS },
    { NULL }
};


/*
 * Connection object
 */

typedef struct
{
    PyObject_HEAD
    DBusConnection *connection;
    PyObject *loop;
} PyTDBusConnectionObject;

PyTypeObject PyTDBusConnectionType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "_tdbus.Connection",
    sizeof(PyTDBusConnectionObject)
};

static DBusConnection *
_tdbus_connection_open(const char *address)
{
    int ret;
    DBusError error;
    DBusConnection *connection = NULL;

    dbus_error_init(&error);
    if (!strcmp(address, "<SYSTEM>")) {
        Py_BEGIN_ALLOW_THREADS
        connection = dbus_bus_get_private(DBUS_BUS_SYSTEM, &error);
        Py_END_ALLOW_THREADS
        if (connection == NULL)
            RETURN_DBUS_ERROR(error);
    } else if (!strcmp(address, "<SESSION>")) {
        Py_BEGIN_ALLOW_THREADS
        connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
        Py_END_ALLOW_THREADS
        if (connection == NULL)
            RETURN_DBUS_ERROR(error);
    } else if (!strcmp(address, "<STARTER>")) {
		Py_BEGIN_ALLOW_THREADS
		connection = dbus_bus_get_private(DBUS_BUS_STARTER, &error);
		Py_END_ALLOW_THREADS
		if (connection == NULL)
			RETURN_DBUS_ERROR(error);
    } else {
        Py_BEGIN_ALLOW_THREADS
        connection = dbus_connection_open_private(address, &error);
        Py_END_ALLOW_THREADS
        if (connection == NULL)
            RETURN_DBUS_ERROR(error);
        Py_BEGIN_ALLOW_THREADS
        ret = dbus_bus_register(connection, &error);
        Py_END_ALLOW_THREADS
        if (ret == 0)
            RETURN_DBUS_ERROR(error);
    }

    dbus_connection_set_exit_on_disconnect(connection, FALSE);

    return connection;

error:
    if (connection != NULL) {
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
    }
    return NULL;
}

static int
tdbus_connection_init(PyTDBusConnectionObject *self, PyObject *args,
                      PyObject *kwargs)
{
    char *address = NULL;
    static char *kwlist[] = { "address", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &address))
        return -1;

    if (address != NULL) {
        self->connection = _tdbus_connection_open(address);
        if (self->connection == NULL)
            return -1;
        if (!dbus_connection_set_data(self->connection, GETSTATE()->app_slot, self, NULL))
            return -1;
    }
    return 0;
}

static void
tdbus_connection_dealloc(PyTDBusConnectionObject *self)
{
    if (self->connection) {
        dbus_connection_close(self->connection);
        dbus_connection_unref(self->connection);
        self->connection = NULL;
    }
    if (self->loop) {
        Py_DECREF(self->loop);
        self->loop = NULL;
    }
    PyObject_Del(self);
}

static PyObject *
tdbus_connection_open(PyTDBusConnectionObject *self, PyObject *args)
{
    const char *address;

    if (!PyArg_ParseTuple(args, "s:open", &address))
        return NULL;

    self->connection = _tdbus_connection_open(address);
    if (self->connection == NULL)
        RETURN_ERROR(NULL);
    if (!dbus_connection_set_data(self->connection, GETSTATE()->app_slot, self, NULL))
        RETURN_ERROR("dbus_connection_set_data() failed");

    Py_INCREF(Py_None);
    return Py_None;

error:
    return NULL;
}

static PyObject *
tdbus_connection_close(PyTDBusConnectionObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":close"))
        return NULL;

    if (self->connection != NULL) {
        dbus_connection_close(self->connection);
        dbus_connection_unref(self->connection);
        self->connection = NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
tdbus_connection_get_loop(PyTDBusConnectionObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_loop"))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    if (self->loop == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    } else {
        Py_INCREF(self->loop);
        return self->loop;
    }

error:
    return NULL;
}

static dbus_bool_t
_tdbus_add_watch_callback(DBusWatch *watch, void *data)
{
    PyTDBusWatchObject *Pwatch;

    if ((Pwatch = dbus_watch_get_data(watch)) == NULL) {
        if ((Pwatch = PyObject_New(PyTDBusWatchObject, &PyTDBusWatchType)) == NULL)
            return FALSE;
        Pwatch->watch = watch;
        Pwatch->data = NULL;
        Py_INCREF(Pwatch);
        dbus_watch_set_data(watch, Pwatch, _tdbus_decref);
    }
    PyObject_CallMethod((PyObject *) data, "add_watch", "O", Pwatch);
    if (PyErr_Occurred())
        PyErr_Clear();
    return TRUE;
}

static void
_tdbus_remove_watch_callback(DBusWatch *watch, void *data)
{
    PyObject *Pwatch;

    Pwatch = dbus_watch_get_data(watch);
    ASSERT(Pwatch != NULL);
    PyObject_CallMethod((PyObject *) data, "remove_watch", "O", Pwatch);
    if (PyErr_Occurred())
        PyErr_Clear();
error:
    return;
}

static void
_tdbus_watch_toggled_callback(DBusWatch *watch, void *data)
{
    PyObject *Pwatch;

    Pwatch = dbus_watch_get_data(watch);
    ASSERT(Pwatch != NULL);
    PyObject_CallMethod((PyObject *) data, "watch_toggled", "O", Pwatch);
    if (PyErr_Occurred())
        PyErr_Clear();
error:
    return;
}

static dbus_bool_t
_tdbus_add_timeout_callback(DBusTimeout *timeout, void *data)
{
    PyTDBusTimeoutObject *Ptimeout;

    if ((Ptimeout = dbus_timeout_get_data(timeout)) == NULL) {
        if ((Ptimeout = PyObject_New(PyTDBusTimeoutObject, &PyTDBusTimeoutType)) == NULL)
            return FALSE;
        Ptimeout->timeout = timeout;
        Ptimeout->data = NULL;
        Py_INCREF(Ptimeout);
        dbus_timeout_set_data(timeout, Ptimeout, _tdbus_decref);
    }
    PyObject_CallMethod((PyObject *) data, "add_timeout", "O", Ptimeout);
    if (PyErr_Occurred())
        PyErr_Clear();
    return TRUE;
}

static void
_tdbus_remove_timeout_callback(DBusTimeout *timeout, void *data)
{
    PyObject *Ptimeout;

    Ptimeout = dbus_timeout_get_data(timeout);
    ASSERT(Ptimeout != NULL);
    PyObject_CallMethod((PyObject *) data, "remove_timeout", "O", Ptimeout);
    if (PyErr_Occurred())
        PyErr_Clear();
error:
    return;
}

static void
_tdbus_timeout_toggled_callback(DBusTimeout *timeout, void *data)
{
    PyObject *Ptimeout;

    Ptimeout = dbus_timeout_get_data(timeout);
    ASSERT(Ptimeout != NULL);
    PyObject_CallMethod((PyObject *) data, "timeout_toggled", "O", Ptimeout);
    if (PyErr_Occurred())
        PyErr_Clear();
error:
    return;
}

static PyObject *
tdbus_connection_set_loop(PyTDBusConnectionObject *self, PyObject *args)
{
    PyObject *loop;
    
    if (!PyArg_ParseTuple(args, "O:set_loop", &loop))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    if (!PyObject_HasAttrString(loop, "add_watch") ||
                !PyObject_HasAttrString(loop, "remove_watch") ||
                !PyObject_HasAttrString(loop, "watch_toggled") ||
                !PyObject_HasAttrString(loop, "add_timeout") ||
                !PyObject_HasAttrString(loop, "remove_timeout") ||
                !PyObject_HasAttrString(loop, "timeout_toggled"))
        RETURN_ERROR("expecting an EventLoop like object");

    Py_INCREF(loop);
    self->loop = loop;

    Py_INCREF(loop);
    if (!dbus_connection_set_watch_functions(self->connection,
            _tdbus_add_watch_callback, _tdbus_remove_watch_callback,
            _tdbus_watch_toggled_callback, loop, _tdbus_decref))
        RETURN_ERROR("dbus_connection_set_watch_functions() failed");

    Py_INCREF(loop);
    if (!dbus_connection_set_timeout_functions(self->connection,
            _tdbus_add_timeout_callback, _tdbus_remove_timeout_callback,
            _tdbus_timeout_toggled_callback, loop, _tdbus_decref))
        RETURN_ERROR("dbus_connection_set_watch_functions() failed");

    Py_INCREF(Py_None);
    return Py_None;

error:
    return NULL;
}

static DBusHandlerResult
_tdbus_connection_filter_callback(DBusConnection *connection,
                                  DBusMessage *message, void *data)
{
    int ret;
    PyObject *Presult;
    PyTDBusMessageObject *Pmessage;

    if ((Pmessage = PyObject_New(PyTDBusMessageObject, &PyTDBusMessageType)) == NULL)
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    dbus_message_ref(message);
    Pmessage->message = message;

    Presult = PyObject_CallFunction((PyObject *) data, "O", Pmessage);
    Py_DECREF(Pmessage);
    if (Presult == NULL) {
        PyErr_Clear();
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    if (PyObject_IsTrue(Presult))
        ret = DBUS_HANDLER_RESULT_HANDLED;
    else
        ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    Py_DECREF(Presult);
    return ret;
}

static PyObject *
tdbus_connection_add_filter(PyTDBusConnectionObject *self, PyObject *args)
{
    PyObject *filter;

    if (!PyArg_ParseTuple(args, "O:add_filter", &filter))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    if (!PyCallable_Check(filter))
        RETURN_ERROR("expecting a Python callable");
    Py_INCREF(filter);
    if (!dbus_connection_add_filter(self->connection,
                _tdbus_connection_filter_callback, filter, _tdbus_decref))
        RETURN_ERROR("dbus_connection_add_filter() failed");
    Py_INCREF(Py_None);
    return Py_None;

error:
    return NULL;
}

static PyObject *
tdbus_connection_send(PyTDBusConnectionObject *self, PyObject *args)
{
    dbus_uint32_t serial;
    PyObject *Pserial;
    PyTDBusMessageObject *message;

    if (!PyArg_ParseTuple(args, "O!:send", &PyTDBusMessageType, &message))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    if (!dbus_connection_send(self->connection, message->message, &serial))
        RETURN_ERROR("dbus_connection_send() failed");
    
    if (sizeof(long) == 8)
        Pserial = PyInt_FromLong(serial);
    else
        Pserial = PyLong_FromUnsignedLong(serial);
    CHECK_PYTHON_ERROR(Pserial == NULL);
    return Pserial;

error:
    return NULL;
}

static PyObject *
tdbus_connection_send_with_reply(PyTDBusConnectionObject *self, PyObject *args)
{
    int timeout = -1;
    PyTDBusPendingCallObject *Ppending;
    PyTDBusMessageObject *message;
    DBusPendingCall *pending = NULL;

    if (!PyArg_ParseTuple(args, "O!|i:send", &PyTDBusMessageType, &message,
                          &timeout))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    if (!dbus_connection_send_with_reply(self->connection, message->message,
                &pending, timeout) || (pending == NULL))
        RETURN_ERROR("dbus_connection_send_with_reply() failed");

    Ppending = PyObject_New(PyTDBusPendingCallObject, &PyTDBusPendingCallType);
    CHECK_PYTHON_ERROR(Ppending == NULL);
    Ppending->pending_call = pending;
    return (PyObject *) Ppending;

error:
    if (pending != NULL) dbus_pending_call_unref(pending);
    return NULL;
}

static PyObject *
tdbus_connection_dispatch(PyTDBusConnectionObject *self, PyObject *args)
{
    int status;
    PyObject *Pstatus;

    if (!PyArg_ParseTuple(args, ":dispatch"))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    status = dbus_connection_dispatch(self->connection);
    Pstatus = PyInt_FromLong(status);
    CHECK_PYTHON_ERROR(Pstatus == NULL);
    return Pstatus;

error:
    return NULL;
}

static PyObject *
tdbus_connection_flush(PyTDBusConnectionObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":flush"))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    Py_BEGIN_ALLOW_THREADS
    dbus_connection_flush(self->connection);
    Py_END_ALLOW_THREADS

    Py_INCREF(Py_None);
    return Py_None;

error:
    return NULL;
}

static PyObject *
tdbus_connection_get_unique_name(PyTDBusConnectionObject *self, PyObject *args)
{
    const char *name;
    PyObject *Paddress;

    if (!PyArg_ParseTuple(args, ":get_unique_name"))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    if ((name = dbus_bus_get_unique_name(self->connection)) == NULL)
        RETURN_ERROR("dbus_bus_get_unique_name() failed");
    if ((Paddress = PyUnicode_FromString(name)) == NULL)
        RETURN_ERROR(NULL);
    return Paddress;

error:
    return NULL;
}

static PyObject *
tdbus_connection_get_dispatch_status(PyTDBusConnectionObject *self, PyObject *args)
{
    int status;
    PyObject *Pstatus;

    if (!PyArg_ParseTuple(args, ":get_dispatch_status"))
        return NULL;
    if (self->connection == NULL)
        RETURN_ERROR("not connected");

    status = dbus_connection_get_dispatch_status(self->connection);
    Pstatus = PyInt_FromLong(status);
    CHECK_PYTHON_ERROR(Pstatus == NULL);
    return Pstatus;

error:
    return NULL;
}

static PyMethodDef tdbus_connection_methods[] = \
{
    { "open", (PyCFunction) tdbus_connection_open, METH_VARARGS },
    { "close", (PyCFunction) tdbus_connection_close, METH_VARARGS },
    { "get_loop", (PyCFunction) tdbus_connection_get_loop, METH_VARARGS },
    { "set_loop", (PyCFunction) tdbus_connection_set_loop, METH_VARARGS },
    { "add_filter", (PyCFunction) tdbus_connection_add_filter, METH_VARARGS },
    { "send", (PyCFunction) tdbus_connection_send, METH_VARARGS },
    { "send_with_reply", (PyCFunction) tdbus_connection_send_with_reply, METH_VARARGS },
    { "dispatch", (PyCFunction) tdbus_connection_dispatch, METH_VARARGS },
    { "flush", (PyCFunction) tdbus_connection_flush, METH_VARARGS },
    { "get_unique_name", (PyCFunction) tdbus_connection_get_unique_name, METH_VARARGS },
    { "get_dispatch_status", (PyCFunction) tdbus_connection_get_dispatch_status, METH_VARARGS },
    { NULL }
};


/*
 * _tdbus module
 */

static PyMethodDef tdbus_methods[] = {
    { NULL }
};

#if PY_MAJOR_VERSION >= 3

static int tdbus_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETMSTATE(m)->error);
    return 0;
}

static int tdbus_clear(PyObject *m) {
    Py_CLEAR(GETMSTATE(m)->error);
    return 0;
}


static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_tdbus",
        NULL,
        sizeof(struct module_state),
        tdbus_methods,
        NULL,
        tdbus_traverse,
        tdbus_clear,
        NULL
};

#define INITERROR { Py_XDECREF(Pmodule); return NULL; }

PyObject * PyInit__tdbus(void) {

#else
#define INITERROR { Py_XDECREF(Pmodule); return; }

void init_tdbus(void) {
#endif
    PyObject *Pmodule, *Pdict, *Pint, *Pstr;

#if PY_MAJOR_VERSION >= 3
    if ((Pmodule = PyModule_Create(&moduledef)) == NULL)
#else
    if ((Pmodule = Py_InitModule("_tdbus", tdbus_methods)) == NULL)
#endif
        INITERROR;

    struct module_state *state = GETMSTATE(Pmodule);
    state->error = NULL;
    state->app_slot = -1;

    if ((Pdict = PyModule_GetDict(Pmodule)) == NULL)
        INITERROR;
    if ((state->error = PyErr_NewException("_tdbus.Error", NULL, NULL)) == NULL)
        INITERROR;
    if (PyDict_SetItemString(Pdict, "Error", state->error) == -1)
        INITERROR;

    #define FINALIZE_TYPE(type, name, methods, init, dealloc) \
        do { \
            type.tp_new = PyType_GenericNew; \
            type.tp_init = (initproc) init; \
            type.tp_dealloc = (destructor) dealloc; \
            type.tp_flags = Py_TPFLAGS_DEFAULT; \
            type.tp_doc = "D-BUS " name " Object"; \
            type.tp_methods = methods; \
            if (PyType_Ready(&type) < 0) INITERROR; \
            if (PyDict_SetItemString(Pdict, name, (PyObject *) &type) < 0) INITERROR; \
        } while (0)

    FINALIZE_TYPE(PyTDBusWatchType, "Watch", tdbus_watch_methods,
                  NULL, tdbus_watch_dealloc);
    FINALIZE_TYPE(PyTDBusTimeoutType, "Timeout", tdbus_timeout_methods,
                  NULL, tdbus_timeout_dealloc);
    FINALIZE_TYPE(PyTDBusMessageType, "Message", tdbus_message_methods,
                  tdbus_message_init, tdbus_message_dealloc);
    FINALIZE_TYPE(PyTDBusPendingCallType, "PendingCall", tdbus_pending_call_methods,
                  NULL, tdbus_pending_call_dealloc);
    FINALIZE_TYPE(PyTDBusConnectionType, "Connection", tdbus_connection_methods,
                  tdbus_connection_init, tdbus_connection_dealloc);

    #define EXPORT_STRING(name, value) \
        do { \
            if ((Pstr = PyUnicode_FromString(value)) == NULL) INITERROR; \
            PyDict_SetItemString(Pdict, #name, Pstr); \
            Py_DECREF(Pstr); \
        } while (0)

    EXPORT_STRING(DBUS_BUS_SYSTEM, "<SYSTEM>");
    EXPORT_STRING(DBUS_BUS_SESSION, "<SESSION>");
    EXPORT_STRING(DBUS_BUS_STARTER, "<STARTER>");

	#define EXPORT_INT_SYMBOL(name) \
		do { \
			if ((Pint = PyInt_FromLong(name)) == NULL) INITERROR; \
			PyDict_SetItemString(Pdict, #name, Pint); \
			Py_DECREF(Pint); \
		} while (0)

    #define EXPORT_STRING_SYMBOL(name) \
        do { \
            if ((Pstr = PyUnicode_FromString(name)) == NULL) INITERROR; \
            PyDict_SetItemString(Pdict, #name, Pstr); \
            Py_DECREF(Pstr); \
        } while (0)

    EXPORT_INT_SYMBOL(DBUS_MAJOR_PROTOCOL_VERSION);
    EXPORT_INT_SYMBOL(DBUS_MESSAGE_TYPE_INVALID);
    EXPORT_INT_SYMBOL(DBUS_MESSAGE_TYPE_METHOD_CALL);
    EXPORT_INT_SYMBOL(DBUS_MESSAGE_TYPE_METHOD_RETURN);
    EXPORT_INT_SYMBOL(DBUS_MESSAGE_TYPE_ERROR);
    EXPORT_INT_SYMBOL(DBUS_MESSAGE_TYPE_SIGNAL);
    EXPORT_INT_SYMBOL(DBUS_HEADER_FLAG_NO_REPLY_EXPECTED);
    EXPORT_INT_SYMBOL(DBUS_HEADER_FLAG_NO_AUTO_START);
    EXPORT_INT_SYMBOL(DBUS_MAXIMUM_NAME_LENGTH);
    EXPORT_INT_SYMBOL(DBUS_WATCH_READABLE);
    EXPORT_INT_SYMBOL(DBUS_WATCH_WRITABLE);
    EXPORT_INT_SYMBOL(DBUS_DISPATCH_DATA_REMAINS);
    EXPORT_INT_SYMBOL(DBUS_DISPATCH_COMPLETE);
    EXPORT_INT_SYMBOL(DBUS_DISPATCH_NEED_MEMORY);

    //dbus-shared.h constants
    EXPORT_STRING_SYMBOL(DBUS_SERVICE_DBUS);
    EXPORT_STRING_SYMBOL(DBUS_PATH_DBUS);
    EXPORT_STRING_SYMBOL(DBUS_PATH_LOCAL);

    EXPORT_STRING_SYMBOL(DBUS_INTERFACE_DBUS);
    EXPORT_STRING_SYMBOL(DBUS_INTERFACE_INTROSPECTABLE);
    EXPORT_STRING_SYMBOL(DBUS_INTERFACE_PROPERTIES);
    EXPORT_STRING_SYMBOL(DBUS_INTERFACE_PEER);
    EXPORT_STRING_SYMBOL(DBUS_INTERFACE_LOCAL);

    EXPORT_INT_SYMBOL(DBUS_NAME_FLAG_ALLOW_REPLACEMENT);
    EXPORT_INT_SYMBOL(DBUS_NAME_FLAG_REPLACE_EXISTING);
    EXPORT_INT_SYMBOL(DBUS_NAME_FLAG_DO_NOT_QUEUE);

    EXPORT_INT_SYMBOL(DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);
    EXPORT_INT_SYMBOL(DBUS_REQUEST_NAME_REPLY_IN_QUEUE);
    EXPORT_INT_SYMBOL(DBUS_REQUEST_NAME_REPLY_EXISTS);
    EXPORT_INT_SYMBOL(DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER);
    EXPORT_INT_SYMBOL(DBUS_RELEASE_NAME_REPLY_RELEASED);
    EXPORT_INT_SYMBOL(DBUS_RELEASE_NAME_REPLY_NON_EXISTENT );
    EXPORT_INT_SYMBOL(DBUS_RELEASE_NAME_REPLY_NOT_OWNER);\

    EXPORT_INT_SYMBOL(DBUS_START_REPLY_SUCCESS);
    EXPORT_INT_SYMBOL(DBUS_START_REPLY_ALREADY_RUNNING);


    #define EXPORT_STR_SYMBOL(name) \
        do { \
            if ((Pstr = PyUnicode_FromString(name)) == NULL) INITERROR; \
            PyDict_SetItemString(Pdict, #name, Pstr); \
            Py_DECREF(Pstr); \
        } while (0)

    EXPORT_STR_SYMBOL(DBUS_SERVICE_DBUS);
    EXPORT_STR_SYMBOL(DBUS_PATH_DBUS);
    EXPORT_STR_SYMBOL(DBUS_INTERFACE_DBUS);

    if (!_tdbus_init_check_number_cache())
        INITERROR;

    /* NOTE: dbus_threads_init_default() should better use the same thread
     * implementation that Python uses! At least on Linux, Windows and OSX
     * that seems to be the case.
     *
     * A solution that works for platforms would be to install thread
     * functions that call into Python's threading module. But that would be
     * quite inefficient. */

    if (!dbus_threads_init_default())
        INITERROR;

    if (!dbus_connection_allocate_data_slot(&state->app_slot))
        INITERROR;

#if PY_MAJOR_VERSION >= 3
        return Pmodule;
#endif
}

/*
 * vi: et sts=4 sw=4 et */
