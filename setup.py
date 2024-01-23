from setuptools import Extension, setup

setup(
	ext_modules=[Extension('pyopl', ['pyopl.cpp', 'dbopl.cpp'], depends=['dosbox.h', 'dbopl.h', 'adlib.h'])],
)
