import gc
import os
import unittest
import weakref

import libevent

class TestBufferEvent(unittest.TestCase):

    def createBase(self):
        return libevent.Base()

    def createBufferEvent(self, *args):
        return libevent.BufferEvent(*args)
    
    def test_bucket(self):
        base = self.createBase()
        buf = self.createBufferEvent(base)
        self.failUnlessEqual(buf.bucket, None)

    def test_recursive_callback(self):
        class A:
            def __init__(self, buf):
                self.buf = buf
                self.buf.set_callbacks(self._readable, None, None)
            
            def _readable(self):
                pass
        
        base = self.createBase()
        buf = self.createBufferEvent(base)
        r1 = weakref.ref(buf)
        a = A(buf)
        r2 = weakref.ref(a)
        del buf
        del a
        # the class and the bev still exist due to circular references
        self.failIfEqual(r1(), None)
        self.failIfEqual(r2(), None)
        # ...but after a gc run, they are gone
        gc.collect()
        self.failUnlessEqual(r1(), None)
        self.failUnlessEqual(r2(), None)

def suite():
    suite = unittest.TestSuite()

    test_cases = [
        TestBufferEvent,
    ]

    for tc in test_cases:
        suite.addTest(unittest.makeSuite(tc))

    return suite

if __name__ == '__main__':
    unittest.main(defaultTest='suite')
