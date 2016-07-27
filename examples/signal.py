#!/usr/bin/env python
#
# This file is part of python-tdbus. Python-tdbus is free software
# available under the terms of the MIT license. See the file "LICENSE" that
# was provided together with this source file for the licensing terms.
#
# Copyright (c) 2012 the python-tdbus authors. See the file "AUTHORS" for a
# complete list.


# This example shows how to listen for signals. Here we listen for any signal
# but signal_handler() also accepts keyword arguments to only listen for
# specific signals.

from __future__ import print_function, unicode_literals
from __future__ import division, absolute_import

from tdbus import DBusHandler, signal_handler, SimpleDBusConnection, DBUS_BUS_SESSION

class SignalHandler(DBusHandler):

    @signal_handler()
    def Signal(self, message):
         print('signal received: %s, args = %s' % (message.get_member(), repr(message.get_args())))


conn = SimpleDBusConnection(DBUS_BUS_SESSION)
handler = SignalHandler()

conn.add_handler(handler)

# the next method needs to be called explicitly after all add_handler calls
conn.subscribe_to_signals()


print('Listening for signals. Press CTRL-c to quit.')
print('In another terminal, issue:')
print()
print('  $ dbus-send --session --type=signal --dest={} /com/example/TDBus com.example.Hello.Signal'.format(conn.get_unique_name()))
print()
print('or (only when subscribe_to_signals() is used)')
print('  $ dbus-send --session --type=signal /com/example/TDBus com.example.Hello.Signal')
print()
print('Press CTRL-c to exit.')
print()

conn.dispatch()
