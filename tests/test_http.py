import os
import unittest
import threading

import libevent

class TestHttpServer(unittest.TestCase):

    def createBase(self):
        return libevent.Base()
        
    def createHttpServer(self, *args):
        return libevent.HttpServer(*args)
        
    def test_bound_socket(self):
        socket = libevent.BoundSocket()
    
    def execute_request(self, server, request, userdata):
        print 'Request:', (server, request, userdata)
        #request.send_error(404)
        request.send_reply(200, 'OK', 'lala\n')
        #request.send_reply_start(200, 'OK')
        #request.send_reply_chunk('lala')
        #request.send_reply_chunk('lala123')
        #request.send_reply_end()
    
    def test_server(self):
        base = self.createBase()
        server = self.createHttpServer(base)
        sock1 = server.bind('localhost', 8080)
        self.failUnless(isinstance(sock1, libevent.BoundSocket), sock1)
        sock2 = server.accept(sock1)
        self.failUnless(isinstance(sock2, libevent.BoundSocket), sock2)
        server.set_callback('/', self.execute_request)
        base.loopexit(0.1)
        base.loop()
    
def suite():
    suite = unittest.TestSuite()

    test_cases = [
        TestHttpServer,
    ]

    for tc in test_cases:
        suite.addTest(unittest.makeSuite(tc))

    return suite

if __name__ == '__main__':
    unittest.main(defaultTest='suite')
