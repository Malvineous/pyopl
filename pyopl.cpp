/*
 * pyopl.cpp - Main OPL wrapper.
 *
 * Copyright (C) 2011-2012 Adam Nielsen <malvineous@shikadi.net>
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

// Size of each sample in bytes (2 == 16-bit)
#define SAMPLE_SIZE 2

// Volume amplication (0 == none, 1 == 2x, 2 == 4x)
#define VOL_AMP 1

class SampleHandler: public MixerChannel {
	public:
		Py_buffer pybuf;
		uint8_t channels;

		SampleHandler(uint8_t channels)
			: channels(channels)
		{
		}

		virtual ~SampleHandler()
		{
		}

		virtual void AddSamples_m32(Bitu samples, Bit32s *buffer)
		{
			// Convert samples from mono s32 to stereo s16
			int16_t *out = (int16_t *)this->pybuf.buf;
			for (unsigned int i = 0; i < samples; i++) {
				*out++ = buffer[i] << VOL_AMP;
				if (channels == 2) *out++ = buffer[i] << VOL_AMP;
			}
			return;
		}

		virtual void AddSamples_s32(Bitu samples, Bit32s *buffer)
		{
			// Convert samples from stereo s32 to stereo s16
			int16_t *out = (int16_t *)this->pybuf.buf;
			for (unsigned int i = 0; i < samples; i++) {
				*out++ = buffer[i*2] << VOL_AMP;
				if (channels == 2) *out++ = buffer[i*2+1] << VOL_AMP;
			}
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

	int reg, val;
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "ii", (char **)kwlist, &reg, &val)) return NULL;

	o->opl->WriteReg(reg, val);

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *opl_getSamples(PyObject *self, PyObject *args)
{
	PyOPL *o = (PyOPL *)self;

	if (!PyArg_ParseTuple(args, "w*", &o->sh->pybuf)) return NULL;

	int samples = o->sh->pybuf.len / SAMPLE_SIZE / o->sh->channels;
	if (samples > 512) {
		PyErr_SetString(PyExc_ValueError, "buffer too large (must be 512 samples)");
		return NULL;
	}
	if (samples < 512) {
		PyErr_SetString(PyExc_ValueError, "buffer too small (must be 512 samples)");
		return NULL;
	}

	o->opl->Generate(o->sh, samples);

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
	static const char *kwlist[] = {"freq", "sampleSize", "channels", NULL};

	unsigned int freq;
	uint8_t sampleSize;
	uint8_t channels;
	if (!PyArg_ParseTupleAndKeywords(args, keywds, "Ibb", (char **)kwlist, &freq, &sampleSize, &channels)) return NULL;
	if (sampleSize != SAMPLE_SIZE) {
		PyErr_SetString(PyExc_ValueError, "invalid sample size (valid values: 2=16-bit)");
		return NULL;
	}
	if ((channels != 1) && (channels != 2)) {
		PyErr_SetString(PyExc_ValueError, "invalid channel count (valid values: 1=mono, 2=stereo)");
		return NULL;
	}

	PyOPL *o = PyObject_New(PyOPL, &PyOPLType);
	if (o) {
		o->sh = new SampleHandler(channels);
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
