#
# This file is part of python-tdbus. Python-tdbus is free software
# available under the terms of the MIT license. See the file "LICENSE" that
# was provided together with this source file for the licensing terms.
#
# Copyright (c) 2012 the python-tdbus authors. See the file "AUTHORS" for a
# complete list.

import logging
import math
from threading import Thread
import unittest

import gevent

from tdbus import GEventDBusConnection, DBUS_BUS_SESSION, DBusError, \
    SimpleDBusConnection, method, DBusHandler
from tdbus.handler import signal_handler, dbus_object
from .base import BaseTest


logging.basicConfig()


IFACE_EXAMPLE = 'com.example'


class MessageTest(BaseTest):

    def echo(self, signature=None, args=None):
        reply = self.client.call_method('/', 'Echo', IFACE_EXAMPLE, signature, args,
                                        destination=self.server_name, timeout=10)
        return reply.get_args()

    def echo_exception(self, signature=None, args=None):
        try:
            self.client.call_method('/', 'EchoException', IFACE_EXAMPLE, signature, args,
                                    destination=self.server_name, timeout=10)
        except DBusError as e:
            return e
        else:
            raise ValueError("Should have gotten an DbusError")

    def test_arg_byte(self):
        assert self.echo('y', (0,)) == (0,)
        assert self.echo('y', (10,)) == (10,)
        assert self.echo('y', (0xff,)) == (0xff,)

    def test_arg_byte_out_of_range(self):
        with self.assertRaises(DBusError):
            self.echo('y', (-1,))
            self.echo('y', (0x100,))

    def test_arg_int16(self):
        assert self.echo('n', (-0x8000,)) == (-0x8000,)
        assert self.echo('n', (-10,)) == (-10,)
        assert self.echo('n', (-0,)) == (-0,)
        assert self.echo('n', (10,)) == (10,)
        assert self.echo('n', (0x7fff,)) == (0x7fff,)

    def test_arg_int16_out_of_range(self):
        with self.assertRaises(DBusError):
            self.echo('n', (-0x8001,))
            self.echo('n', (0x8000,))

    def test_arg_uint16(self):
        assert self.echo('q', (0,)) == (0,)
        assert self.echo('q', (10,)) == (10,)
        assert self.echo('q', (0xffff,)) == (0xffff,)

    def test_arg_uint16_out_of_range(self):
        with self.assertRaises(DBusError):
            self.echo('q', (-1,))
            self.echo('q', (0x10000,))

    def test_arg_int32(self):
        assert self.echo('i', (-0x80000000,)) == (-0x80000000,)
        assert self.echo('i', (-10,)) == (-10,)
        assert self.echo('i', (0,)) == (0,)
        assert self.echo('i', (10,)) == (10,)
        assert self.echo('i', (0x7fffffff,)) == (0x7fffffff,)

    def test_arg_int32_out_of_range(self):
        with self.assertRaises(DBusError):
            self.echo('i', (-0x80000001,))
            self.echo('i', (0x80000000,))

    def test_arg_uint32(self):
        assert self.echo('u', (0,)) == (0,)
        assert self.echo('u', (10,)) == (10,)
        assert self.echo('u', (0xffffffff,)) == (0xffffffff,)

    def test_arg_uint32_out_of_range(self):
        with self.assertRaises(DBusError):
            self.echo('u', (-1,))
            self.echo('q', (0x100000000,))

    def test_arg_int64(self):
        assert self.echo('x', (-0x8000000000000000,)) == (-0x8000000000000000,)
        assert self.echo('x', (-10,)) == (-10,)
        assert self.echo('x', (0,)) == (0,)
        assert self.echo('x', (10,)) == (10,)
        assert self.echo('x', (0x7fffffffffffffff,)) == (0x7fffffffffffffff,)

    def test_arg_int64_out_of_range(self):
        with self.assertRaises(DBusError):
            self.echo('x', (-0x8000000000000001,))
            self.echo('x', (0x8000000000000000,))

    def test_arg_uint64(self):
        assert self.echo('t', (0,)) == (0,)
        assert self.echo('t', (10,)) == (10,)
        assert self.echo('t', (0xffffffffffffffff,)) == (0xffffffffffffffff,)

    def test_arg_uint64_out_of_range(self):
        with self.assertRaises(DBusError):
            self.echo('t', (-1,))
            self.echo('t', (0x10000000000000000,))

    def test_arg_boolean(self):
        assert self.echo('b', (False,)) == (False,)
        assert self.echo('b', (True,)) == (True,)
        assert self.echo('b', (0,)) == (False,)
        assert self.echo('b', (1,)) == (True,)
        assert self.echo('b', ([],)) == (False,)
        assert self.echo('b', ([1],)) == (True,)
        assert self.echo('b', ({},)) == (False,)
        assert self.echo('b', ({"": ""},)) == (True,)
        assert self.echo('b', ((),)) == (False,)
        assert self.echo('b', ((1),)) == (True,)
        assert self.echo('b', (None,)) == (False,)

    def test_arg_double(self):
        assert self.echo('d', (-1e100,)) == (-1e100,)
        assert self.echo('d', (-1.0,)) == (-1.0,)
        assert self.echo('d', (-1e-100,)) == (-1e-100,)
        assert self.echo('d', (0.0,)) == (0.0,)
        assert self.echo('d', (1e-100,)) == (1e-100,)
        assert self.echo('d', (1.0,)) == (1.0,)
        assert self.echo('d', (1e100,)) == (1e100,)

    def test_arg_double_special(self):
        inf = 1e1000
        assert self.echo('d', (inf,)) == (inf,)
        assert self.echo('d', (-inf,)) == (-inf,)
        assert self.echo('d', (1 / inf,)) == (1 / inf,)  # 0
        assert self.echo('d', (1 / -inf,)) == (1 / -inf,)  # -0
        nan = inf / inf
        assert math.isnan(self.echo('d', (nan,))[0])  # note: nan != nan

    def test_arg_string(self):
        assert self.echo('s', ('',)) == ('',)
        assert self.echo('s', ('foo',)) == ('foo',)

    def test_arg_string_unicode(self):
        assert self.echo('s', (u'foo \u20ac',)) == (u'foo \u20ac',)

    def test_arg_object_path(self):
        assert self.echo('o', ('/foo/bar',)) == ('/foo/bar',)

    def test_arg_object_path_unicode(self):
        assert self.echo('o', (u'/foo/bar',)) == (u'/foo/bar',)

    def test_arg_invalid_object_path(self):
        with self.assertRaises(DBusError):
            self.echo('o', ('foo',))
            self.echo('o', ('foo/bar',))
            self.echo('o', ('/foo/bar/',))
            self.echo('o', ('/foo//bar/',))
            self.echo('o', ('/foo bar/',))

    def test_arg_signature(self):
        assert self.echo('g', ('iii',)) == ('iii',)

    def test_arg_signature_unicode(self):
        assert self.echo('g', (u'iii',)) == (u'iii',)

    def test_arg_invalid_signature(self):
        with self.assertRaises(DBusError):
            self.echo('*', (1,))
            self.echo('(i', (1,))
            self.echo('i' * 256, (1,) * 256)

        def nested_tuple(d, v):
            if d == 0:
                return (v,)
            return (nested_tuple(d - 1, v),)

        with self.assertRaises(DBusError):
            self.echo('(' * 33 + 'i' + ')' * 33,
                          nested_tuple(33, 1))
            self.echo('a' * 33 + 'i', nested_tuple(33, 1))

    def test_arg_variant(self):
        assert self.echo('v', (('i', 10),)) == (('i', 10),)
        assert self.echo('v', (('ai', [1, 2, 3]),)) == (('ai', [1, 2, 3]),)

    def test_arg_variant_unicode(self):
        assert self.echo('v', ((u'i', 10),)) == ((u'i', 10),)
        assert self.echo('v', ((u'ai', [1, 2, 3]),)) == ((u'ai', [1, 2, 3]),)


    def test_arg_invalid_variant(self):
        with self.assertRaises(DBusError):
            self.echo('v', (('ii', (1, 2)),))

    def test_arg_multi(self):
        assert self.echo('ii', (1, 2)) == (1, 2)
        assert self.echo('iii', (1, 2, 3)) == (1, 2, 3)

    def test_arg_too_few(self):
        with self.assertRaises(DBusError):
            self.echo('ii', (1,))

    def test_arg_too_many(self):
        with self.assertRaises(DBusError):
            self.echo('ii', (1, 2, 3))

    def test_arg_struct(self):
        assert self.echo('(i)', ((1,),)) == ((1,),)
        assert self.echo('(ii)', ((1, 2),)) == ((1, 2),)
        assert self.echo('(iii)', ((1, 2, 3),)) == ((1, 2, 3),)
        assert self.echo('(((((i)))))', ((((((1,),),),),),)) == \
                    ((((((1,),),),),),)

    def test_arg_invalid_struct(self):
        with self.assertRaises(DBusError):
            self.echo('(i', ((10,),))
            self.echo('(i}', ((10,),))

    def test_arg_array(self):
        assert self.echo('ai', ([1],)) == ([1],)
        assert self.echo('ai', ([1, 2],)) == ([1, 2],)
        assert self.echo('ai', ([1, 2, 3],)) == ([1, 2, 3],)
        assert self.echo('a(ii)', ([(1, 2), (3, 4)],)) == ([(1, 2), (3, 4)],)
        assert self.echo('av', ([('i', 10), ('s', 'foo')],)) == \
                    ([('i', 10), ('s', 'foo')],)

    def test_arg_dict(self):
        assert self.echo('a{ss}', ({'foo': 'bar'},)) == ({'foo': 'bar'},)
        assert self.echo('a{ss}', ({'foo': 'bar', 'baz': 'qux'},)) == \
                    ({'foo': 'bar', 'baz': 'qux'},)
        assert self.echo('a{si}', ({'foo': 10},)) == ({'foo': 10},)
        assert self.echo('a{ii}', ({1: 10},)) == ({1: 10},)

    def test_arg_byte_array(self):
        assert self.echo('ay', (b'foo',)) == (b'foo',)

    def test_arg_byte_array_illegal_type(self):
        with self.assertRaises(DBusError):
            self.echo('ay', ([1, 2, 3],))

    def test_exceptions(self):
        error = self.echo_exception('ss', ['SpecialException', 'message'])
        print(error, type(error))
        self.assertEquals(error.__class__.__name__, 'SpecialException')
        self.assertEquals(error.type, 'SpecialException')
        self.assertEquals(error.args[0], 'message')
        assert isinstance(error, DBusError)

        error = self.echo_exception('s', ['Missing second argument, which raises an ValueError'])
        self.assertEquals(error.__class__.__name__, 'ValueError')
        self.assertEquals(error.type, 'ValueError')
        self.assertTrue(error.args[0] in  ('need more than 1 value to unpack', 'not enough values to unpack (expected 2, got 1)'))
        assert isinstance(error, DBusError)


