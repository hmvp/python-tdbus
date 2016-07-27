#!/usr/bin/env python
#
# This file is part of python-tdbus. Python-tdbus is free software
# available under the terms of the MIT license. See the file "LICENSE" that
# was provided together with this source file for the licensing terms.
#
# Copyright (c) 2012 the python-tdbus authors. See the file "AUTHORS" for a
# complete list.

# This example shows how to access Avahi on the D-BUS.

from __future__ import print_function, unicode_literals
from __future__ import division, absolute_import

from tdbus import SimpleDBusConnection, DBUS_BUS_SYSTEM, DBusHandler, signal_handler, DBusError

import logging

logging.basicConfig(level=logging.DEBUG)

CONN_AVAHI = 'org.freedesktop.Avahi'
PATH_SERVER = '/'
IFACE_SERVER = 'org.freedesktop.Avahi.Server'

conn = SimpleDBusConnection(DBUS_BUS_SYSTEM)

try:
    result = conn.call_method(PATH_SERVER, 'GetVersionString',
                        interface=IFACE_SERVER, destination=CONN_AVAHI)
except DBusError:
    print('Avahi NOT available.')
    raise

print('Avahi is available at %s' % CONN_AVAHI)
print('Avahi version: %s' % result.get_args()[0])
print()
print('Browsing service types on domain: local')
print('Press CTRL-c to exit')
print()

result = conn.call_method('/', 'ServiceTypeBrowserNew', interface=IFACE_SERVER,
                    destination=CONN_AVAHI, format='iisu', args=(-1, 0, 'local', 0))
browser = result.get_args()[0]
print(browser)

class AvahiHandler(DBusHandler):

    @signal_handler()
    def ItemNew(self, message):
        args = message.get_args()
        print('service %s exists on domain %s' % (args[2], args[3]))

conn.add_handler(AvahiHandler())
conn.dispatch()
