/*
 * pyopl.cpp - Main OPL wrapper.
 *
 * Copyright (C) 2011 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <Python.h>
#include <cassert>
#include "dbopl.h"

class SampleHandler: public MixerChannel {
	public:
		Py_buffer pybuf;

		virtual void AddSamples_m32(Bitu samples, Bit32s *buffer)
		{
			// Convert samples from mono s32 to s16
			int16_t *out = (int16_t *)this->pybuf.buf;
			for (int i = 0; i < samples; i++) {
				*out++ = buffer[i];
			}
			return;
		}

		virtual void AddSamples_s32(Bitu samples, Bit32s *buffer)
		{
			// Convert samples from stereo s32 to s16
			printf("TODO: Stereo samples are not yet implemented!\n");
			return;
		}
};

struct PyOPL {
	// Can't put any objects in here (only pointers) as this struct is allocated
	// with malloc() instead of operator new (so constructors don't get called.)
	PyObject_HEAD
	SampleHandler *sh;
	DBOPL::Handler *opl;
};

PyObject *opl_writeReg(PyObject *self, PyObject *args, PyObject *keywds)
{
	PyOPL *o = (PyOPL *)self;
	static const char *kwlist[] = {"reg", "val", NULL};

	uint8_t reg, val;
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "bb", (char **)kwlist, &reg, &val)) return NULL;

	o->opl->WriteReg(reg, val);

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *opl_getSamples(PyObject *self, PyObject *args)
{
	PyOPL *o = (PyOPL *)self;

	if (!PyArg_ParseTuple(args, "w*", &o->sh->pybuf)) return NULL;

	if (o->sh->pybuf.len > 512*2) {
		PyErr_SetString(PyExc_ValueError, "buffer too large");
		return NULL;
	}

	o->opl->Generate(o->sh, o->sh->pybuf.len / 2);

	PyBuffer_Release(&o->sh->pybuf); // won't use it any more

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef opl_methods[] = {
	{"writeReg",   (PyCFunction)opl_writeReg, METH_VARARGS | METH_KEYWORDS, "writeReg(reg=, val=): Write a value to an OPL register."},
	{"getSamples", (PyCFunction)opl_getSamples, METH_VARARGS, "getSamples(buffer): Fill the supplied buffer with audio samples."},
	{NULL, NULL, 0, NULL}
};

void opl_dealloc(PyObject *self)
{
	PyOPL *o = (PyOPL *)self;
	delete o->opl;
	delete o->sh;
	PyObject_Del(self);
	return;
}

PyObject *opl_getattr(PyObject *self, char *attrname)
{
	return Py_FindMethod(opl_methods, self, attrname);
}

int opl_setattr(PyObject *self, char *attrname, PyObject *value)
{
	PyErr_SetString(PyExc_AttributeError, attrname);
	return -1;
}

PyObject *opl_repr(PyObject *self)
{
	return PyString_FromString("<OPL>");
}

static PyTypeObject PyOPLType = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,              // ob_size
	"pyopl",        // tp_name
	sizeof(PyOPL),  // tp_basicsize
	0,              // tp_itemsize

	opl_dealloc,    // tp_dealloc
	0,              // tp_print
	opl_getattr,    // tp_getattr
	opl_setattr,    // tp_setattr
	0,              // tp_compare
	opl_repr,       // tp_repr
	0,              // tp_as_number
	0,              // tp_as_sequence
	0,              // tp_as_mapping
	0,              // tp_hash
	0,              // tp_call
	0,              // tp_str
	0,              // tp_getattro
	0,              // tp_setattro
	0,              // tp_as_buffer
	Py_TPFLAGS_DEFAULT, // tp_flags
};

PyObject *opl_new(PyObject *self, PyObject *args, PyObject *keywds)
{
	static const char *kwlist[] = {"freq", "sampleSize", NULL};

	unsigned int freq;
	uint8_t sampleSize;
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "Ib", (char **)kwlist, &freq, &sampleSize)) return NULL;
	if (sampleSize != 2) {
		PyErr_SetString(PyExc_ValueError, "invalid sample size");
		return NULL;
	}

	PyOPL *o = PyObject_New(PyOPL, &PyOPLType);
	if (o) {
		o->sh = new SampleHandler();
		o->opl = new DBOPL::Handler();
		o->opl->Init(freq);
	}
	return (PyObject *)o;
}

static PyMethodDef methods[] = {
	{"opl", (PyCFunction)opl_new, METH_VARARGS | METH_KEYWORDS, "Create a new OPL emulator instance."},
	{NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initpyopl(void)
{
    (void) Py_InitModule("pyopl", methods);
}
