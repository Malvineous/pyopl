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
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <cassert>
#include "dbopl.h"

#define PyString_FromString PyUnicode_FromString
#define ERROR_INIT NULL

// Size of each sample in bytes (2 == 16-bit)
#define SAMPLE_SIZE 2

// Volume amplication (0 == none, 1 == 2x, 2 == 4x)
#define VOL_AMP 1

// Clipping function to prevent integer wraparound after amplification
#define SAMP_BITS (SAMPLE_SIZE << 3)
#define SAMP_MAX ((1 << (SAMP_BITS-1)) - 1)
#define SAMP_MIN -((1 << (SAMP_BITS-1)))
#define CLIP(v) (((v) > SAMP_MAX) ? SAMP_MAX : (((v) < SAMP_MIN) ? SAMP_MIN : (v)))

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
				Bit32s v = buffer[i] << VOL_AMP;
				*out++ = CLIP(v);
				if (channels == 2) *out++ = CLIP(v);
			}
			return;
		}

		virtual void AddSamples_s32(Bitu samples, Bit32s *buffer)
		{
			// Convert samples from stereo s32 to stereo s16
			int16_t *out = (int16_t *)this->pybuf.buf;
			for (unsigned int i = 0; i < samples; i++) {
				Bit32s v = buffer[i*2] << VOL_AMP;
				*out++ = CLIP(v);
				if (channels == 2) {
					v = buffer[i*2+1] << VOL_AMP;
					*out++ = CLIP(v);
				}
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

	Py_RETURN_NONE;
}

PyObject *opl_getSamples(PyObject *self, PyObject *args)
{
	PyOPL *o = (PyOPL *)self;

	if (!PyArg_ParseTuple(args, "w*", &o->sh->pybuf)) return NULL;

	int samples = o->sh->pybuf.len / SAMPLE_SIZE / o->sh->channels;
	if (samples > 512) {
		PyErr_SetString(PyExc_ValueError, "buffer too large (max 512 samples)");
		return NULL;
	}
	if (samples < 2) {
		PyErr_SetString(PyExc_ValueError, "buffer too small (min 2 samples)");
		return NULL;
	}

	o->opl->Generate(o->sh, samples);

	PyBuffer_Release(&o->sh->pybuf); // won't use it any more

	Py_RETURN_NONE;
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

PyObject *opl_repr(PyObject *self)
{
	return PyString_FromString("<OPL>");
}

static PyObject *opl_new(PyTypeObject *type, PyObject *args, PyObject *keywds)
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

	PyOPL *o = (PyOPL *)type->tp_alloc(type, 0);
	if (o) {
		o->sh = new SampleHandler(channels);
		o->opl = new DBOPL::Handler();
		o->opl->Init(freq);
	}
	return (PyObject *)o;
}

static PyTypeObject PyOPLType = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"pyopl.opl",    // tp_name
	sizeof(PyOPL),  // tp_basicsize
	0,              // tp_itemsize
	opl_dealloc,    // tp_dealloc
	0,              // tp_print
	0,              // tp_getattr
	0,              // tp_setattr
	0,              // tp_as_async
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
	"OPL emulator", // tp_doc
	0,              // tp_traverse
	0,              // tp_clear
	0,              // tp_richcompare
	0,              // tp_weaklistoffset
	0,              // tp_iter
	0,              // tp_iternext
	opl_methods,    // tp_methods
	0,              // tp_members
	0,              // tp_getset
	0,              // tp_base
	NULL,           // tp_dict
	0,              // tp_descr_get
	0,              // tp_descr_set
	0,              // tp_dictoffset
	0,              // tp_init
	0,              // tp_alloc
	opl_new,        // tp_new
};

static PyMethodDef methods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyoplmodule = {
	PyModuleDef_HEAD_INIT, // m_base
	"pyopl",        // m_name
	NULL,           // m_doc
	-1,             // m_size
	methods,        // m_methods
	NULL,           // m_slots
	NULL,           // m_traverse
	NULL,           // m_clear
	NULL,           // m_free
};

PyMODINIT_FUNC
PyInit_pyopl(void)
{
	if (PyType_Ready(&PyOPLType) < 0)
	{
		return ERROR_INIT;
	}
	PyObject *module;
	module = PyModule_Create(&pyoplmodule);
	if (!module) return ERROR_INIT;

	Py_INCREF(&PyOPLType);
	if (PyModule_AddObject(module, "opl", (PyObject*)&PyOPLType) < 0)
	{
		Py_DECREF(&PyOPLType);
		Py_DECREF(module);
		return ERROR_INIT;
	}
	return module;
}
