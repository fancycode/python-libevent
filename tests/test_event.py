import os
import unittest
import threading
try:
    import signal
except ImportError:
    import warnings
    warnings.warn('No signals available on this platform, signal tests will be skipped')
    signal = None

import libevent

class TestEventBase(unittest.TestCase):

    def createBase(self, *args):
        return libevent.Base(*args)
        
    def createConfig(self):
        return libevent.Config()
        
    def createTimer(self, *args):
        return libevent.Timer(*args)
    
    def createSignal(self, *args):
        return libevent.Signal(*args)
    
    def fire_timer(self, _, evt):
        evt.set()
    
    def fire_signal(self, _, signum, evt):
        evt.set()
    
    def test_events(self):
        methods = libevent.METHODS
        self.failUnless('select' in methods, methods)
        self.failUnless('poll' in methods, methods)
    
    def test_timer(self):
        evt = threading.Event()
        base = self.createBase()
        t = self.createTimer(base, self.fire_timer, evt)
        base.loopexit(0.1)
        base.loop()
        self.failIf(evt.isSet(), 'timer should not have been fired')

    def test_timer_noref(self):
        # a timer object without a reference doesn't fire
        evt = threading.Event()
        base = self.createBase()
        t = self.createTimer(base, self.fire_timer, evt)
        t.add(0.1)
        del t
        base.loopexit(0.2)
        base.loop()
        self.failIf(evt.isSet(), 'timer should not have been fired')

    def test_timer_add(self):
        evt = threading.Event()
        base = self.createBase()
        t = self.createTimer(base, self.fire_timer, evt)
        t.add(0.1)
        base.loopexit(0.2)
        base.loop()
        self.failUnless(evt.isSet(), 'timer did not fire')

    def test_timer_delete(self):
        evt = threading.Event()
        base = self.createBase()
        t = self.createTimer(base, self.fire_timer, evt)
        t.add(0.1)
        t.delete()
        base.loopexit(0.2)
        base.loop()
        self.failIf(evt.isSet(), 'timer should not have been fired')

    if signal is not None:
        
        def test_signal(self):
            evt = threading.Event()
            base = self.createBase()
            t = self.createSignal(base, signal.SIGUSR1, self.fire_signal, evt)
            t.add(0.1)
            base.loopexit(0.2)
            os.kill(os.getpid(), signal.SIGUSR1)
            base.loop()
            self.failUnless(evt.isSet(), 'signal did not fire')

    def test_cfg_avoid_method(self):
        self.failUnless(len(libevent.METHODS) > 1)
        for method in libevent.METHODS:
            all_methods = set(libevent.METHODS)
            all_methods.remove(method)
            cfg = self.createConfig()
            for m in all_methods:
                cfg.avoid_method(m)
            base = self.createBase(cfg)
            self.failUnlessEqual(method, base.method)
    
def suite():
    suite = unittest.TestSuite()

    test_cases = [
        TestEventBase,
    ]

    for tc in test_cases:
        suite.addTest(unittest.makeSuite(tc))

    return suite

if __name__ == '__main__':
    unittest.main(defaultTest='suite')
