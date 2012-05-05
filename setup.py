import os
from distutils.core import setup
from distutils.extension import Extension

# Borrowed from PyPI example
def read(fname):
	return open(os.path.join(os.path.dirname(__file__), fname)).read()

setup(name='PyOPL',
	version='1.2',
	description='OPL2/3 Adlib emulation',
	author='Adam Nielsen',
	author_email='malvineous@shikadi.net',
	url='http://www.github.com/Malvineous/pyopl',
	license='GPL',
	keywords='Adlib FM OPL OPL2 OPL3 YM3812 YMF262',
	long_description=read('README'),
	classifiers=[
		'Development Status :: 5 - Production/Stable',
		'Intended Audience :: End Users/Desktop',
		'License :: OSI Approved :: GNU General Public License (GPL)',
		'Operating System :: OS Independent',
		'Programming Language :: C++',
		'Topic :: Multimedia :: Sound/Audio :: Sound Synthesis',
	],
	platforms=['Any'],
	ext_modules=[Extension('pyopl', ['pyopl.cpp', 'dbopl.cpp'])],
)