class EchoHandlerBase(object):
    def __init__(self, signal_handler=None):
        super(EchoHandlerBase, self).__init__()
        self.signal_handler = signal_handler

    @method(interface=IFACE_EXAMPLE, member="Echo")
    def echo_method(self, message):
        self.set_response(message.get_signature(), message.get_args())

    @method(interface=IFACE_EXAMPLE, member="EchoException")
    def echo_exception(self, message):
        name, message = message.get_args()

        raise type(str(name), (Exception,), {})(message)

    @signal_handler(interface=IFACE_EXAMPLE, member="Echo")
    def echo_signal(self, message):
        if self.signal_handler:
            self.signal_handler(message)

    @method(interface=IFACE_EXAMPLE, member="Stop")
    def stop(self, _):
        self.connection.stop()


class EchoHandlerInherit(EchoHandlerBase, DBusHandler):
    pass


@dbus_object
class EchoHandlerDecorator(EchoHandlerBase):
    pass


class TestMessageASimple(unittest.TestCase, MessageTest):

    @classmethod
    def dbus_server(cls, conn):
        conn.dispatch()

    @classmethod
    def setUpClass(cls):
        super(TestMessageASimple, cls).setUpClass()
        handler = EchoHandlerInherit()
        conn = SimpleDBusConnection(DBUS_BUS_SESSION)
        conn.add_handler(handler)
        cls.server_name = conn.get_unique_name()
        cls.server = Thread(target=cls.dbus_server, args=(conn,))
        cls.server.start()
        cls.client = SimpleDBusConnection(DBUS_BUS_SESSION)

    @classmethod
    def tearDownClass(cls):
        cls.client.call_method('/', 'Stop', IFACE_EXAMPLE, destination=cls.server_name)
        cls.server.join()
        super(TestMessageASimple, cls).tearDownClass()


