import sys

from setuptools import Extension, setup
from wheel.bdist_wheel import bdist_wheel

is_stable_api_supported = sys.version_info.major >= 3 and sys.version_info.minor >= 11
"""The minimum supported version for stable ABI (limited API) is 3.11, due to use of Buffer."""


class bdist_wheel_abi3(bdist_wheel):
	def get_tag(self):
		python, abi, plat = super().get_tag()

		if python.startswith("cp"):
			# on CPython, wheels are abi3 and compatible back to 3.11
			return "cp311", "abi3", plat

		return python, abi, plat


setup(
	cmdclass={"bdist_wheel": bdist_wheel_abi3} if is_stable_api_supported else {},
	ext_modules=[
		Extension(
			'pyopl',
			['pyopl.cpp', 'dbopl.cpp'],
			define_macros=[("Py_LIMITED_API", "0x030B0000")] if is_stable_api_supported else [],
			depends=['dosbox.h', 'dbopl.h', 'adlib.h'],
			py_limited_api=is_stable_api_supported,
		)
	],
)
