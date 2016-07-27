.. image:: https://travis-ci.org/hmvp/python-tdbus.svg?branch=master
    :target: https://travis-ci.org/hmvp/python-tdbus

Overview
========

Python-tdbus is a simple ("trivial") python interface for D-BUS. It builds
directly on top of libdbus and has no other dependencies. Some benefits of
python-tdbus with respect to the standard dbus-python [1]_ Python bindings:

 * The code is extremely simple. Python-tdbus is < 2.000 lines of code (C and
   Python), while dbus-python contains > 15.000 lines of code.
 * Event loop integration is not required for sending and receiving signals (if
   you can afford to block).
 * Includes `gevent' [2]_ event loop integration.
 * Event loop integration can be achieved in Python code rather than in C.
 * Uses native Python types for method and signal arguments, driven by a simple
   format string.
 * Provides a more "correct" object model (IMHO) where there's separate
   Dispatcher and Connection objects, instead of putting dispatching
   functionality into the connection object.

Building and Installing
=======================

 $ python setup.py build
 # python setup.py install

 or using pip

 $ pip install python-tdbus


Requirements
============

Python-tdbus should work with Python 2.7 and later.

Comments and Suggestion
=======================

Feel free to add an issue on the Github site for python-tdbus:

  https://github.com/hmvp/python-tdbus

Documentation
=============

See the examples/ directory and "pydoc tdbus".

References
==========

.. [1] http://cgit.freedesktop.org/dbus/dbus-python/
.. [2] http://www.gevent.org/
