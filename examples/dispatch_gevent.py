#!/usr/bin/env python
#
# This file is part of python-tdbus. Python-tdbus is free software
# available under the terms of the MIT license. See the file "LICENSE" that
# was provided together with this source file for the licensing terms.
#
# Copyright (c) 2012 the python-tdbus authors. See the file "AUTHORS" for a
# complete list.


# This example shows how to listen for signals. Here we listen for any signal named "Hello" on interface
# com.example.Hello but signal_handler() also accepts keyword arguments to only listen for
# specific signals.

from __future__ import print_function, unicode_literals
from __future__ import division, absolute_import

import sys
import tdbus

import gevent

if not hasattr(tdbus, 'GEventDBusConnection'):
    print('gevent is not available on this system')
    sys.exit(1)

from tdbus import GEventDBusConnection, DBUS_BUS_SESSION, signal_handler, DBusHandler, method


class GEventHandler(DBusHandler):

    @signal_handler(interface="com.example.Hello")
    def Hello(self, message):
        print('signal received: %s, args = %s' % (message.get_member(), repr(message.get_args())))

    @method(interface="com.example.Hello")
    def HelloMethod(self, message):
        print('signal received: %s, args = %s' % (message.get_member(), repr(message.get_args())))

    @method(interface="org.freedesktop.DBus.Introspectable")
    def Introspect(self, message):
        """Return DBus introspection data for debugging

        @see: http://dbus.freedesktop.org/doc/dbus-specification.html#introspection-format
        """

        xml = """<?xml version="1.0" encoding="UTF-8"?>
<!-- /usr/share/dbus-1/interfaces/com.mastervolt.Iris.ShowDevice.xml -->
<node name="/com/mastervolt/Iris">
        <interface name="com.mastervolt.Iris">
                <method name="HelloMethod">
                        <arg type="s" name="somestring" direction="in" />
                        <arg type="i" name="someint" direction="in" />
                        <annotation name="org.freedesktop.DBus.Method.NoReply" value="true"/>
                </method>
                 <signal name="Hello">
                        <arg type="as" name="arrayofstrings" direction="in" />
                </signal>
                <!-- Add more methods/signals if you want -->
        </interface>
</node>"""

        self.set_response("s", [xml])

conn = GEventDBusConnection(DBUS_BUS_SESSION)
handler = GEventHandler()
conn.add_handler(handler)

print('Listening for signals, with gevent dispatcher.')
print('In another terminal, issue:')
print()
print('  $ dbus-send --session --type=signal --dest={} /com/example/TDBus com.example.Hello.Hello'.format(conn.get_unique_name()))
print('  $ dbus-send --session --print-reply --type=method_call --dest={} /com/example/TDBus com.example.Hello.HelloMethod'.format(conn.get_unique_name()))
print()
print('Press CTRL-c to exit.')
print()


from gevent.hub import get_hub
try:
    get_hub().switch()
except KeyboardInterrupt:
    pass

