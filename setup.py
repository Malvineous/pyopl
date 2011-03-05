from distutils.core import setup
from distutils.extension import Extension
setup(name='pyopl',
	version='1.0',
	ext_modules=[Extension('pyopl', ['pyopl.cpp', 'dbopl.cpp'])],
)
