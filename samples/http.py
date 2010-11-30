import errno
import libevent
import socket

def readable(bev, userdata):
    bev.disable(libevent.EV_READ)
    print 'Readable:', (bev, userdata)
    data = bev.read(8192)
    print data
    bev.enable(libevent.EV_WRITE)

def writable(bev, userdata):
    bev.disable(libevent.EV_WRITE)
    print 'Writable:', (bev, userdata)
    bev.write('lala')
    bev.enable(libevent.EV_READ)

def event_received(bev, what, userdata):
    print 'Event:', (bev, what, userdata)

def main():
    # the classic hello world example...
    
    # we need an event base that will hold the events
    base = libevent.Base()
    print 'Initialized base with %r backend' % (base.method)

    # create listening socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('localhost', 1234))
    s.listen(1)
    print 'Running on %s:%d' % s.getsockname()
    
    # wait for client connection
    conn, addr = s.accept()
    conn.setblocking(False)
    print 'Connected from %s:%d' % addr
    
    bev = libevent.BufferEvent(base, conn.fileno())
    bev.set_callbacks(readable, writable, event_received)
    bev.enable(libevent.EV_READ)
    bev.set_timeouts(5, 5)
    
    # run event loop until termination
    base.loop()

if __name__ == '__main__':
    main()