class TestMessageGEvent(unittest.TestCase, MessageTest):

    @classmethod
    def setUpClass(cls):
        super(TestMessageGEvent, cls).setUpClass()
        handler = EchoHandlerInherit()
        conn = GEventDBusConnection(DBUS_BUS_SESSION)
        conn.add_handler(handler)
        cls.server_name = conn.get_unique_name()
        cls.client = GEventDBusConnection(DBUS_BUS_SESSION)


class TestMessageDecorated(unittest.TestCase, MessageTest):

    @classmethod
    def setUpClass(cls):
        super(TestMessageDecorated, cls).setUpClass()
        handler = EchoHandlerDecorator()
        conn = GEventDBusConnection(DBUS_BUS_SESSION)
        conn.add_handler(handler)
        cls.server_name = conn.get_unique_name()
        cls.client = GEventDBusConnection(DBUS_BUS_SESSION)


class TestMessageName(unittest.TestCase, MessageTest):

    @classmethod
    def setUpClass(cls):
        super(TestMessageName, cls).setUpClass()
        cls.server_name = "org.tdbus.Test"
        handler = EchoHandlerInherit()
        conn = GEventDBusConnection(DBUS_BUS_SESSION)
        conn.register_name(cls.server_name)
        conn.add_handler(handler)
        cls.client = GEventDBusConnection(DBUS_BUS_SESSION)


