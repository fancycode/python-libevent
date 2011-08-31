#!/usr/bin/python -u
#
# Python Bindings for libevent
#
# Copyright (c) 2010-2011 by Joachim Bauch, mail@joachim-bauch.de
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
import sys, os

try:
    from setuptools import setup, Extension
except ImportError:
    from ez_setup import use_setuptools
    use_setuptools()

    from setuptools import setup, Extension

import version

LIBEVENT_ROOT = os.environ.get('LIBEVENT_ROOT')
if LIBEVENT_ROOT is None:
    raise TypeError('Please set the environment variable LIBEVENT_ROOT ' \
        'to the path of your libevent root directory and make sure ' \
        'to pass "--with-pic" to configure when building it')

descr = "Python bindings for libevent"
modules = [
    'libevent',
]
c_files = [
    'src/_libevent.c',
    'src/pybase.c',
    'src/pybuffer.c',
    'src/pybufferevent.c',
    'src/pyevent.c',
    'src/pyhttp.c',
    'src/pylistener.c',
]
include_dirs = [
    os.path.join(LIBEVENT_ROOT, 'include'),
]
library_dirs = [
]
libraries = [
]
extra_link_args = [
]
if os.name == 'posix':
    # enable thread support
    extra_link_args.extend([
        os.path.join(LIBEVENT_ROOT, '.libs', 'libevent.a'),
        os.path.join(LIBEVENT_ROOT, '.libs', 'libevent_pthreads.a'),
    ])
    libraries.append('rt')
    libraries.append('pthread')
elif os.name == 'nt':
    # enable thread support
    extra_link_args.extend([
        os.path.join(LIBEVENT_ROOT,  'libevent.lib'),
    ])    
    libraries.append('ws2_32')
    libraries.append('Advapi32')
    
extens = [
    Extension('_libevent', c_files, libraries=libraries,
        include_dirs=include_dirs, library_dirs=library_dirs,
        extra_link_args=extra_link_args),
]

setup(
    name = "python-libevent",
    version = version.get_git_version(),
    description = descr,
    author = "Joachim Bauch",
    author_email = "mail@joachim-bauch.de",
    url = "http://www.joachim-bauch.de/projects/python-libevent/",
    download_url = "http://pypi.python.org/pypi/python-libevent/",
    license = 'LGPL',
    keywords = "libevent network",
    classifiers = [
        'Development Status :: 5 - Production/Stable',
        'Programming Language :: Python',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)',
        'Operating System :: OS Independent',
    ],
    py_modules = modules,
    ext_modules = extens,
    test_suite = 'tests.suite',
)
