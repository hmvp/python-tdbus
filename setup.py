#!/usr/bin/env python
# This file is part of python-tdbus. Python-tdbus is free software
# available under the terms of the MIT license. See the file "LICENSE" that
# was provided together with this source file for the licensing terms.
#
# Copyright (c) 2012 the python-tdbus authors. See the file "AUTHORS" for a
# complete list.

import subprocess
from setuptools import setup, Extension


version_info = {
    'name': 'python-tdbus',
    'version': '0.11',
    'description': 'A Python interface to D-BUS',
    'author': 'Hmvp, Geert Jansen',
    'author_email': 'github@hmvp.nl',
    'url': 'https://github.com/hmvp/python-tdbus',
    'license': 'MIT',
    'classifiers': [
        'Development Status :: 4 - Beta',
        'License :: OSI Approved :: MIT License',
        'Operating System :: POSIX',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
    ],
}


def pkgconfig(*args):
    """Run pkg-config."""
    output = subprocess.check_output(['pkg-config'] + list(args), universal_newlines=True)
    return output.strip().split()


setup(
    package_dir={ '': 'lib' },
    packages=['tdbus'],
    ext_modules=[Extension('tdbus._tdbus', ['lib/tdbus/_tdbus.c'],
              extra_compile_args=pkgconfig('--cflags', 'dbus-1'),
              extra_link_args=pkgconfig('--libs', 'dbus-1'))],
    **version_info
)