class TestMessageSignal(unittest.TestCase, MessageTest):

    last_message = None

    @classmethod
    def setUpClass(cls):
        super(TestMessageSignal, cls).setUpClass()
        cls.server_name = "org.tdbus.Test"

        def signal_handler_f(message):
            logging.getLogger('tdbus').info(message)
            cls.last_message = message

        handler = EchoHandlerInherit(signal_handler_f)
        conn = GEventDBusConnection(DBUS_BUS_SESSION)
        conn.register_name(cls.server_name)
        conn.add_handler(handler)
        cls.client = GEventDBusConnection(DBUS_BUS_SESSION)

    def echo(self, signature=None, args=None):
        self.client.send_signal('/', 'Echo', IFACE_EXAMPLE, signature, args,
                                       destination=self.server_name)

        # Wait a sec to server can process the message
        gevent.sleep(0.1)
        return self.last_message.get_args()


class TestMessageSignalMatched(unittest.TestCase, MessageTest):

    last_message = None

    @classmethod
    def setUpClass(cls):
        super(TestMessageSignalMatched, cls).setUpClass()
        cls.server_name = "org.tdbus.Test"

        def signal_handler_f(message):
            logging.getLogger('tdbus').info(message)
            cls.last_message = message

        handler = EchoHandlerInherit(signal_handler_f)
        conn = GEventDBusConnection(DBUS_BUS_SESSION)
        conn.add_handler(handler)
        conn.subscribe_to_signals()
        cls.client = GEventDBusConnection(DBUS_BUS_SESSION)

    def echo(self, signature=None, args=None):
        self.client.send_signal('/', 'Echo', IFACE_EXAMPLE, signature, args)

        # Wait a sec to server can process the message
        gevent.sleep(0.01)
        return self.last_message.get_args()
