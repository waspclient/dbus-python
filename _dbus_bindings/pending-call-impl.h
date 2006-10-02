/* Implementation of PendingCall helper type for D-Bus bindings.
 *
 * Copyright (C) 2006 Collabora Ltd.
 *
 * Licensed under the Academic Free License version 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

PyDoc_STRVAR(PendingCall_tp_doc,
"Object representing a pending D-Bus call, returned by\n"
"Connection._send_with_reply(). Cannot be instantiated directly.\n"
);

static PyTypeObject PendingCallType;

static inline int PendingCall_Check (PyObject *o)
{
    return (o->ob_type == &PendingCallType)
            || PyObject_IsInstance(o, (PyObject *)&PendingCallType);
}

typedef struct {
    PyObject_HEAD
    DBusPendingCall *pc;
} PendingCall;

PyDoc_STRVAR(PendingCall_cancel__doc__,
"cancel()\n\n"
"Cancel this pending call. Its reply will be ignored and the associated\n"
"reply handler will never be called.\n");
static PyObject *
PendingCall_cancel(PendingCall *self, PyObject *unused)
{
    Py_BEGIN_ALLOW_THREADS
    dbus_pending_call_cancel(self->pc);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

PyDoc_STRVAR(PendingCall__block__doc__,
"_block()\n\n"
"Block until this pending call has completed and the associated\n"
"reply handler has been called.\n"
"\n"
"This can lead to a deadlock, if the called method tries to make a\n"
"synchronous call to a method in this application. As a result, it's\n"
"probably a bad idea.\n");
static PyObject *
PendingCall__block(PendingCall *self, PyObject *unused)
{
    Py_BEGIN_ALLOW_THREADS
    dbus_pending_call_block(self->pc);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static void 
_pending_call_notify_function(DBusPendingCall *pc,
                              PyObject *handler)
{
    PyGILState_STATE gil = PyGILState_Ensure();
    DBusMessage *msg = dbus_pending_call_steal_reply(pc);

    if (!msg) {
        /* omg, what happened here? the notify should only get called
         * when we have a reply */
        PyErr_Warn(PyExc_UserWarning, "D-Bus notify function was called "
                   "for an incomplete pending call (shouldn't happen)");
    } else {
        Message *msg_obj = (Message *)Message_ConsumeDBusMessage(msg);

        if (msg_obj) {
            PyObject *ret = PyObject_CallFunctionObjArgs(handler, msg_obj, NULL);

            Py_XDECREF(ret);
        }
        /* else OOM has happened - not a lot we can do about that,
         * except possibly making it fatal (FIXME?) */
    }

    PyGILState_Release(gil);
}

PyDoc_STRVAR(PendingCall_get_completed__doc__,
"get_completed() -> bool\n\n"
"Return true if this pending call has completed.\n\n"
"If so, its associated reply handler has been called and it is no\n"
"longer meaningful to cancel it.\n");
static PyObject *
PendingCall_get_completed(PendingCall *self, PyObject *unused)
{
    dbus_bool_t ret;

    Py_BEGIN_ALLOW_THREADS
    ret = dbus_pending_call_get_completed(self->pc);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(ret);
}

/* Steals the reference to the pending call. */
static PyObject *
PendingCall_ConsumeDBusPendingCall (DBusPendingCall *pc, PyObject *callable)
{
    dbus_bool_t ret;
    PendingCall *self = PyObject_New(PendingCall, &PendingCallType);

    if (!self) {
        Py_BEGIN_ALLOW_THREADS
        dbus_pending_call_cancel(pc);
        dbus_pending_call_unref(pc);
        Py_END_ALLOW_THREADS
        return NULL;
    }

    Py_INCREF(callable);

    Py_BEGIN_ALLOW_THREADS
    ret = dbus_pending_call_set_notify(pc,
        (DBusPendingCallNotifyFunction)_pending_call_notify_function,
        (void *)callable, (DBusFreeFunction)Glue_TakeGILAndXDecref);
    Py_END_ALLOW_THREADS

    if (!ret) {
        PyErr_NoMemory();
        Py_DECREF(callable);
        Py_DECREF(self);
        Py_BEGIN_ALLOW_THREADS
        dbus_pending_call_cancel(pc);
        dbus_pending_call_unref(pc);
        Py_END_ALLOW_THREADS
        return NULL;
    }
    self->pc = pc;
    return (PyObject *)self;
}

static void
PendingCall_tp_dealloc (PendingCall *self)
{
    if (self->pc) {
        Py_BEGIN_ALLOW_THREADS
        dbus_pending_call_unref(self->pc);
        Py_END_ALLOW_THREADS
    }
    PyObject_Del (self);
}

static PyMethodDef PendingCall_tp_methods[] = {
    {"_block", (PyCFunction)PendingCall__block, METH_NOARGS,
     PendingCall__block__doc__},
    {"cancel", (PyCFunction)PendingCall_cancel, METH_NOARGS,
     PendingCall_cancel__doc__},
    {"get_completed", (PyCFunction)PendingCall_get_completed, METH_NOARGS,
     PendingCall_get_completed__doc__},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject PendingCallType = {
    PyObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type))
    0,
    "_dbus_bindings.PendingCall",
    sizeof(PendingCall),
    0,
    (destructor)PendingCall_tp_dealloc,     /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    0,                                      /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    0,                                      /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                     /* tp_flags */
    PendingCall_tp_doc,                     /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    0,                                      /* tp_iter */
    0,                                      /* tp_iternext */
    PendingCall_tp_methods,                 /* tp_methods */
    0,                                      /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    0,                                      /* tp_init */
    0,                                      /* tp_alloc */
    /* deliberately not callable! Use PendingCall_ConsumeDBusPendingCall */
    0,                                      /* tp_new */
};

static inline int
init_pending_call (void)
{
    if (PyType_Ready (&PendingCallType) < 0) return 0;
    return 1;
}

static inline int
insert_pending_call (PyObject *this_module)
{
    if (PyModule_AddObject (this_module, "PendingCall",
                            (PyObject *)&PendingCallType) < 0) return 0;
    return 1;
}

/* vim:set ft=c cino< sw=4 sts=4 et: */
