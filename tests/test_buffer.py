import os
import unittest

import libevent

class TestBuffer(unittest.TestCase):

    def createBuffer(self):
        return libevent.Buffer()
    
    def test_add_remove(self):
        buf = self.createBuffer()
        self.failUnlessEqual(len(buf), 0)
        self.failIf(bool(buf))
        buf.add('12')
        self.failUnless(bool(buf))
        self.failUnlessEqual(len(buf), 2)
        buf.add('34')
        self.failUnlessEqual(len(buf), 4)
        self.failUnless('12' in buf)
        self.failUnless('23' in buf)
        self.failUnless('34' in buf)
        self.failUnless('1234' in buf)
        self.failUnlessEqual(buf.remove(4), '1234')
        self.failUnlessEqual(len(buf), 0)
        self.failUnlessRaises(TypeError, buf.remove, -2)

    def test_add_copyout(self):
        buf = self.createBuffer()
        buf.add('12')
        buf.add('34')
        self.failUnlessEqual(buf.copyout(2), '12')
        self.failUnlessEqual(buf.copyout(4), '1234')
        self.failUnlessRaises(TypeError, buf.copyout, -2)

    def test_add_buffer(self):
        buf1 = self.createBuffer()
        buf1.add('12')
        buf1.add('34')
        buf2 = self.createBuffer()
        buf2.add(buf1)
        self.failUnlessEqual(buf1.remove(1), '')
        self.failUnlessEqual(buf2.remove(4), '1234')

    def test_prepend_remove(self):
        buf = self.createBuffer()
        buf.prepend('12')
        buf.prepend('34')
        self.failUnlessEqual(buf.remove(4), '3412')

    def test_prepend_buffer(self):
        buf1 = self.createBuffer()
        buf1.add('12')
        buf2 = self.createBuffer()
        buf2.add('34')
        buf3 = self.createBuffer()
        buf3.prepend(buf1)
        buf3.prepend(buf2)
        self.failUnlessEqual(buf3.remove(4), '3412')

    def test_readln_noline(self):
        buf = self.createBuffer()
        buf.add('foo')
        line = buf.readln()
        self.failUnlessEqual(line, '')
        buf.add('\n')
        line = buf.readln()
        self.failUnlessEqual(line, 'foo')
        self.failIf(bool(buf))

def suite():
    suite = unittest.TestSuite()

    test_cases = [
        TestBuffer,
    ]

    for tc in test_cases:
        suite.addTest(unittest.makeSuite(tc))

    return suite

if __name__ == '__main__':
    unittest.main(defaultTest='suite')
