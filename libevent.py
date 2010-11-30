from _libevent import *
import weakref

class Timer(Event):
    """Simplified class for timers."""
    
    __slots__ = ('_callback')
    
    def __init__(self, base, callback, userdata=None):
        if not callable(callback):
            raise TypeError("the callback must be callable")
        
        def _fire(evt, fd, what, userdata, selfref=weakref.ref(self)):
            """Special internal class to prevent circular references."""
            self = selfref()
            if self is not None:
                self._callback(self, userdata)
        
        super(Timer, self).__init__(base, -1, 0, _fire, userdata)
        self._callback = callback

class Signal(Event):
    """Simplified class for signals."""
    
    __slots__ = ('_callback')
    
    def __init__(self, base, signum, callback, userdata=None):
        if not callable(callback):
            raise TypeError("the callback must be callable")
        
        def _fire(evt, fd, what, userdata, selfref=weakref.ref(self)):
            """Special internal class to prevent circular references."""
            self = selfref()
            if self is not None:
                self._callback(self, fd, userdata)
        
        super(Signal, self).__init__(base, signum, EV_SIGNAL|EV_PERSIST, _fire, userdata)
        self._callback = callback
